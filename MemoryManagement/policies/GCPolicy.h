#pragma once

#include "../IMemoryManager.h"
#include "../ITracer.h"
#include "../ITraceable.h"
#include "../IWriteBarrier.h"
#include "../IRootManager.h"
#include "../concepts.h"
#include <type_traits>
#include <stdexcept>

// GCPolicy is an adapter over the real tracing collector. It allocates through
// the current GC context so every object receives a gc_header (type-specific
// tracer + destructor) and is registered for mark/sweep tracking. Previously it
// used raw IMemoryManager::allocate(), producing header-less, untracked objects
// that were invisible to the GC (reclaimed incorrectly / double-freed).
#include "../../GarbageCollector/gc_object.h"
#include "../../GarbageCollector/gc_context.h"

namespace memory {
namespace policies {

/**
 * @file GCPolicy.h
 * @brief Policy adapter for garbage-collected memory
 *
 * This policy routes all allocations through the current GC context
 * (thread-local). It assumes:
 * - gc_context::current() returns active GC instance
 * - GC implements all memory:: interfaces (IMemoryManager, ITracer, etc.)
 * - Objects derive from memory::ITraceable for tracing
 *
 * Usage:
 *   template<typename Policy>
 *   class Node {
 *       typename Policy::template Ptr<Node> left, right;
 *       // ...
 *   };
 *
 *   using GCNode = Node<GCPolicy>;
 *
 *   GCNode* node = GCPolicy::allocate<GCNode>();
 *   // ... use node ...
 *   // No explicit delete - GC handles it
 */
struct GCPolicy {
    // ========================================================================
    // Type Aliases
    // ========================================================================

    /**
     * @brief Pointer type for GC-managed objects
     *
     * Raw pointers are used because GC manages lifetime.
     * No smart pointer wrapper needed (GC is the "smart" part).
     */
    template<typename T>
    using Ptr = T*;

    /**
     * @brief Container type for GC-managed object collections
     *
     * Forward declaration - actual implementation in gc_containers.h
     * Uses slot-based tracing for moving GC support.
     */
    template<typename T>
    class Container;  // Defined in gc_containers.h (gc_vector)

    // ========================================================================
    // Allocation/Deallocation
    // ========================================================================

    /**
     * @brief Allocate GC-managed object
     * @tparam T Type to allocate (must derive from ITraceable if has pointers)
     * @tparam Args Constructor argument types
     * @param args Arguments forwarded to T's constructor
     * @return Pointer to allocated object
     *
     * Delegates to current GC context:
     * 1. Get thread-local GC via gc_context::current()
     * 2. Allocate memory via IMemoryManager::allocate()
     * 3. Construct object in-place
     * 4. Register with GC for tracking
     *
     * Example:
     *   Node* node = GCPolicy::allocate<Node>(42);
     *   // GC will collect when unreachable
     *
     * Thread Safety:
     * - Thread-local GC context (no synchronization needed)
     * - Each thread has its own heap
     */
    template<typename T, typename... Args>
    static T* allocate(Args&&... args) {
        static_assert(std::is_base_of_v<gc_object, T>,
            "GCPolicy manages tracing-GC objects: T must derive from gc_object so "
            "the collector can install a gc_header and trace/reclaim it correctly.");

        IGarbageCollector* gc = get_current_gc();
        if (!gc) {
            throw std::runtime_error("No active GC context");
        }

        // Typed, tracked allocation. IGarbageCollector::allocate<T>() routes
        // through gc_box::allocate (installing the gc_header with the
        // type-specific tracer + destructor) and then registers the object with
        // the collector via on_allocate(), so it participates in mark/sweep.
        return gc->allocate<T>(std::forward<Args>(args)...);
    }

    /**
     * @brief Deallocate GC-managed object
     * @tparam T Type of object
     * @param ptr Pointer to deallocate
     *
     * NOTE: For GC, this is typically a no-op. GC handles deallocation
     * automatically during collection. Explicit deallocation only needed
     * for early cleanup (before GC runs).
     *
     * Example:
     *   GCPolicy::deallocate(node);  // Optional - GC will collect anyway
     */
    template<typename T>
    static void deallocate(T* ptr) {
        // No-op: under a tracing collector the object is reclaimed (and its
        // destructor run via the gc_header destructor fn) during collection once
        // it becomes unreachable. Destructing/freeing here would run the dtor
        // twice and desync GC tracking (double free). Drop references and let the
        // collector reclaim it instead.
        (void)ptr;
    }

    // ========================================================================
    // GC Integration (Tracing)
    // ========================================================================

    /**
     * @brief Trace pointer for GC
     * @tparam T Type of object (must be ITraceable for recursive tracing)
     * @param tracer GC tracer
     * @param ptr_slot Address of pointer to trace (slot-based!)
     *
     * This is called during GC mark phase to trace object graph.
     *
     * Behavior:
     * - If T is ITraceable: Visit slot (allows moving GC to update pointer)
     * - If T is not ITraceable: No-op (leaf object, no children)
     *
     * Example:
     *   void trace_references(ITracer& t) override {
     *       GCPolicy::trace(t, left_);   // Trace left child
     *       GCPolicy::trace(t, right_);  // Trace right child
     *   }
     *
     * Slot-Based Semantics:
     * - Pass ADDRESS of pointer (&ptr), not value
     * - Allows moving GC to update pointer after relocation
     * - Critical for generational/copying GC correctness
     */
    template<typename T>
    static void trace(ITracer& tracer, T*& ptr_slot) {
        if constexpr (std::is_base_of_v<ITraceable, T> || Traceable<T>) {
            // Traceable object - visit slot for moving GC
            if (ptr_slot) {
                tracer.visit_field(reinterpret_cast<void**>(&ptr_slot));
            }
        } else {
            // Non-traceable (leaf object) - just mark as reachable
            if (ptr_slot) {
                tracer.visit(ptr_slot);
            }
        }
    }

    /**
     * @brief Trace const pointer (for read-only traversal)
     *
     * Some objects need to trace const pointers. This uses visit()
     * instead of visit_field() since we can't update const pointers.
     */
    template<typename T>
    static void trace(ITracer& tracer, const T* ptr) {
        if (ptr) {
            tracer.visit(const_cast<T*>(ptr));
        }
    }

    // ========================================================================
    // Write Barrier (Generational GC)
    // ========================================================================

    /**
     * @brief Fire write barrier for pointer assignment
     * @param owner Object being modified
     * @param old_value Previous pointer value
     * @param new_value New pointer value
     *
     * Used by generational GC to track old→young pointers.
     * Automatically called by gc_field when bound to owner.
     *
     * Example:
     *   void set_child(Node* child) {
     *       Node* old_child = child_;
     *       child_ = child;
     *       GCPolicy::write_barrier(this, old_child, child);
     *   }
     */
    template<typename Owner, typename T>
    static void write_barrier(Owner* owner, T* old_value, T* new_value) {
        IMemoryManager* mgr = get_current_gc();
        if (!mgr) return;

        // Check if GC supports write barriers
        if constexpr (SupportsWriteBarrier<IMemoryManager>) {
            IWriteBarrier* barrier = dynamic_cast<IWriteBarrier*>(mgr);
            if (barrier) {
                barrier->write_barrier(owner, old_value, new_value);
            }
        }
    }

    // ========================================================================
    // Capability Flags
    // ========================================================================

    /**
     * @brief Compile-time capability flags
     *
     * Libraries can use these to conditionally enable features:
     *   if constexpr (Policy::supports_gc) {
     *       // Enable GC-specific optimizations
     *   }
     */
    static constexpr bool supports_gc = true;
    static constexpr bool supports_tracing = true;
    static constexpr bool supports_barriers = true;
    static constexpr bool supports_rooting = true;

    /**
     * @brief Runtime capability check
     *
     * Check if current GC context supports specific capabilities.
     * Use this when capability depends on runtime GC type.
     */
    static bool has_write_barriers() {
        IMemoryManager* mgr = get_current_gc();
        if (!mgr) return false;
        return dynamic_cast<IWriteBarrier*>(mgr) != nullptr;
    }

    static bool has_tracing() {
        IMemoryManager* mgr = get_current_gc();
        if (!mgr) return false;
        return dynamic_cast<ITracer*>(mgr) != nullptr;
    }
};

} // namespace policies
} // namespace memory
