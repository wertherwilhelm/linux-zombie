#pragma once

#include <cstddef>  // for size_t

namespace memory {

/**
 * @file ITracer.h
 * @brief Optional tracing interface for garbage collectors
 *
 * This is an OPTIONAL capability for memory managers that support garbage collection.
 * Non-GC allocators (malloc, pools, smart pointers) do not need to implement this.
 *
 * Tracing allows GC to discover reachable objects by traversing the object graph.
 * Two modes are supported:
 * - Value-based: visit(ptr) - For non-moving GC (mark-sweep)
 * - Slot-based: visit_field(&ptr) - For moving GC (copying, generational)
 *
 * Capability Detection:
 *   template<typename T>
 *   concept SupportsTracing = requires(T& tracer, void** slot) {
 *       { tracer.visit_field(slot) } -> std::same_as<void>;
 *   };
 */

/**
 * @brief Tracer interface for GC object graph traversal
 *
 * During garbage collection, the GC walks the object graph starting from roots.
 * Each object implements ITraceable::trace_references() and uses an ITracer
 * to report pointers to child objects.
 *
 * Two visiting modes:
 *
 * 1. Value-based (non-moving GC):
 *    - Object calls tracer.visit(child_ptr)
 *    - Tracer marks the child as reachable
 *    - Pointers never change (objects don't move)
 *    - Simpler, but heap can fragment
 *
 * 2. Slot-based (moving GC):
 *    - Object calls tracer.visit_field(&child_ptr)
 *    - Tracer marks the child AND updates the pointer
 *    - After GC, pointers point to new locations
 *    - Complex, but compacts heap (no fragmentation)
 *
 * Thread Safety:
 * - Tracers are NOT thread-safe (single-threaded traversal)
 * - Each collection cycle creates a fresh tracer
 * - Parallel GC would use per-thread tracers + work-stealing
 */
class ITracer {
public:
    virtual ~ITracer() = default;

    /**
     * @brief Visit a pointer VALUE (for non-moving GC)
     * @param ptr Pointer to object being referenced
     *
     * Used by mark-sweep GC to mark reachable objects.
     * Pointers are NOT updated (objects don't move).
     *
     * Example (mark-sweep):
     *   void trace_references(ITracer& t) override {
     *       t.visit(child_);  // Mark child as reachable
     *       t.visit(next_);   // Mark next as reachable
     *   }
     *
     * After GC, child_ and next_ still point to same addresses.
     *
     * nullptr is safe to pass (treated as no-op).
     */
    virtual void visit(void* ptr) = 0;

    /**
     * @brief Visit a pointer SLOT (for moving GC)
     * @param slot Address of pointer field (allows GC to update it)
     *
     * Used by copying/generational GC to mark AND relocate objects.
     * After GC, *slot may point to a different address.
     *
     * Example (generational GC):
     *   void trace_references(ITracer& t) override {
     *       t.visit_field(reinterpret_cast<void**>(&child_));  // Mark & update
     *       t.visit_field(reinterpret_cast<void**>(&next_));   // Mark & update
     *   }
     *
     * After minor GC:
     *   - If child_ was in young gen, *slot now points to to-space or old-gen
     *   - Object can safely dereference child_ (pointer is updated)
     *
     * CRITICAL: Must pass ADDRESS of pointer (&ptr), not value!
     *
     * nullptr slot is safe to pass (treated as no-op).
     * *slot == nullptr is safe (no object to visit).
     */
    virtual void visit_field(void** slot) = 0;

    /**
     * @brief Visit a range of pointer slots (for arrays/containers)
     * @param slots Pointer to first element
     * @param count Number of elements
     *
     * Convenience method for tracing arrays/vectors.
     * Equivalent to calling visit_field() on each element.
     *
     * Default implementation: Loop calling visit_field()
     *
     * Derived implementations may optimize (e.g., batch processing).
     */
    virtual void visit_range(void** slots, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            visit_field(&slots[i]);
        }
    }
};

} // namespace memory
