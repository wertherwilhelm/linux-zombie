#pragma once

#include "IMemoryManager.h"
#include "ITracer.h"
#include "ITraceable.h"
#include "IWriteBarrier.h"
#include "IRootManager.h"
#include <concepts>
#include <type_traits>

/**
 * @file concepts.h
 * @brief Compile-time capability detection for memory managers
 *
 * This file provides C++20 concepts for detecting optional capabilities
 * of memory management strategies. This allows code to conditionally
 * enable features based on allocator capabilities.
 *
 * Design Philosophy:
 * - Required: IMemoryManager (allocate/deallocate)
 * - Optional: ITracer, ITraceable, IWriteBarrier, IRootManager
 * - Detect at compile time via concepts
 * - Enable/disable code paths based on capabilities
 *
 * Example:
 *   template<typename Mgr>
 *   void process(Mgr& mgr, Object* obj) {
 *       // Allocate (always available)
 *       void* ptr = mgr.allocate(100, 8);
 *
 *       // Trace (only if supported)
 *       if constexpr (memory::SupportsTracing<Mgr>) {
 *           obj->trace_references(mgr);
 *       }
 *
 *       // Write barrier (only if supported)
 *       if constexpr (memory::SupportsWriteBarrier<Mgr>) {
 *           mgr.write_barrier(obj, old_val, new_val);
 *       }
 *   }
 */

namespace memory {

// ============================================================================
// Core Memory Management
// ============================================================================

/**
 * @brief Memory manager with allocate/deallocate (REQUIRED)
 *
 * All memory managers must satisfy this concept.
 */
template<typename T>
concept MemoryManager = requires(T& mgr, size_t size, size_t align, void* ptr) {
    { mgr.allocate(size, align) } -> std::same_as<void*>;
    { mgr.deallocate(ptr, size) } -> std::same_as<void>;
};

/**
 * @brief Verify all standard memory managers satisfy concept
 */
static_assert(MemoryManager<IMemoryManager>,
              "IMemoryManager must satisfy MemoryManager concept");

// ============================================================================
// Optional GC Capabilities
// ============================================================================

/**
 * @brief Supports GC tracing (mark/trace phase)
 *
 * Memory manager can trace object graph for reachability analysis.
 *
 * DESIGN: This concept uses the "manager-as-tracer" pattern where GC
 * implementations inherit from both IMemoryManager and ITracer. This
 * allows the manager to be passed directly to trace_references() calls:
 *
 *   if constexpr (SupportsTracing<GC>) {
 *       obj->trace_references(gc);  // gc acts as both manager and tracer
 *   }
 *
 * The concept checks for ITracer interface methods (visit/visit_field)
 * using duck typing, allowing any type that implements these methods
 * to satisfy the concept, regardless of inheritance.
 */
template<typename T>
concept SupportsTracing = requires(T& tracer, void* ptr, void** slot) {
    { tracer.visit(ptr) } -> std::same_as<void>;
    { tracer.visit_field(slot) } -> std::same_as<void>;
};

/**
 * @brief Object can be traced by GC
 *
 * Object reports references to other GC-managed objects by implementing
 * the trace_references(ITracer&) method.
 *
 * DESIGN: This concept requires the trace_references method unconditionally
 * using duck typing. Any type with a public trace_references method satisfies
 * this concept, regardless of whether it inherits from ITraceable.
 *
 * This is more flexible than checking inheritance, as it allows:
 * - Types that implement the interface without inheriting
 * - Mock objects for testing
 * - Template-based implementations
 *
 * Note: The requires clause checks for PUBLIC accessibility and correct
 * signature, preventing types with private/inaccessible methods from
 * incorrectly satisfying the concept.
 */
template<typename T>
concept Traceable = requires(T& obj, ITracer& tracer) {
    { obj.trace_references(tracer) } -> std::same_as<void>;
};

/**
 * @brief Supports write barriers (generational/incremental GC)
 *
 * Memory manager tracks pointer modifications for cross-generation edges.
 */
template<typename T>
concept SupportsWriteBarrier = requires(T& mgr, void* owner, void* old_val, void* new_val) {
    { mgr.write_barrier(owner, old_val, new_val) } -> std::same_as<void>;
};

/**
 * @brief Supports root management (GC entry points)
 *
 * Memory manager maintains set of roots for tracing.
 */
template<typename T>
concept SupportsRooting = requires(T& mgr, void** root_slot) {
    { mgr.add_root(root_slot) } -> std::same_as<void>;
    { mgr.remove_root(root_slot) } -> std::same_as<void>;
};

// ============================================================================
// Compound Capabilities
// ============================================================================

/**
 * @brief Full tracing GC (mark-sweep, copying, generational)
 *
 * Supports all capabilities needed for tracing garbage collection:
 * - Memory management (allocate/deallocate)
 * - Tracing (mark phase)
 * - Rooting (entry points)
 */
template<typename T>
concept TracingGC =
    MemoryManager<T> &&
    SupportsTracing<T> &&
    SupportsRooting<T>;

/**
 * @brief Generational GC (young/old generations)
 *
 * Supports all capabilities for generational collection:
 * - Tracing GC (base capabilities)
 * - Write barriers (cross-generation tracking)
 */
template<typename T>
concept GenerationalGC =
    TracingGC<T> &&
    SupportsWriteBarrier<T>;

/**
 * @brief Manual memory manager (malloc-style)
 *
 * Only supports allocate/deallocate (no GC capabilities).
 */
template<typename T>
concept ManualMemoryManager =
    MemoryManager<T> &&
    !SupportsTracing<T> &&
    !SupportsRooting<T>;

// ============================================================================
// Type Traits (for non-concept code)
// ============================================================================

/**
 * @brief Check if type supports tracing at compile time
 */
template<typename T>
struct supports_tracing : std::bool_constant<SupportsTracing<T>> {};

template<typename T>
inline constexpr bool supports_tracing_v = supports_tracing<T>::value;

/**
 * @brief Check if type supports write barriers
 */
template<typename T>
struct supports_write_barrier : std::bool_constant<SupportsWriteBarrier<T>> {};

template<typename T>
inline constexpr bool supports_write_barrier_v = supports_write_barrier<T>::value;

/**
 * @brief Check if type supports rooting
 */
template<typename T>
struct supports_rooting : std::bool_constant<SupportsRooting<T>> {};

template<typename T>
inline constexpr bool supports_rooting_v = supports_rooting<T>::value;

/**
 * @brief Check if type is a full tracing GC
 */
template<typename T>
struct is_tracing_gc : std::bool_constant<TracingGC<T>> {};

template<typename T>
inline constexpr bool is_tracing_gc_v = is_tracing_gc<T>::value;

/**
 * @brief Check if type is a generational GC
 */
template<typename T>
struct is_generational_gc : std::bool_constant<GenerationalGC<T>> {};

template<typename T>
inline constexpr bool is_generational_gc_v = is_generational_gc<T>::value;

// ============================================================================
// Usage Examples
// ============================================================================

namespace examples {
    /**
     * @brief Generic function with conditional GC support
     */
    template<MemoryManager Mgr>
    void process_object(Mgr& mgr, void* obj) {
        // Allocation (always available)
        void* new_obj = mgr.allocate(100, 8);

        // Tracing (only if GC)
        if constexpr (SupportsTracing<Mgr>) {
            // Can use as tracer
            ITracer& tracer = static_cast<ITracer&>(mgr);
            tracer.visit(obj);
        }

        // Write barrier (only if generational)
        if constexpr (SupportsWriteBarrier<Mgr>) {
            mgr.write_barrier(obj, nullptr, new_obj);
        }

        // Deallocation (always available)
        mgr.deallocate(new_obj, 100);
    }

    /**
     * @brief Function requiring full GC capabilities
     */
    template<TracingGC GC>
    void run_gc(GC& gc, void** roots, size_t root_count) {
        // Register roots
        for (size_t i = 0; i < root_count; ++i) {
            gc.add_root(&roots[i]);
        }

        // Trace from roots (concept guarantees this works)
        // ... tracing logic ...

        // Unregister roots
        for (size_t i = 0; i < root_count; ++i) {
            gc.remove_root(&roots[i]);
        }
    }

    /**
     * @brief Function requiring generational GC
     */
    template<GenerationalGC GC>
    void assign_with_barrier(GC& gc, void* owner, void** field, void* new_value) {
        void* old_value = *field;
        *field = new_value;

        // Concept guarantees write_barrier exists
        gc.write_barrier(owner, old_value, new_value);
    }
}

} // namespace memory
