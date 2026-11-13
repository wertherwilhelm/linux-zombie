#pragma once

#include "../primitive_base.h"

/**
 * @file primitive_control.h
 * @brief Control primitives (R5RS Section 6.9)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 *
 * Implements control flow operations including:
 * - Evaluation: eval, apply
 * - Predicates: Procedure?
 * - Higher-order: map, for-each, filter, fold-left, fold-right
 * - Promises: force
 * - Multiple values: values, call-with-values
 * - Dynamic extent: dynamic-wind
 * - Continuations: call/cc
 */

/**
 * @brief Control primitives (R5RS Section 6.9)
 *
 * Handles all control flow operations.
 *
 * **Example Usage:**
 * @code
 * (eval '(+ 1 2))              ; => 3
 * (apply + '(1 2 3))           ; => 6
 * (map (lambda (x) (* x 2)) '(1 2 3))  ; => (2 4 6)
 * (filter even? '(1 2 3 4))    ; => (2 4)
 * (fold-left + 0 '(1 2 3))     ; => 6
 * (force (delay (+ 1 2)))      ; => 3
 * @endcode
 */
class PrimitiveControl : public PrimitiveBase {
public:
    /**
     * @brief Primitive IDs for control operations
     */
    enum ID {
        EVAL = 0,           ///< eval
        PROCEDUREQ = 1,     ///< Procedure? predicate
        APPLY = 2,          ///< apply
        MAP = 3,            ///< map
        FOREACH = 4,        ///< for-each
        FILTER = 5,         ///< filter
        FOLDLEFT = 6,       ///< fold-left
        FOLDRIGHT = 7,      ///< fold-right
        FORCE = 8,          ///< force
        VALUES = 9,         ///< values
        CALLWITHVALUES = 10,///< call-with-values
        DYNAMICWIND = 11,   ///< dynamic-wind
        CALLCC = 12         ///< call/cc (call-with-current-Continuation)
    };

    /**
     * @brief Construct control Primitive
     * @param id Which control Primitive
     * @param minArgs Minimum arguments
     * @param maxArgs Maximum arguments
     */
    PrimitiveControl(ID id, int minArgs, int maxArgs);

    /**
     * @brief Apply control Primitive to arguments
     * @param eval Evaluator
     * @param args Argument list
     * @param env Environment
     * @return Result of control operation
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all control primitives into environment
     * @param env Environment to install primitives into
     */
    static void install(Environment* env);

private:
    ID m_id;  ///< Which control Primitive this instance represents
};
