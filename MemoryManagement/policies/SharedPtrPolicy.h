#pragma once

#include "../IMemoryManager.h"
#include "../ITracer.h"
#include "../concepts.h"
#include <memory>
#include <vector>

namespace memory {
namespace policies {

/**
 * @file SharedPtrPolicy.h
 * @brief Policy adapter for std::shared_ptr reference counting
 *
 * This policy uses standard C++ smart pointers for automatic memory management.
 * No garbage collection - relies on reference counting instead.
 *
 * Benefits:
 * - No GC dependency (pure C++ standard library)
 * - Portable (works everywhere C++11+ works)
 * - Deterministic destruction (RAII)
 * - Thread-safe reference counting (atomic ops)
 *
 * Drawbacks:
 * - Cannot handle cycles (circular references leak)
 * - Overhead per object (refcount + weak count)
 * - Atomic operations on every copy/destroy
 * - No compaction (heap fragmentation possible)
 *
 * Usage:
 *   template<typename Policy>
 *   class Node {
 *       typename Policy::template Ptr<Node> left, right;
 *       // ...
 *   };
 *
 *   using SharedNode = Node<SharedPtrPolicy>;
 *
 *   auto node = SharedPtrPolicy::allocate<SharedNode>();
 *   // ... use node ...
 *   // Automatic cleanup when last reference drops
 */
struct SharedPtrPolicy {
    // ========================================================================
    // Type Aliases
    // ========================================================================

    /**
     * @brief Pointer type for reference-counted objects
     *
     * Uses std::shared_ptr for automatic reference counting.
     * Thread-safe (atomic refcount operations).
     */
    template<typename T>
    using Ptr = std::shared_ptr<T>;

    /**
     * @brief Container type for reference-counted collections
     *
     * Standard vector of shared_ptr - no special GC integration needed.
     */
    template<typename T>
    using Container = std::vector<std::shared_ptr<T>>;

    // ========================================================================
    // Allocation/Deallocation
    // ========================================================================

    /**
     * @brief Allocate reference-counted object
     * @tparam T Type to allocate
     * @tparam Args Constructor argument types
     * @param args Arguments forwarded to T's constructor
     * @return shared_ptr to allocated object
     *
     * Uses std::make_shared for optimal allocation:
     * - Single allocation for object + control block
     * - Exception-safe
     * - Cache-friendly (object and refcount together)
     *
     * Example:
     *   auto node = SharedPtrPolicy::allocate<Node>(42);
     *   auto copy = node;  // Increment refcount
     *   copy.reset();      // Decrement refcount
     *   // node deleted when last reference drops
     */
    template<typename T, typename... Args>
    static std::shared_ptr<T> allocate(Args&&... args) {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    /**
     * @brief Deallocate reference-counted object
     * @tparam T Type of object
     * @param ptr shared_ptr to deallocate
     *
     * Decrements reference count. Object deleted when count reaches 0.
     *
     * Example:
     *   SharedPtrPolicy::deallocate(node);  // Equivalent to node.reset()
     */
    template<typename T>
    static void deallocate(std::shared_ptr<T>& ptr) {
        ptr.reset();
    }

    // ========================================================================
    // GC Integration (No-Ops for Reference Counting)
    // ========================================================================

    /**
     * @brief Trace pointer (no-op for reference counting)
     * @tparam T Type of object
     * @param tracer Unused (no tracing in refcount GC)
     * @param ptr_slot shared_ptr to "trace"
     *
     * Reference counting doesn't use tracing - reachability is implicit
     * in the reference count. This is a no-op for API compatibility.
     *
     * Example:
     *   void trace_references(ITracer& t) override {
     *       SharedPtrPolicy::trace(t, left_);   // No-op
     *       SharedPtrPolicy::trace(t, right_);  // No-op
     *   }
     */
    template<typename T>
    static void trace(ITracer& tracer, std::shared_ptr<T>& ptr_slot) {
        // No-op: Reference counting doesn't use tracing
        (void)tracer;
        (void)ptr_slot;
    }

    /**
     * @brief Trace const pointer (no-op)
     */
    template<typename T>
    static void trace(ITracer& tracer, const std::shared_ptr<T>& ptr) {
        (void)tracer;
        (void)ptr;
    }

    // ========================================================================
    // Write Barrier (No-Op for Reference Counting)
    // ========================================================================

    /**
     * @brief Write barrier (no-op for reference counting)
     * @param owner Object being modified (unused)
     * @param old_value Previous value (unused)
     * @param new_value New value (unused)
     *
     * Reference counting doesn't use write barriers - reference count
     * updates happen automatically on assignment. No-op for API compatibility.
     */
    template<typename Owner, typename T>
    static void write_barrier(Owner* owner, const std::shared_ptr<T>& old_value,
                               const std::shared_ptr<T>& new_value) {
        // No-op: Reference counting doesn't need write barriers
        (void)owner;
        (void)old_value;
        (void)new_value;
    }

    // ========================================================================
    // Capability Flags
    // ========================================================================

    /**
     * @brief Compile-time capability flags
     *
     * Reference counting has no GC capabilities:
     * - No tracing (implicit reachability via refcount)
     * - No write barriers (refcount updated on assignment)
     * - No explicit roots (every shared_ptr is a "root")
     */
    static constexpr bool supports_gc = false;
    static constexpr bool supports_tracing = false;
    static constexpr bool supports_barriers = false;
    static constexpr bool supports_rooting = false;

    /**
     * @brief Runtime capability check (always false)
     */
    static bool has_write_barriers() { return false; }
    static bool has_tracing() { return false; }

    // ========================================================================
    // Cycle Detection Helpers
    // ========================================================================

    /**
     * @brief Check if object has circular references
     * @param ptr Object to check
     * @return true if object may be in a cycle
     *
     * Uses weak_ptr count to detect potential cycles.
     * If weak_count > 0, object might be in a cycle (parent back-pointers).
     *
     * WARNING: This is a heuristic, not a guarantee.
     */
    template<typename T>
    static bool may_have_cycles(const std::shared_ptr<T>& ptr) {
        return ptr.use_count() > 1;  // Multiple references (possible cycle)
    }

    /**
     * @brief Break cycle by converting to weak_ptr
     * @param ptr Strong reference to convert
     * @return Weak reference (doesn't prevent deletion)
     *
     * Use for parent back-pointers to break cycles:
     *   struct Node {
     *       std::shared_ptr<Node> left, right;  // Children (strong)
     *       std::weak_ptr<Node> parent;         // Parent (weak)
     *   };
     */
    template<typename T>
    static std::weak_ptr<T> make_weak(const std::shared_ptr<T>& ptr) {
        return std::weak_ptr<T>(ptr);
    }
};

} // namespace policies
} // namespace memory
