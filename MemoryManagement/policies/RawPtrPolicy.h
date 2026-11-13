#pragma once

#include "../IMemoryManager.h"
#include "../ITracer.h"
#include "../concepts.h"
#include <vector>
#include <new>

namespace memory {
namespace policies {

/**
 * @file RawPtrPolicy.h
 * @brief Policy adapter for manual memory management (raw pointers)
 *
 * This policy uses raw pointers and manual new/delete for memory management.
 * No garbage collection, no reference counting - programmer controls lifetime.
 *
 * Benefits:
 * - Zero overhead (no refcount, no GC metadata)
 * - Full control over allocation/deallocation
 * - Predictable performance (no GC pauses)
 * - Minimal binary size
 *
 * Drawbacks:
 * - Manual lifetime management (easy to leak or double-free)
 * - No cycle handling
 * - No automatic cleanup
 * - Dangling pointer bugs possible
 *
 * Best For:
 * - Performance-critical code (real-time systems)
 * - Embedded systems (no GC overhead)
 * - Prototyping before adding GC
 * - Short-lived objects with clear ownership
 *
 * Usage:
 *   template<typename Policy>
 *   class Node {
 *       typename Policy::template Ptr<Node> left, right;
 *       // ...
 *   };
 *
 *   using RawNode = Node<RawPtrPolicy>;
 *
 *   RawNode* node = RawPtrPolicy::allocate<RawNode>();
 *   // ... use node ...
 *   RawPtrPolicy::deallocate(node);  // MUST manually delete!
 */
struct RawPtrPolicy {
    // ========================================================================
    // Type Aliases
    // ========================================================================

    /**
     * @brief Pointer type for manually-managed objects
     *
     * Raw pointers - no automatic lifetime management.
     */
    template<typename T>
    using Ptr = T*;

    /**
     * @brief Container type for manually-managed collections
     *
     * Standard vector of raw pointers - caller responsible for deletion.
     */
    template<typename T>
    using Container = std::vector<T*>;

    // ========================================================================
    // Allocation/Deallocation
    // ========================================================================

    /**
     * @brief Allocate object with manual lifetime
     * @tparam T Type to allocate
     * @tparam Args Constructor argument types
     * @param args Arguments forwarded to T's constructor
     * @return Raw pointer to allocated object
     *
     * Uses operator new - caller MUST manually delete.
     *
     * Example:
     *   Node* node = RawPtrPolicy::allocate<Node>(42);
     *   // ... use node ...
     *   RawPtrPolicy::deallocate(node);  // REQUIRED!
     *
     * Leak Example (BAD):
     *   Node* node = RawPtrPolicy::allocate<Node>(42);
     *   // ... forgot to delete ...
     *   // LEAK!
     */
    template<typename T, typename... Args>
    static T* allocate(Args&&... args) {
        return new T(std::forward<Args>(args)...);
    }

    /**
     * @brief Deallocate manually-managed object
     * @tparam T Type of object
     * @param ptr Pointer to deallocate
     *
     * Uses operator delete - caller responsible for calling this.
     *
     * Example:
     *   RawPtrPolicy::deallocate(node);
     *
     * Double-Free Example (BAD):
     *   RawPtrPolicy::deallocate(node);
     *   RawPtrPolicy::deallocate(node);  // CRASH!
     *
     * Dangling Pointer Example (BAD):
     *   Node* ptr = node;
     *   RawPtrPolicy::deallocate(node);
     *   ptr->value = 42;  // CRASH or corruption!
     */
    template<typename T>
    static void deallocate(T* ptr) {
        delete ptr;
    }

    // ========================================================================
    // Array Allocation (Bonus)
    // ========================================================================

    /**
     * @brief Allocate array of objects
     * @tparam T Type of elements
     * @param count Number of elements
     * @return Pointer to array
     *
     * Uses operator new[] - caller MUST use deallocate_array.
     *
     * Example:
     *   Node* arr = RawPtrPolicy::allocate_array<Node>(100);
     *   // ... use arr[0..99] ...
     *   RawPtrPolicy::deallocate_array(arr);
     */
    template<typename T>
    static T* allocate_array(size_t count) {
        return new T[count];
    }

    /**
     * @brief Deallocate array of objects
     * @tparam T Type of elements
     * @param ptr Pointer to array
     *
     * Uses operator delete[] - MUST match allocate_array.
     *
     * Mismatch Example (BAD):
     *   Node* arr = RawPtrPolicy::allocate_array<Node>(10);
     *   RawPtrPolicy::deallocate(arr);  // WRONG! Should be deallocate_array
     */
    template<typename T>
    static void deallocate_array(T* ptr) {
        delete[] ptr;
    }

    // ========================================================================
    // GC Integration (No-Ops for Manual Management)
    // ========================================================================

    /**
     * @brief Trace pointer (no-op for manual management)
     * @tparam T Type of object
     * @param tracer Unused
     * @param ptr_slot Pointer to "trace"
     *
     * Manual memory management doesn't use tracing.
     * No-op for API compatibility.
     */
    template<typename T>
    static void trace(ITracer& tracer, T*& ptr_slot) {
        // No-op: Manual management doesn't use tracing
        (void)tracer;
        (void)ptr_slot;
    }

    /**
     * @brief Trace const pointer (no-op)
     */
    template<typename T>
    static void trace(ITracer& tracer, const T* ptr) {
        (void)tracer;
        (void)ptr;
    }

    // ========================================================================
    // Write Barrier (No-Op for Manual Management)
    // ========================================================================

    /**
     * @brief Write barrier (no-op for manual management)
     *
     * Manual memory management doesn't use write barriers.
     * No-op for API compatibility.
     */
    template<typename Owner, typename T>
    static void write_barrier(Owner* owner, T* old_value, T* new_value) {
        // No-op: Manual management doesn't need write barriers
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
     * Manual management has no GC capabilities.
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
    // Ownership Helpers
    // ========================================================================

    /**
     * @brief Transfer ownership of pointer
     * @param ptr Pointer to transfer
     * @return Same pointer (for chaining)
     *
     * Semantic helper to make ownership transfer explicit:
     *   Node* node = RawPtrPolicy::take_ownership(create_node());
     *   // Caller now owns node, must delete it
     */
    template<typename T>
    static T* take_ownership(T* ptr) {
        return ptr;  // No-op, just documentation
    }

    /**
     * @brief Release ownership of pointer without deleting
     * @param ptr Pointer to release
     * @return Same pointer
     *
     * Semantic helper for releasing ownership:
     *   Node* node = create_node();
     *   give_to_caller(RawPtrPolicy::release_ownership(node));
     *   // Caller now owns node, we don't delete it
     */
    template<typename T>
    static T* release_ownership(T* ptr) {
        return ptr;  // No-op, just documentation
    }
};

} // namespace policies
} // namespace memory
