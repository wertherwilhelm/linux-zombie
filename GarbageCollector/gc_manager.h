#pragma once

#include <vector>
#include <unordered_set>
#include <algorithm>
#include <iostream>
#include "gc_interface.h"
#include "gc_context.h"
#include "gc_box.h"

class gc_object;
class MarkSweepStrategy;  // Forward declaration

/**
 * @brief Garbage Collector Manager implementing mark-and-sweep collection
 *
 * This class manages all heap-allocated objects in the Lisp system and provides
 * automatic memory management through mark-and-sweep garbage collection.
 *
 * Implements IGarbageCollector interface for decoupling from Object model.
 */
class gc_manager : public IGarbageCollector
{
public:
    gc_manager();
    ~gc_manager();

    /**
     * @brief Allocate a new Object and register it with the GC
     *
     * @tparam T The type of Object to allocate (must derive from gc_object)
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return Pointer to the newly allocated Object
     */
    template<typename T, typename... Args>
    T* allocate(Args&&... args);

    /**
     * @brief Allocate and wrap in gc_ptr (RAII smart pointer with auto-rooting)
     *
     * @tparam T The type of Object to allocate (must derive from gc_object)
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return gc_ptr<T> wrapping the newly allocated Object
     *
     * Example:
     *   auto cell = gc->make_gc_ptr<ConsCell>(car, cdr);
     */
    template<typename T, typename... Args>
    auto make_gc_ptr(Args&&... args);  // Implementation in gc_ptr.h

    /**
     * @brief Allocate and wrap in gc_root (RAII root guard)
     *
     * @tparam T The type of Object to allocate (must derive from gc_object)
     * @tparam Args Constructor argument types
     * @param args Constructor arguments
     * @return gc_root<T> wrapping the newly allocated Object
     *
     * Example:
     *   auto cell = gc->make_gc_root<ConsCell>(car, cdr);
     */
    template<typename T, typename... Args>
    auto make_gc_root(Args&&... args);  // Implementation in gc_ptr.h

    // ========================================================================
    // IGarbageCollector interface implementation
    // ========================================================================

    /**
     * @brief Add a GC root (slot-based, implements memory::IRootManager)
     * @param root_slot Address of pointer to protect (&ptr, not ptr!)
     *
     * This is the NEW slot-based API from memory::IRootManager.
     * Moving GC can update *root_slot after relocation.
     */
    void add_root(void** root_slot) override;

    /**
     * @brief Remove a GC root (slot-based, implements memory::IRootManager)
     * @param root_slot Address previously passed to add_root()
     */
    void remove_root(void** root_slot) override;

    /**
     * @brief Manually trigger garbage collection
     */
    void collect() override;

    /**
     * @brief Mark an Object as reachable during the mark phase
     *
     * @param obj Object to mark
     */
    void mark_object(gc_object* obj) override;

    /**
     * @brief Reclaim a single Object (called by GC strategies during sweep)
     *
     * @param obj Object to deallocate
     */
    void reclaim(gc_object* obj) override;

    /**
     * @brief Get the number of objects currently managed by the GC
     *
     * @return Number of allocated objects
     */
    size_t object_count() const override;

    /**
     * @brief Get the number of bytes currently allocated
     *
     * @return Bytes allocated
     */
    size_t bytes_allocated() const override;

    /**
     * @brief Get the current GC threshold
     *
     * @return GC threshold in bytes
     */
    size_t gc_threshold() const { return m_gc_threshold; }

    /**
     * @brief Set the GC threshold
     *
     * @param threshold New threshold in bytes
     */
    void set_gc_threshold(size_t threshold) { m_gc_threshold = threshold; }

    /**
     * @brief Get garbage collection statistics
     */
    struct gc_stats {
        size_t collections_run;
        size_t objects_collected;
        size_t bytes_freed;
    };

    gc_stats get_stats() const;

protected:
    // ========================================================================
    // IGarbageCollector protected interface
    // ========================================================================

    /**
     * @brief Allocate raw memory (delegates to allocator)
     * @param size Size in bytes
     * @param align Alignment requirement
     * @return Pointer to allocated memory
     */
    void* allocate_impl(size_t size, size_t align) override;

    /**
     * @brief Notify GC of newly allocated Object
     * @param obj Constructed Object pointer
     */
    void on_allocate(gc_object* obj) override;

private:
    MarkSweepStrategy* m_strategy;     // GC strategy implementation
    size_t m_gc_threshold;             // Trigger GC when this is exceeded
};

// Global GC instance (defined in gc_manager.cpp)
extern gc_manager* gc;

// Note: Global GC instance is defined in gc_manager.cpp
// For access, use gc_context::current() from gc_context.h instead

/**
 * @brief RAII helper to temporarily register an Object as a GC root
 *
 * This ensures the Object won't be collected during its scope,
 * and automatically unregisters it when the scope exits.
 *
 * Thread-safety: Captures the owning GC at construction, ensuring
 * proper cleanup even if gc_context is swapped mid-lifetime.
 */
/**
 * @brief RAII guard for GC roots (non-copyable, slot-based for moving GC)
 *
 * IMPORTANT: Uses slot-based registration for moving GC support.
 * The _obj field is automatically updated by moving GC after objects are relocated.
 *
 * NOTE: This is the old-style guard. New code should prefer gc_root<T> or gc_ptr<T>
 * from gc_ptr.h for type safety and copyability.
 */
class gc_root_guard
{
public:
    gc_root_guard(gc_object* obj) : m_obj(obj), m_gc(gc_context::current())
    {
        if (m_obj && m_gc)
            m_gc->register_persistent_root(&m_obj);  // Register LOCATION, not value
    }

    ~gc_root_guard()
    {
        if (m_obj && m_gc)
            m_gc->unregister_persistent_root(&m_obj);  // Unregister slot
    }

    // Disable copy/move to prevent double-unregistration
    gc_root_guard(const gc_root_guard&) = delete;
    gc_root_guard& operator=(const gc_root_guard&) = delete;

    // Accessor to get current location (may have moved after GC!)
    gc_object* get() const { return m_obj; }

private:
    gc_object* m_obj;  // This field's ADDRESS is registered as a root slot
    IGarbageCollector* m_gc;  // Capture GC at construction
};

/**
 * @brief Initialize the garbage collection system
 *
 * Must be called before any Object allocation
 */
void initialize_gc();

/**
 * @brief Shutdown the garbage collection system
 *
 * Should be called at program exit
 */
void shutdown_gc();

// Include strategy implementation for template methods
#include "mark_sweep_strategy.h"

// Template method implementation (after MarkSweepStrategy definition)
template<typename T, typename... Args>
T* gc_manager::allocate(Args&&... args)
{
    // Debug: Check GC state before allocation
    static size_t alloc_count = 0;
    alloc_count++;
    if (alloc_count % 10000 == 0) {
        std::cerr << "[GC ALLOC] Count=" << alloc_count
                  << " strategy=" << (m_strategy ? "OK" : "NULL")
                  << " allocator=" << (m_allocator ? "OK" : "NULL")
                  << " bytes=" << m_strategy->bytes_allocated() << std::endl;
    }

    // Trigger GC BEFORE allocating, never after: a collection run after the
    // allocation would sweep the brand-new object, because the caller has
    // had no chance to root it yet. (Found via the GC obituary hook: a
    // binding cell allocated by Environment::define was reclaimed inside
    // the very call that created it.)
    if (m_strategy->bytes_allocated() > m_gc_threshold)
    {
        collect();
    }

    // Allocate with gc_box wrapper using stored allocator (pluggable!)
    // Uses m_allocator from Base class (IGarbageCollector)
    T* obj = gc_box::allocate<T>(*m_allocator, std::forward<Args>(args)...);
    m_strategy->on_allocate(obj);

    return obj;
}
