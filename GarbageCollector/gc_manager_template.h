#pragma once

#include "gc_interface.h"  // IGarbageCollector interface
#include "gc_strategy.h"
#include "mark_sweep_strategy.h"
#include "generational_gc_strategy.h"  // For template specialization
#include "gc_box.h"
#include "gc_allocator.h"
#include <cstddef>

/**
 * @file gc_manager_template.h
 * @brief Template-based GC manager with compile-time strategy selection
 *
 * This provides a zero-cost abstraction alternative to the runtime
 * strategy pattern in gc_manager.h. When the strategy is known at
 * compile time, this allows:
 * - No virtual function overhead
 * - Better inlining and optimization
 * - Smaller code size
 * - Strategy-specific optimizations
 *
 * Usage:
 *   // Default mark-sweep strategy
 *   gc_manager_t<> gc;
 *
 *   // Explicit strategy selection
 *   gc_manager_t<MarkSweepStrategy> gc_ms;
 *   gc_manager_t<RCStrategy> gc_rc;  // Future reference counting
 *
 *   // Use like regular gc_manager
 *   auto obj = gc.allocate<ConsCell>(car, cdr);
 */

/**
 * @brief Template-based GC manager with pluggable allocator
 *
 * @tparam Strategy The GC strategy to use (must satisfy GCStrategyC concept)
 * @tparam Allocator The memory allocator (e.g., MallocAllocator, PoolAllocator)
 *
 * This is a zero-cost abstraction that provides the same interface as
 * gc_manager but with compile-time strategy and allocator selection.
 */
template<typename Strategy = MarkSweepStrategy, typename Allocator = MallocAllocator>
class gc_manager_t : public IGarbageCollector {
public:
    gc_manager_t()
        : IGarbageCollector(nullptr)  // Will be set after m_alloc is initialized
        , m_gc_threshold(1024 * 1024)
        , m_alloc() {
        // Now update Base class allocator pointer to our allocator
        // (can't pass &m_allocator to Base constructor - not initialized yet!)
        m_allocator = &m_alloc;

        // Set back-reference if strategy needs it (now type-safe!)
        if constexpr (requires { m_strategy.set_gc_manager(static_cast<IGarbageCollector*>(this)); }) {
            m_strategy.set_gc_manager(static_cast<IGarbageCollector*>(this));
        }
    }

    explicit gc_manager_t(Allocator alloc, size_t gc_threshold = 1024 * 1024)
        : IGarbageCollector(nullptr)  // Will be set after m_alloc is initialized
        , m_gc_threshold(gc_threshold)
        , m_alloc(std::move(alloc)) {
        // Now update Base class allocator pointer to our moved allocator
        m_allocator = &m_alloc;

        if constexpr (requires { m_strategy.set_gc_manager(static_cast<IGarbageCollector*>(this)); }) {
            m_strategy.set_gc_manager(static_cast<IGarbageCollector*>(this));
        }
    }

    ~gc_manager_t() = default;

    /**
     * @brief Allocate a new Object and register it with the GC
     *
     * @tparam T The type of Object to allocate (must derive from gc_object)
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pointer to the newly allocated Object
     *
     * Note: This uses the template's allocator (NOT default_allocator!)
     * to enable pluggable allocation strategies (malloc, pool, arena, etc.)
     */
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        // Allocate with gc_box wrapper using template's allocator
        T* obj = gc_box::allocate<T>(m_alloc, std::forward<Args>(args)...);

        // Register with GC (triggers collection if needed)
        on_allocate(static_cast<gc_object*>(obj));

        return obj;
    }

    // ========================================================================
    // IGarbageCollector interface implementation
    // ========================================================================

    /**
     * @brief Add a GC root (slot-based, implements memory::IRootManager)
     */
    void add_root(void** root_slot) override {
        m_strategy.register_persistent_root(reinterpret_cast<gc_object**>(root_slot));
    }

    /**
     * @brief Remove a GC root (slot-based, implements memory::IRootManager)
     */
    void remove_root(void** root_slot) override {
        m_strategy.unregister_persistent_root(reinterpret_cast<gc_object**>(root_slot));
    }

    /**
     * @brief Manually trigger garbage collection
     */
    void collect() override {
        m_strategy.collect();
    }

    /**
     * @brief Mark an Object as reachable during the mark phase
     */
    void mark_object(gc_object* obj) override {
        m_strategy.mark_object(obj);
    }

    /**
     * @brief Get the number of objects currently managed by the GC
     */
    size_t object_count() const override {
        return m_strategy.object_count();
    }

    /**
     * @brief Get the number of bytes currently allocated
     */
    size_t bytes_allocated() const override {
        return m_strategy.bytes_allocated();
    }

    /**
     * @brief Get the current GC threshold
     */
    size_t gc_threshold() const {
        return m_gc_threshold;
    }

    /**
     * @brief Set the GC threshold
     */
    void set_gc_threshold(size_t threshold) {
        m_gc_threshold = threshold;
    }

    /**
     * @brief Get garbage collection statistics
     */
    struct gc_stats {
        size_t collections_run;
        size_t objects_collected;
        size_t bytes_freed;
    };

    gc_stats get_stats() const {
        GCStats strategy_stats = m_strategy.get_stats();
        gc_stats stats;
        stats.collections_run = strategy_stats.collections_run;
        stats.objects_collected = strategy_stats.objects_collected;
        stats.bytes_freed = strategy_stats.bytes_reclaimed;
        return stats;
    }

    /**
     * @brief Get direct access to the strategy
     *
     * Allows strategy-specific operations without virtual overhead
     */
    Strategy& strategy() { return m_strategy; }
    const Strategy& strategy() const { return m_strategy; }

    /**
     * @brief Reclaim a single Object (called by strategy during sweep)
     *
     * This is the only deallocation path - no direct free/delete.
     * Uses the template's allocator (NOT default_allocator!)
     */
    void reclaim(gc_object* obj) override {
        gc_box::deallocate(m_alloc, obj);
    }

    /**
     * @brief Write barrier (implements memory::IWriteBarrier)
     *
     * Mark-sweep GC doesn't need write barriers since it traces the entire heap.
     * Generational GC overrides this to track old→young pointers.
     */
    void write_barrier(void* owner, void* old_value, void* new_value) override {
        // No-op for mark-sweep - entire heap is traced
        (void)owner;
        (void)old_value;
        (void)new_value;
    }

    /**
     * @brief Access underlying allocator (for std container adapters)
     */
    Allocator& allocator() {
        return m_alloc;
    }

    const Allocator& allocator() const {
        return m_alloc;
    }

protected:
    // ========================================================================
    // IGarbageCollector protected interface
    // ========================================================================

    /**
     * @brief Allocate raw memory using the template's allocator
     */
    void* allocate_impl(size_t size, size_t align) override {
        return m_alloc.allocate(size, align);
    }

    /**
     * @brief Notify strategy of newly allocated Object
     */
    void on_allocate(gc_object* obj) override {
        m_strategy.on_allocate(obj);

        // Trigger GC if we've exceeded the threshold
        if (m_strategy.bytes_allocated() > m_gc_threshold) {
            collect();
        }
    }

private:
    Strategy m_strategy;         // Strategy embedded directly (zero-cost abstraction)
    size_t m_gc_threshold;      // Trigger GC when this is exceeded
    Allocator m_alloc;          // Pluggable memory allocator
};

// ============================================================================
// Specialization for GenerationalGCStrategy - use nursery allocation
// ============================================================================

/**
 * @brief Specialized gc_manager for generational GC with nursery allocation
 *
 * This specialization overrides allocate() to route through the young generation
 * nursery allocator, providing the full performance benefits of generational GC:
 * - 5-10x faster allocation (bump pointer vs malloc)
 * - 90% faster minor GC (only young gen scanned)
 * - Automatic promotion of long-lived objects to old generation
 *
 * All other functionality is identical to the Base template.
 */
template<typename Allocator>
class gc_manager_t<GenerationalGCStrategy, Allocator> : public IGarbageCollector {
public:
    gc_manager_t()
        : IGarbageCollector(nullptr)
        , m_gc_threshold(1024 * 1024)
        , m_alloc()
        , m_strategy(512 * 1024, &m_alloc) {  // Pass allocator to strategy!
        m_allocator = &m_alloc;
        if constexpr (requires { m_strategy.set_gc_manager(static_cast<IGarbageCollector*>(this)); }) {
            m_strategy.set_gc_manager(static_cast<IGarbageCollector*>(this));
        }
    }

    explicit gc_manager_t(Allocator alloc, size_t gc_threshold = 1024 * 1024)
        : IGarbageCollector(nullptr)
        , m_gc_threshold(gc_threshold)
        , m_alloc(std::move(alloc))
        , m_strategy(512 * 1024, &m_alloc) {  // Pass allocator to strategy!
        m_allocator = &m_alloc;
        if constexpr (requires { m_strategy.set_gc_manager(static_cast<IGarbageCollector*>(this)); }) {
            m_strategy.set_gc_manager(static_cast<IGarbageCollector*>(this));
        }
    }

    ~gc_manager_t() = default;

    /**
     * @brief Allocate through young generation nursery
     *
     * SPECIALIZED FOR GENERATIONAL GC: Routes through nursery allocator instead
     * of going directly to malloc. This provides the full performance benefits
     * of generational collection.
     */
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        return m_strategy.allocate_young<T>(std::forward<Args>(args)...);
    }

    // ========================================================================
    // All other methods identical to Base template - delegate to strategy
    // ========================================================================

    void add_root(void** root_slot) override {
        m_strategy.register_persistent_root(reinterpret_cast<gc_object**>(root_slot));
    }

    void remove_root(void** root_slot) override {
        m_strategy.unregister_persistent_root(reinterpret_cast<gc_object**>(root_slot));
    }

    void collect() override {
        m_strategy.collect();
    }

    void mark_object(gc_object* obj) override {
        m_strategy.mark_object(obj);
    }

    size_t object_count() const override {
        return m_strategy.object_count();
    }

    size_t bytes_allocated() const override {
        return m_strategy.bytes_allocated();
    }

    size_t gc_threshold() const {
        return m_gc_threshold;
    }

    void set_gc_threshold(size_t threshold) {
        m_gc_threshold = threshold;
    }

    struct gc_stats {
        size_t collections_run;
        size_t objects_collected;
        size_t bytes_freed;
    };

    gc_stats get_stats() const {
        GCStats strategy_stats = m_strategy.get_stats();
        gc_stats stats;
        stats.collections_run = strategy_stats.collections_run;
        stats.objects_collected = strategy_stats.objects_collected;
        stats.bytes_freed = strategy_stats.bytes_reclaimed;
        return stats;
    }

    GenerationalGCStrategy& strategy() { return m_strategy; }
    const GenerationalGCStrategy& strategy() const { return m_strategy; }

    void reclaim(gc_object* obj) override {
        gc_box::deallocate(m_alloc, obj);
    }

    /**
     * @brief Write barrier for generational GC (implements memory::IWriteBarrier)
     *
     * Tracks old→young pointers in remembered set for minor GC correctness.
     * Called automatically by gc_field when fields are updated.
     */
    void write_barrier(void* owner, void* old_value, void* new_value) override {
        (void)old_value;  // Not used for generational barrier
        if (new_value) {
            m_strategy.record_write_barrier(
                static_cast<gc_object*>(owner),
                static_cast<gc_object*>(new_value));
        }
    }

    Allocator& allocator() {
        return m_alloc;
    }

    const Allocator& allocator() const {
        return m_alloc;
    }

protected:
    void* allocate_impl(size_t size, size_t align) override {
        return m_alloc.allocate(size, align);
    }

    void on_allocate(gc_object* obj) override {
        m_strategy.on_allocate(obj);
        if (m_strategy.bytes_allocated() > m_gc_threshold) {
            collect();
        }
    }

private:
    Allocator m_alloc;              // Must be first - strategy needs it during construction
    size_t m_gc_threshold;
    GenerationalGCStrategy m_strategy;
};

/**
 * @brief Strategy trait for compile-time strategy selection
 *
 * This allows user code to be generic over the GC strategy:
 *
 *   template<typename GCStrategy = MarkSweepStrategy>
 *   class Interpreter {
 *       gc_manager_t<GCStrategy> gc;
 *       ...
 *   };
 */
template<typename T>
concept GCStrategyConcept = requires(T strategy, gc_object* obj) {
    { strategy.on_allocate(obj) } -> std::same_as<void>;
    { strategy.collect() } -> std::same_as<void>;
    { strategy.add_root(obj) } -> std::same_as<void>;
    { strategy.remove_root(obj) } -> std::same_as<void>;
    { strategy.mark_object(obj) } -> std::same_as<void>;
    { strategy.object_count() } -> std::same_as<size_t>;
    { strategy.bytes_allocated() } -> std::same_as<size_t>;
    { strategy.get_stats() } -> std::same_as<GCStats>;
};

// Verify that our strategies satisfy the concept
static_assert(GCStrategyConcept<MarkSweepStrategy>, "MarkSweepStrategy must satisfy GCStrategyConcept");

// Convenient type aliases for common configurations
using default_gc = gc_manager_t<MarkSweepStrategy, MallocAllocator>;
using pool_gc = gc_manager_t<MarkSweepStrategy, PoolAllocator>;

/**
 * @brief Global templated GC instance
 *
 * Usage:
 *   gc_global<MarkSweepStrategy, MallocAllocator> gc;
 *   auto obj = gc.allocate<ConsCell>(car, cdr);
 */
template<typename Strategy = MarkSweepStrategy, typename Allocator = MallocAllocator>
inline gc_manager_t<Strategy, Allocator>* gc_global = nullptr;

/**
 * @brief Initialize templated GC
 *
 * Usage:
 *   initialize_gc_t<MarkSweepStrategy, MallocAllocator>();
 */
template<typename Strategy = MarkSweepStrategy, typename Allocator = MallocAllocator>
inline void initialize_gc_t() {
    if (!gc_global<Strategy, Allocator>) {
        gc_global<Strategy, Allocator> = new gc_manager_t<Strategy, Allocator>();
    }
}

/**
 * @brief Shutdown templated GC
 */
template<typename Strategy = MarkSweepStrategy, typename Allocator = MallocAllocator>
inline void shutdown_gc_t() {
    if (gc_global<Strategy, Allocator>) {
        delete gc_global<Strategy, Allocator>;
        gc_global<Strategy, Allocator> = nullptr;
    }
}
