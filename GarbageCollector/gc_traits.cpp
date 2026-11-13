#include "pch.h"
#include "gc_traits.h"
#include "gc_object.h"
#include "gc_tracer.h"

/**
 * Trace<gc_object> implementation - Bridges trait-based to virtual tracing
 *
 * This allows any gc_object* to be traced via its gc_header's TraceFn.
 * The TraceFn will call this, which in turn calls the virtual trace_references().
 */
void Trace<gc_object>::trace(Tracer& tracer, gc_object* obj) {
    if (obj) {
        // Call the virtual trace_references method
        obj->trace_references(tracer);
    }
}
