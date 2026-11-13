#pragma once

#include "../primitive_base.h"

/**
 * @file primitive_booleans.h
 * @brief Boolean primitives (R5RS Section 6.1)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 *
 * Implements boolean-related primitives:
 * - not: Logical negation
 * - boolean?: Type predicate for booleans
 *
 * **R5RS Specification:**
 * Section 6.1 Booleans states that #t and #f are the only boolean values.
 * The not Procedure returns #t if obj is false and #f otherwise.
 */

/**
 * @brief Boolean primitives (R5RS Section 6.1)
 *
 * Handles boolean-related operations:
 * - `not`: Logical negation (returns #t if argument is #f, #f otherwise)
 * - `boolean?`: Type predicate (returns #t only for #t and #f)
 *
 * **Example Usage:**
 * @code
 * (not #f)      ; => #t
 * (not #t)      ; => #f
 * (not 0)       ; => #f  (R5RS: only #f is false)
 * (boolean? #t) ; => #t
 * (boolean? #f) ; => #t
 * (boolean? 0)  ; => #f
 * @endcode
 */
class PrimitiveBooleans : public PrimitiveBase {
public:
    /**
     * @brief Primitive IDs for boolean operations
     */
    enum ID {
        NOT = 0,      ///< Logical negation (not)
        BOOLEANQ = 1  ///< Boolean type predicate (boolean?)
    };

    /**
     * @brief Construct boolean Primitive
     * @param id Which boolean Primitive (NOT or BOOLEANQ)
     */
    PrimitiveBooleans(ID id);

    /**
     * @brief Apply boolean Primitive to arguments
     * @param eval Evaluator for nested evaluation (not used by boolean primitives)
     * @param args Argument list (expected: 1 argument)
     * @param env Environment (not used by boolean primitives)
     * @return Result of boolean operation
     * @note Both primitives require exactly 1 argument
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all boolean primitives into environment
     * @param env Environment to install primitives into
     *
     * Registers:
     * - "not" → PrimitiveBooleans(NOT)
     * - "boolean?" → PrimitiveBooleans(BOOLEANQ)
     */
    static void install(Environment* env);

private:
    ID m_id;  ///< Which boolean Primitive this instance represents
};
