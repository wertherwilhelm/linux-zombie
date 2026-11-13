#pragma once

#include "gc_tracer.h"
#include "gc_object.h"
#include <cstddef>
#include <type_traits>

/**
 * TraceFn - Type-erased function pointer for tracing
 *
 * Stored in allocation headers to allow virtual-free tracing.
 * Points to Trace<T>::trace for the specific type T.
 */
using TraceFn = void(*)(Tracer& tracer, void* obj);

/**
 * Trace<T> - Type trait for GC tracing (virtual-free)
 *
 * Types implement this trait to support GC traversal without virtuals.
 * Default implementation bridges to virtual trace_references for gc_object-derived types.
 *
 * Example custom implementation:
 *
 *   struct MyType {
 *       gc_object* field1;
 *       gc_object* field2;
 *   };
 *
 *   template<>
 *   struct Trace<MyType> {
 *       static void trace(Tracer& tracer, MyType* obj) {
 *           if (obj) {
 *               tracer.visit_field(obj->field1);  // Use visit_field for moving GC support
 *               tracer.visit_field(obj->field2);
 *           }
 *       }
 *   };
 */
template<typename T>
struct Trace {
    /**
     * Default: bridge to virtual trace_references if T derives from gc_object,
     * otherwise no-op for POD types.
     */
    static void trace(Tracer& tracer, T* obj) {
        if constexpr (std::is_base_of<gc_object, T>::value) {
            // T derives from gc_object - call virtual trace_references
            if (obj) {
                obj->trace_references(tracer);
            }
        } else {
            // POD type - no references to trace
            (void)tracer;
            (void)obj;
        }
    }
};

/**
 * Trace<gc_object> - Specialization for gc_object hierarchy
 *
 * Bridges trait-based tracing to virtual trace_references(Tracer&).
 * Allows any gc_object subclass to be traced via its header TraceFn.
 */
template<>
struct Trace<gc_object> {
    static void trace(Tracer& tracer, gc_object* obj);
};

/**
 * Helper: Get TraceFn for a type T
 *
 * Returns type-erased function pointer to Trace<T>::trace
 */
template<typename T>
constexpr TraceFn get_trace_fn() {
    return [](Tracer& tracer, void* obj) {
        Trace<T>::trace(tracer, static_cast<T*>(obj));
    };
}
