#pragma once

#include "../primitive_base.h"

/**
 * @file primitive_lists.h
 * @brief List and pair primitives (R5RS Section 6.3)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 *
 * Implements list and pair operations including:
 * - Basic operations: cons, car, cdr, set-car!, set-cdr!
 * - Predicates: pair?, list?, null?
 * - List operations: list, length, append, reverse, list-tail, list-ref
 * - Search operations: member, memq, memv, assoc, assq, assv
 * - Accessors: second, third, and CXR combinations (cadr, caddr, etc.)
 *
 * **Special CXR Handling:**
 * CXR operations like cadr, caddr, cadddr are handled by a single Primitive
 * that reads the operation name right-to-left ('a' for car, 'd' for cdr).
 */

/**
 * @brief List and pair primitives (R5RS Section 6.3)
 *
 * Handles all list and pair operations defined in R5RS.
 *
 * **Example Usage:**
 * @code
 * (cons 1 2)            ; => (1 . 2)
 * (car '(a b c))        ; => a
 * (cdr '(a b c))        ; => (b c)
 * (list 1 2 3)          ; => (1 2 3)
 * (append '(a) '(b))    ; => (a b)
 * (member 'b '(a b c))  ; => (b c)
 * (cadr '(a b c))       ; => b (same as (car (cdr ...)))
 * @endcode
 */
class PrimitiveLists : public PrimitiveBase {
public:
    /**
     * @brief Primitive IDs for list operations
     */
    enum ID {
        PAIRQ = 0,      ///< pair? predicate
        LISTQ = 1,      ///< list? predicate
        CXR = 2,        ///< CXR operations (cadr, caddr, etc.) - uses m_name
        CONS = 3,       ///< cons constructor
        CAR = 4,        ///< car accessor
        CDR = 5,        ///< cdr accessor
        SETCAR = 6,     ///< set-car! mutator (extension)
        SETCDR = 7,     ///< set-cdr! mutator (extension)
        SECOND = 8,     ///< second accessor (cadr)
        THIRD = 9,      ///< third accessor (caddr)
        NULLQ = 10,     ///< null? predicate
        LIST = 11,      ///< list constructor
        LENGTH = 12,    ///< length function
        APPEND = 13,    ///< append function
        REVERSE = 14,   ///< reverse function
        ListTAIL = 15,  ///< list-tail function
        ListREF = 16,   ///< list-ref function
        MEMQ = 17,      ///< memq search (eq? comparison)
        MEMV = 18,      ///< memv search (eqv? comparison)
        MEMBER = 19,    ///< member search (equal? comparison)
        ASSQ = 20,      ///< assq search (eq? comparison)
        ASSV = 21,      ///< assv search (eqv? comparison)
        ASSOC = 22,     ///< assoc search (equal? comparison)
        MAKELIST = 23,  ///< make-list function (R7RS)
        LISTCOPY = 24,  ///< list-copy function (R7RS)
        LISTSET = 25    ///< list-set! function (R7RS)
    };

    /**
     * @brief Construct list Primitive
     * @param id Which list Primitive
     * @param minArgs Minimum arguments (varies by Primitive)
     * @param maxArgs Maximum arguments (varies by Primitive)
     */
    PrimitiveLists(ID id, int minArgs, int maxArgs);

    /**
     * @brief Apply list Primitive to arguments
     * @param eval Evaluator for nested evaluation
     * @param args Argument list
     * @param env Environment
     * @return Result of list operation
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all list primitives into environment
     * @param env Environment to install primitives into
     */
    static void install(Environment* env);

private:
    ID m_id;  ///< Which list Primitive this instance represents
};
