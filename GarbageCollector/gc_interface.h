#pragma once

#include <cstddef>  // for size_t
#include <utility>  // for std::forward
#include "gc_box.h"  // for gc_box::allocate

// Forward declarations
class gc_object;

/**
 * @file gc_interface.h
 * @brief Abstract interface for garbage collectors
 *
 * Design Philosophy:
 * - Minimal interface (only what objects need to interact with GC)
 * - Type-safe allocation via templates
 * - Virtual dispatch only where necessary
 * - No implementation details exposed
 *
 * This is a self-contained interface. (It formerly inherited a set of
 * `memory::` abstraction interfaces meant to be shared with an external
 * scene-graph engine; that engine was abandoned and the abstraction layer
 * removed, so the allocate/deallocate/tracing/root/barrier methods are now
 * declared directly here.)
 */

// Forward declaration for allocator
struct IAllocator;

/**
 * @brief Abstract garbage collector interface
 *
 * Provides allocate/deallocate, object-graph tracing (visit/visit_field),
 * root management (add_root/remove_root), and a generational write barrier.
 *
 * Thread Safety:
 * - Implementations may be thread-local or thread-safe
 * - Caller must ensure proper synchronization
 * - See gc_context for thread-local GC management
 */
class IGarbageCollector
{
public:
    /**
     * @brief Construct GC with custom allocator
     * @param alloc Allocator to use (defaults to default_allocator if nullptr)
     */
    explicit IGarbageCollector(IAllocator* alloc = nullptr);

    virtual ~IGarbageCollector() = default;

    // ========================================================================
    // memory::IRootManager Interface Implementation
    // ========================================================================

    /**
     * @brief Register a root (implements memory::IRootManager)
     * @param root_slot Address of pointer to protect (&ptr, not ptr!)
     *
     * This is the NEW slot-based API from memory::IRootManager.
     * Moving GC can update *root_slot after relocation.
     *
     * Example:
     *   gc_object* my_obj = gc->allocate<MyObject>();
     *   gc->add_root(reinterpret_cast<void**>(&my_obj));  // Pass address!
     *
     * Thread Safety: Must be called from the thread that owns this GC instance.
     */
    virtual void add_root(void** root_slot) = 0;

    /**
     * @brief Unregister a root (implements memory::IRootManager)
     * @param root_slot Address previously passed to add_root()
     *
     * Must match a previous add_root call.
     */
    virtual void remove_root(void** root_slot) = 0;

    /**
     * @brief Register a persistent root (heap-allocated, long-lived root)
     * @param root_ptr Pointer to the gc_object* that should be treated as a root
     *
     * This is the type-safe wrapper around add_root(void**).
     * Used by gc_persistent_root for heap-allocated roots that outlive stack frames.
     *
     * Thread Safety: Must be called from the thread that owns this GC instance.
     */
    void register_persistent_root(gc_object** root_ptr) {
        add_root(reinterpret_cast<void**>(root_ptr));
    }

    /**
     * @brief Unregister a persistent root
     * @param root_ptr Pointer previously passed to register_persistent_root
     *
     * This is the type-safe wrapper around remove_root(void**).
     */
    void unregister_persistent_root(gc_object** root_ptr) {
        remove_root(reinterpret_cast<void**>(root_ptr));
    }

    // ========================================================================
    // Obituary Hook (per-object death notification)
    // ========================================================================

    /**
     * @brief Install a callback invoked for each object just before it is
     * deallocated (sweep or shutdown).
     *
     * The hook runs while the object is still valid, but its referenced
     * objects may already have been reclaimed in the same sweep — the hook
     * must only read the object shallowly and must not allocate GC memory
     * or retain the pointer.
     *
     * Pass nullptr/empty to remove. One hook at a time; the runtime layer
     * can fan out if it needs more.
     */
    void set_obituary_hook(void (*hook)(gc_object*)) {
        m_obituary_hook = hook;
    }

    // ========================================================================
    // memory::ITracer Interface Implementation
    // ========================================================================

    /**
     * @brief Visit pointer value (implements memory::ITracer)
     * @param ptr Pointer to Object to visit
     *
     * This is the NEW value-based API from memory::ITracer.
     * Used by non-moving GC (mark-sweep).
     *
     * Example:
     *   void trace_references(memory::ITracer& tracer) override {
     *       tracer.visit(child_);  // Mark child as reachable
     *   }
     */
    virtual void visit(void* ptr) {
        mark_object(static_cast<gc_object*>(ptr));
    }

    /**
     * @brief Visit pointer slot (implements memory::ITracer)
     * @param slot Address of pointer to visit (&ptr, not ptr!)
     *
     * This is the NEW slot-based API from memory::ITracer.
     * Used by moving GC (copying, generational).
     * Moving GC can update *slot after relocation.
     *
     * Example:
     *   void trace_references(memory::ITracer& tracer) override {
     *       tracer.visit_field(reinterpret_cast<void**>(&child_));
     *   }
     */
    virtual void visit_field(void** slot) {
        if (slot && *slot) {
            mark_object(static_cast<gc_object*>(*slot));
        }
    }

    /**
     * @brief Mark an Object as reachable during tracing (LEGACY)
     * @param obj Object to mark (can be null)
     *
     * Called by trace_references() implementations to mark child objects.
     * The GC will recursively trace marked objects to find all reachable data.
     *
     * Implementation Note:
     * - Should be idempotent (marking twice is safe)
     * - Should handle cycles gracefully
     * - Null pointers should be no-ops
     *
     * This is the legacy GC-specific API. New code should use visit() or
     * visit_field() from memory::ITracer interface.
     */
    virtual void mark_object(gc_object* obj) = 0;

    // ========================================================================
    // memory::IMemoryManager Interface Implementation
    // ========================================================================

    /**
     * @brief Allocate raw memory (implements memory::IMemoryManager)
     * @param size Size in bytes
     * @param alignment Alignment requirement
     * @return Pointer to allocated memory
     *
     * This is the NEW low-level API from memory::IMemoryManager.
     * Delegates to allocate_impl() for actual allocation.
     */
    virtual void* allocate(size_t size, size_t alignment) {
        return allocate_impl(size, alignment);
    }

    /**
     * @brief Deallocate memory (implements memory::IMemoryManager)
     * @param ptr Pointer to deallocate
     * @param size Size of allocation (hint)
     *
     * This is the NEW low-level API from memory::IMemoryManager.
     * For GC, this is typically a no-op (GC handles deallocation during collection).
     *
     * Explicit deallocation only needed for early cleanup before GC runs.
     */
    virtual void deallocate(void* ptr, size_t size) {
        (void)ptr;
        (void)size;
        // Intentionally a no-op for a tracing GC.
        //
        // Previously this called reclaim(ptr), which freed the object's memory
        // but left a dangling entry in the strategy's tracking set
        // (e.g. MarkSweepStrategy::m_all_objects). The next collection would then
        // either reclaim the same pointer again (double free) or dereference the
        // freed vtable during the mark phase (use-after-free).
        //
        // Under a tracing collector, manual deallocation is unnecessary and
        // unsafe: an unreachable object is reclaimed at the next collect(), and a
        // still-reachable object must NOT be freed here. Callers that need prompt
        // reclamation should drop their references and run a collection.
    }

    /**
     * @brief Type-safe Object allocation
     * @tparam T Type of Object to allocate (must derive from gc_object)
     * @tparam Args Constructor argument types
     * @param args Arguments forwarded to T's constructor
     * @return Pointer to newly allocated and constructed Object
     *
     * This is the primary allocation API for GC. It:
     * 1. Allocates memory via allocate_impl()
     * 2. Constructs T in-place with perfect forwarding
     * 3. Registers the Object with the GC for tracking
     *
     * Usage:
     *   auto* cell = gc->allocate<ConsCell>(car, cdr);
     *   auto* num = gc->allocate<atom<long>>(42);
     *
     * Note: Allocated objects are NOT automatically rooted. Use gc_root
     * or gc_ptr if the Object might not be reachable from other roots.
     */
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        // Use gc_box::allocate with stored allocator (pluggable!)
        // This ensures all objects have the gc_header prefix that gc_box::deallocate expects
        //
        // NOTE: Template methods cannot be virtual, so polymorphic calls through
        // IGarbageCollector* always use this implementation. For GenerationalGCStrategy
        // to use nursery allocation, call the concrete gc_manager_t<GenerationalGCStrategy>
        // type directly instead of through the interface.
        T* obj = gc_box::allocate<T>(*m_allocator, std::forward<Args>(args)...);

        // Register with GC (implementation-specific tracking).
        // NOTE: on_allocate() may trigger a threshold collection; it is responsible
        // for keeping the just-allocated (not-yet-rooted) object alive across that
        // collection — see the P1 fix in each on_allocate() override. (allocate<T>()
        // can't root here because the concrete managers shadow this template with
        // their own allocate<T>.)
        on_allocate(static_cast<gc_object*>(obj));

        return obj;
    }

    /**
     * @brief Get allocator name (implements memory::IMemoryManager)
     * @return Human-readable name
     *
     * Default implementation returns "IGarbageCollector".
     * Derived classes should override with specific GC strategy name.
     */
    virtual const char* name() const {
        return "IGarbageCollector";
    }

    /**
     * @brief Get total bytes allocated (implements memory::IMemoryManager)
     * @return Approximate bytes currently allocated
     */
    virtual size_t bytes_allocated() const = 0;

    /**
     * @brief Get count of live objects (for debugging/profiling)
     * @return Number of objects currently tracked by GC
     *
     * GC-specific method (not in memory:: interfaces).
     */
    virtual size_t object_count() const = 0;

    /**
     * @brief Trigger a garbage collection cycle
     *
     * Runs a full collection: mark all reachable objects, sweep unreachable.
     * This is typically called automatically when allocation threshold is reached,
     * but can be called manually for testing or before critical sections.
     *
     * Note: This is a stop-the-world operation in most implementations.
     */
    virtual void collect() = 0;

    /**
     * @brief Reclaim a single Object (called by GC strategies during sweep)
     * @param obj Object to deallocate
     *
     * This is the single deallocation path for GC-managed objects.
     * Strategies call this instead of direct free/delete to allow:
     * - Pluggable allocators (malloc vs pool)
     * - Finalization hooks
     * - Statistics tracking
     * - Debug checks
     *
     * Implementation delegates to allocator-aware gc_box::deallocate.
     */
    virtual void reclaim(gc_object* obj) = 0;

    // ========================================================================
    // memory::IWriteBarrier Interface Implementation
    // ========================================================================

    /**
     * @brief Record write barrier (implements memory::IWriteBarrier)
     * @param owner Object being modified (contains the field)
     * @param old_value Previous value of the field (may be nullptr)
     * @param new_value New value being written (may be nullptr)
     *
     * This is the NEW API from memory::IWriteBarrier.
     * Called automatically by gc_field when fields are updated.
     * Allows generational GC to track old→young pointers.
     *
     * Default implementation: No-op (for mark-sweep GC).
     * Generational GC: Records owner in remembered set if new_value is in young gen.
     *
     * Example:
     *   old_gen_node->left = young_child;
     *   // gc_field calls: gc->write_barrier(old_gen_node, nullptr, young_child)
     *   // GC adds old_gen_node to remembered set
     *
     * Benefits:
     * - No RTTI/dynamic_cast needed
     * - No heavy includes in gc_field.h
     * - Centralized barrier logic
     * - Easy to instrument/profile
     */
    virtual void write_barrier(void* owner, void* old_value, void* new_value) {
        // Default: no-op for non-generational collectors
        // Derived classes override this for generational GC
        (void)owner;
        (void)old_value;
        (void)new_value;
    }

    /**
     * @brief Type-safe write barrier wrapper (for convenience)
     * @param owner GC Object being modified
     * @param old_value Previous gc_object value
     * @param new_value New gc_object value
     *
     * Convenience wrapper that delegates to memory::IWriteBarrier::write_barrier
     */
    void write_barrier(gc_object* owner, gc_object* old_value, gc_object* new_value) {
        write_barrier(static_cast<void*>(owner),
                      static_cast<void*>(old_value),
                      static_cast<void*>(new_value));
    }

protected:
    /**
     * @brief Allocate raw memory (implementation-specific)
     * @param size Size in bytes to allocate
     * @param align Alignment requirement
     * @return Pointer to allocated memory (uninitialized)
     *
     * This is the low-level allocation hook. Implementations can:
     * - Use malloc/new
     * - Manage memory pools
     * - Track allocation sizes
     * - Implement different strategies per size class
     *
     * Note: Memory is NOT zero-initialized. Caller must construct objects.
     */
    virtual void* allocate_impl(size_t size, size_t align) = 0;

    /**
     * @brief Notify GC of newly allocated Object (for tracking)
     * @param obj Pointer to constructed Object
     *
     * Called after Object construction to register it with the GC.
     * Implementations typically add to a tracking list and update stats.
     */
    virtual void on_allocate(gc_object* obj) = 0;

    /**
     * @brief Pluggable allocator for memory management
     *
     * Allows GC to use different allocation strategies:
     * - MallocAllocator (default) - standard malloc/free
     * - PoolAllocator - fixed-size block pools
     * - ArenaAllocator - region-based allocation (future)
     *
     * Set via constructor, defaults to default_allocator.
     */
    IAllocator* m_allocator;

    /**
     * @brief Optional per-object death notification (see set_obituary_hook).
     * Plain function pointer: no allocation, no <functional> dependency.
     */
    void (*m_obituary_hook)(gc_object*) = nullptr;
};

/**
 * @brief Thread-local GC context (forward declaration)
 *
 * Provides access to the current thread's GC instance without globals.
 * Defined in gc_context.h (to be created in Phase 2).
 *
 * Usage:
 *   IGarbageCollector* gc = gc_context::current();
 *   auto* obj = gc->allocate<ConsCell>(car, cdr);
 */
class gc_context;
