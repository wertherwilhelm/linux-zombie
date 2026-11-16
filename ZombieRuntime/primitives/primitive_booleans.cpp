#include "zombie_runtime_types.h"
#include "zombie_runtime_types.h"
#include "primitive_booleans.h"
#include "../Base.h"
#include "../constants.h"
#include "../atom.h"

/**
 * @file primitive_booleans.cpp
 * @brief Implementation of boolean primitives (R5RS Section 6.1)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 */

// ============================================================================
// Constructor
// ============================================================================

PrimitiveBooleans::PrimitiveBooleans(ID id)
    : PrimitiveBase(id == BOOLEANEQ ? 2 : 1,        // boolean=? is variadic (2+);
                    id == BOOLEANEQ ? 999999 : 1),  // the others take exactly one
      m_id(id) {
    // Set Primitive name based on ID
    switch (id) {
        case NOT:
            m_name = "not";
            break;

        case BOOLEANQ:
            m_name = "boolean?";
            break;

        case PROCEDUREQ:
            m_name = "procedure?";
            break;

        case BOOLEANEQ:
            m_name = "boolean=?";
            break;
    }
}

// ============================================================================
// Primitive Application
// ============================================================================

Object* PrimitiveBooleans::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Arity checking (inherited from PrimitiveBase)
    // Both primitives require exactly 1 argument
    if (!check_arity(args)) {
        return arity_error("");
    }

    // Extract the single argument
    Object* x = first(args);

    // Dispatch based on this group's Primitive ID
    switch (m_id) {
        case NOT:
            // (not obj)
            // Returns #t if obj is false, #f otherwise
            // R5RS: Only #f is false, everything else is true
            return truth(x == false_obj);

        case BOOLEANQ:
            // (boolean? obj)
            // Returns #t if obj is #t or #f, #f otherwise
            // R5RS: Only #t and #f are booleans
            // Use helper function from constants.h
            return truth(is_boolean(x));

        case PROCEDUREQ:
            // (procedure? obj)
            // Returns #t if obj is applicable: a closure, primitive, or
            // continuation (all derive from Procedure).
            return truth(dynamic_cast<zombie::runtime::Procedure*>(x) != nullptr);

        case BOOLEANEQ: {
            // (boolean=? b1 b2 ...) - #t iff all arguments are the same boolean
            // (R7RS 6.3). #t / #f are singletons, so identity compares values.
            if (!is_boolean(x)) {
                return static_cast<Object*>(false_obj);
            }
            for (Object* p = rest(args); p && !p->is_nil(); p = rest(p)) {
                Object* e = first(p);
                if (!is_boolean(e) || e != x) {
                    return static_cast<Object*>(false_obj);
                }
            }
            return t;
        }

        default:
            // Should never reach here
            return error("Unknown boolean Primitive");
    }
}

// ============================================================================
// Registration
// ============================================================================

void PrimitiveBooleans::install(Environment* env) {
    // Register all boolean primitives
    // GC Safety: Protect name string before allocating primitive
    gc_root<a_string> name_not(gc_new<a_string>("not"));
    env->define(name_not.get(), gc_new<PrimitiveBooleans>(NOT));

    gc_root<a_string> name_boolq(gc_new<a_string>("boolean?"));
    env->define(name_boolq.get(), gc_new<PrimitiveBooleans>(BOOLEANQ));

    gc_root<a_string> name_procq(gc_new<a_string>("procedure?"));
    env->define(name_procq.get(), gc_new<PrimitiveBooleans>(PROCEDUREQ));

    gc_root<a_string> name_booleq(gc_new<a_string>("boolean=?"));
    env->define(name_booleq.get(), gc_new<PrimitiveBooleans>(BOOLEANEQ));
}
