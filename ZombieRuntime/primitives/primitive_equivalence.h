#pragma once

#include "../primitive_base.h"

/**
 * @file primitive_equivalence.h
 * @brief Equivalence predicates (R5RS Section 6.2)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 *
 * Implements equivalence predicates:
 * - eq?: Tests whether two objects are the same (pointer equality)
 * - eqv?: Tests whether two objects are equivalent (like eq? but handles numbers)
 * - equal?: Tests whether two objects are structurally equal (recursive comparison)
 *
 * **R5RS Specification:**
 * Section 6.2 defines three levels of equivalence:
 * - eq? is the finest (pointer equality)
 * - eqv? handles numbers and characters specially
 * - equal? is the coarsest (structural equality)
 */

/**
 * @brief Equivalence predicates (R5RS Section 6.2)
 *
 * Handles three levels of equivalence testing:
 * - `eq?`: Pointer equality (same Object in memory)
 * - `eqv?`: Operational equivalence (same Object or equivalent number/char)
 * - `equal?`: Structural equality (recursively equal contents)
 *
 * **Example Usage:**
 * @code
 * (eq? 'a 'a)           ; => #t  (same symbol)
 * (eq? '(a) '(a))       ; => #f  (different cons cells)
 * (eqv? 3 3)            ; => #t  (same number)
 * (eqv? 3 3.0)          ; => #f  (different types)
 * (equal? '(a b) '(a b)); => #t  (same structure)
 * (equal? 3 3)          ; => #t  (equal numbers)
 * @endcode
 */
class PrimitiveEquivalence : public PrimitiveBase {
public:
    /**
     * @brief Primitive IDs for equivalence operations
     */
    enum ID {
        EQQ = 0,     ///< Pointer equality (eq?)
        EQVQ = 1,    ///< Operational equivalence (eqv?)
        EQUALQ = 2   ///< Structural equality (equal?)
    };

    /**
     * @brief Construct equivalence Primitive
     * @param id Which equivalence Primitive (EQQ, EQVQ, or EQUALQ)
     */
    PrimitiveEquivalence(ID id);

    /**
     * @brief Apply equivalence Primitive to arguments
     * @param eval Evaluator for nested evaluation (not used by equivalence primitives)
     * @param args Argument list (expected: 2 arguments)
     * @param env Environment (not used by equivalence primitives)
     * @return #t if objects are equivalent according to the predicate, #f otherwise
     * @note All equivalence primitives require exactly 2 arguments
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all equivalence primitives into environment
     * @param env Environment to install primitives into
     *
     * Registers:
     * - "eq?" → PrimitiveEquivalence(EQQ)
     * - "eqv?" → PrimitiveEquivalence(EQVQ)
     * - "equal?" → PrimitiveEquivalence(EQUALQ)
     */
    static void install(Environment* env);

private:
    ID m_id;  ///< Which equivalence Primitive this instance represents
};
