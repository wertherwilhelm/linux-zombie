#pragma once

#include "../primitive_base.h"

/**
 * @file primitive_vectors.h
 * @brief Vector primitives (R5RS Section 6.8)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 *
 * Implements vector operations including:
 * - Predicates: vector?
 * - Construction: make-vector, vector
 * - Access: vector-length, vector-ref, vector-set!
 * - Operations: vector-fill!
 * - Conversions: vector->list, list->vector
 */

/**
 * @brief Vector primitives (R5RS Section 6.8)
 *
 * Handles all vector operations.
 *
 * **Example Usage:**
 * @code
 * (make-vector 3)              ; => #(nil nil nil)
 * (make-vector 3 'a)           ; => #(a a a)
 * (vector 'a 'b 'c)            ; => #(a b c)
 * (vector-length #(1 2 3))     ; => 3
 * (vector-ref #(a b c) 1)      ; => b
 * (vector->list #(1 2 3))      ; => (1 2 3)
 * (list->vector '(1 2 3))      ; => #(1 2 3)
 * @endcode
 */
class PrimitiveVectors : public PrimitiveBase {
public:
    /**
     * @brief Primitive IDs for vector operations
     */
    enum ID {
        VECTORQ = 0,        ///< vector? predicate
        MAKEVECTOR = 1,     ///< make-vector
        VECTOR = 2,         ///< vector (construct from elements)
        VECTORLENGTH = 3,   ///< vector-length
        VECTORREF = 4,      ///< vector-ref
        VECTORSET = 5,      ///< vector-set!
        VECTORFILL = 6,     ///< vector-fill!
        VECTORTOLIST = 7,   ///< vector->list
        LISTTOVECTOR = 8,   ///< list->vector
        VECTORCOPY = 9,     ///< vector-copy (R7RS)
        VECTORAPPEND = 10,  ///< vector-append (R7RS)
        VECTORMAP = 11,     ///< vector-map (R7RS)
        VECTORFOREACH = 12, ///< vector-for-each (R7RS)
        STRINGTOVECTOR = 13,///< string->vector (R7RS)
        VECTORTOSTRING = 14 ///< vector->string (R7RS)
    };

    /**
     * @brief Construct vector Primitive
     * @param id Which vector Primitive
     * @param minArgs Minimum arguments
     * @param maxArgs Maximum arguments
     */
    PrimitiveVectors(ID id, int minArgs, int maxArgs);

    /**
     * @brief Apply vector Primitive to arguments
     * @param eval Evaluator
     * @param args Argument list
     * @param env Environment
     * @return Result of vector operation
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all vector primitives into environment
     * @param env Environment to install primitives into
     */
    static void install(Environment* env);

private:
    ID m_id;  ///< Which vector Primitive this instance represents
};
