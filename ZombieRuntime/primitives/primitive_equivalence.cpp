#include "pch.h"
#include "../pch.h"
#include "primitive_equivalence.h"
#include "../Base.h"
#include "../atom.h"

/**
 * @file primitive_equivalence.cpp
 * @brief Implementation of equivalence predicates (R5RS Section 6.2)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 */

// ============================================================================
// Constructor
// ============================================================================

PrimitiveEquivalence::PrimitiveEquivalence(ID id)
    : PrimitiveBase(2, 2),  // All equivalence predicates take exactly 2 arguments
      m_id(id) {
    // Set Primitive name based on ID
    switch (id) {
        case EQQ:
            m_name = "eq?";
            break;

        case EQVQ:
            m_name = "eqv?";
            break;

        case EQUALQ:
            m_name = "equal?";
            break;
    }
}

// ============================================================================
// Primitive Application
// ============================================================================

Object* PrimitiveEquivalence::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Arity checking (inherited from PrimitiveBase)
    // All equivalence predicates require exactly 2 arguments
    if (!check_arity(args)) {
        return arity_error("");
    }

    // Extract the two arguments
    Object* x = first(args);
    Object* y = second(args);

    // Dispatch based on this group's Primitive ID
    switch (m_id) {
        case EQQ:
            // (eq? obj1 obj2)
            // Returns #t if obj1 and obj2 are the same Object (pointer equality)
            // R5RS: eq? is the finest equivalence predicate
            // Symbols are interned (as of 2026-01-11), so eq? works correctly with symbols
            // All symbols with the same name share the same Object identity
            return truth(x == y);

        case EQVQ:
            // (eqv? obj1 obj2)
            // Returns #t if obj1 and obj2 are operationally equivalent
            // R5RS: like eq? but handles numbers and characters specially
            // Uses helper function from Base.h
            return truth(eqv(x, y));

        case EQUALQ:
            // (equal? obj1 obj2)
            // Returns #t if obj1 and obj2 are structurally equal
            // R5RS: the coarsest equivalence predicate (recursive comparison)
            // Uses helper function from Base.h
            return truth(equal(x, y));

        default:
            // Should never reach here
            return error("Unknown equivalence Primitive");
    }
}

// ============================================================================
// Registration
// ============================================================================

void PrimitiveEquivalence::install(Environment* env) {
    // Register all equivalence primitives
    // GC Safety: Protect name string before allocating primitive
    gc_root<a_string> name_eq(gc_new<a_string>("eq?"));
    env->define(name_eq.get(), gc_new<PrimitiveEquivalence>(EQQ));

    gc_root<a_string> name_eqv(gc_new<a_string>("eqv?"));
    env->define(name_eqv.get(), gc_new<PrimitiveEquivalence>(EQVQ));

    gc_root<a_string> name_equal(gc_new<a_string>("equal?"));
    env->define(name_equal.get(), gc_new<PrimitiveEquivalence>(EQUALQ));
}
