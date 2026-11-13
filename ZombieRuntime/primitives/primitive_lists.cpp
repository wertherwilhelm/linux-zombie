#include "pch.h"
#include "../pch.h"
#include "primitive_lists.h"
#include "../Base.h"
#include "../cons_cell.h"
#include "../atom.h"
#include "../numeric_runtime.h"

/**
 * @file primitive_lists.cpp
 * @brief Implementation of list and pair primitives (R5RS Section 6.3)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 */

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Helper for member/assoc family of functions
 * @param obj Object to search for
 * @param List List to search in
 * @param m 'm' for member, 'a' for assoc
 * @param eq 'q' for eq?, 'v' for eqv?, ' ' for equal?
 * @return Tail of list starting with found element, or #f
 */
static Object* member_assoc(Object *obj, Object *List, char m, char eq) {
    while (!List->is_atom()) {
        Object *target = (m == 'm') ? first(List) : first(first(List));
        bool found;
        switch (eq) {
            case 'q':
                found = ::eq(target, obj);
                break;
            case 'v':
                found = eqv(target, obj);
                break;
            case ' ':
                found = equal(target, obj);
                break;
            default:
                warn("Bad option to member_assoc:" + eq);
                return false_obj;
        }
        if (found) return (m == 'm') ? List : first(List);
        List = rest(List);
    }
    return false_obj;  // Return #f when not found (R5RS compliant)
}

// ============================================================================
// Constructor
// ============================================================================

PrimitiveLists::PrimitiveLists(ID id, int minArgs, int maxArgs)
    : PrimitiveBase(minArgs, maxArgs), m_id(id) {
    // Set Primitive name based on ID
    switch (id) {
        case PAIRQ:    m_name = "pair?"; break;
        case LISTQ:    m_name = "list?"; break;
        case CXR:      m_name = "cxr"; break;  // Will be overridden with specific name
        case CONS:     m_name = "cons"; break;
        case CAR:      m_name = "car"; break;
        case CDR:      m_name = "cdr"; break;
        case SETCAR:   m_name = "set-car!"; break;
        case SETCDR:   m_name = "set-cdr!"; break;
        case SECOND:   m_name = "second"; break;
        case THIRD:    m_name = "third"; break;
        case NULLQ:    m_name = "null?"; break;
        case LIST:     m_name = "list"; break;
        case LENGTH:   m_name = "length"; break;
        case APPEND:   m_name = "append"; break;
        case REVERSE:  m_name = "reverse"; break;
        case ListTAIL: m_name = "list-tail"; break;
        case ListREF:  m_name = "list-ref"; break;
        case MEMQ:     m_name = "memq"; break;
        case MEMV:     m_name = "memv"; break;
        case MEMBER:   m_name = "member"; break;
        case ASSQ:     m_name = "assq"; break;
        case ASSV:     m_name = "assv"; break;
        case ASSOC:    m_name = "assoc"; break;
        case MAKELIST: m_name = "make-list"; break;
        case LISTCOPY: m_name = "list-copy"; break;
        case LISTSET:  m_name = "list-set!"; break;
    }
}

// ============================================================================
// Primitive Application
// ============================================================================

Object* PrimitiveLists::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Arity checking (inherited from PrimitiveBase)
    if (!check_arity(args)) {
        return arity_error("");
    }

    // Extract common arguments
    Object* x = (args && !args->is_nil()) ? first(args) : nil;
    Object* y = (args && !args->is_nil() && rest(args) && !rest(args)->is_nil()) ? second(args) : nil;

    // Dispatch based on Primitive ID
    switch (m_id) {
        case PAIRQ:
            // (pair? obj) - returns #t if obj is a pair
            return truth(!x->is_atom());

        case LISTQ:
            // (list? obj) - returns #t if obj is a proper list
            return truth(::is_list(x));

        case CXR: {
            // CXR operations (cadr, caddr, etc.)
            // Pattern is read right to left: "cadr" means cdr then car
            // Loop from second-to-last char (index length-2) down to index 1
            for (int i = static_cast<int>(m_name.length()) - 2; i >= 1; i--) {
                // If we hit nil or an atom, return nil (accessing beyond list bounds)
                if (x == nil || x->is_nil() || x->is_atom()) {
                    return nil;
                }
                x = (m_name[i] == 'a') ? first(x) : rest(x);
            }
            return x;
        }

        case CONS:
            // (cons obj1 obj2) - construct a pair
            return cons(x, y);

        case CAR:
            // (car pair) - return first element
            return first(x);

        case CDR:
            // (cdr pair) - return rest
            return rest(x);

        case SETCAR:
            // (set-car! pair obj) - mutate first element
            return set_first(x, y);

        case SETCDR:
            // (set-cdr! pair obj) - mutate rest
            return set_rest(x, y);

        case SECOND:
            // (second list) - return second element (cadr)
            return second(x);

        case THIRD:
            // (third list) - return third element (caddr)
            return third(x);

        case NULLQ:
            // (null? obj) - returns #t if obj is nil
            return truth(x == NULL || x == nil || x->is_nil());

        case LIST:
            // (list obj ...) - return list of arguments
            return args;

        case LENGTH: {
            // (length list) - return length of list
            if (x == NULL || x->is_nil()) {
                return numeric::make_integer(0);
            }
            return numeric::make_integer(length(x));
        }

        case APPEND: {
            // (append list ...) - concatenate lists
            if (args == NULL || args->is_nil()) {
                return nil;
            }
            // Start with the first list
            Object* result = first(args);
            Object* remaining = rest(args);

            // Append each subsequent list
            while (remaining != NULL && !remaining->is_nil()) {
                result = append(result, first(remaining));
                remaining = rest(remaining);
            }
            return result;
        }

        case REVERSE: {
            // (reverse list) - reverse a list
            if (x == NULL || x->is_nil()) {
                return nil;
            }
            return reverse(x);
        }

        case ListTAIL: {
            // (list-tail list k) - return list after k elements
            int k = (int)(((a_int*)y)->value());
            for (; k > 0; k--) {
                x = rest(x);
            }
            return x;
        }

        case ListREF: {
            // (list-ref list k) - return the kth element of list (0-indexed)
            if (x == NULL || x->is_nil()) {
                return error("list-ref: list is empty");
            }
            if (y == NULL || y->is_nil()) {
                return error("list-ref: index required");
            }

            int k = (int)num(y);
            if (k < 0) {
                return error("list-ref: index must be non-negative");
            }

            Object* current = x;
            for (int i = 0; i < k; i++) {
                if (current == NULL || current->is_nil()) {
                    return error("list-ref: index out of bounds");
                }
                current = rest(current);
            }

            if (current == NULL || current->is_nil()) {
                return error("list-ref: index out of bounds");
            }

            return first(current);
        }

        case MEMQ:
            // (memq obj list) - search using eq?
            return member_assoc(x, y, 'm', 'q');

        case MEMV:
            // (memv obj list) - search using eqv?
            return member_assoc(x, y, 'm', 'v');

        case MEMBER:
            // (member obj list) - search using equal?
            return member_assoc(x, y, 'm', ' ');

        case ASSQ:
            // (assq obj alist) - association list search using eq?
            return member_assoc(x, y, 'a', 'q');

        case ASSV:
            // (assv obj alist) - association list search using eqv?
            return member_assoc(x, y, 'a', 'v');

        case ASSOC:
            // (assoc obj alist) - association list search using equal?
            return member_assoc(x, y, 'a', ' ');

        case MAKELIST: {
            // (make-list k [fill]) - create list of k elements with optional fill value
            // R7RS Section 6.4
            if (x == NULL || x->is_nil()) {
                return error("make-list: requires integer argument");
            }

            int k = (int)num(x);
            if (k < 0) {
                return error("make-list: count must be non-negative");
            }

            // Optional fill value (defaults to nil)
            Object* fill = (y && !y->is_nil()) ? y : nil;

            // Build list of k elements
            Object* result = nil;
            for (int i = 0; i < k; i++) {
                result = cons(fill, result);
            }
            return result;
        }

        case LISTCOPY: {
            // (list-copy list) - create shallow copy of list
            // R7RS Section 6.4
            if (x == NULL || x->is_nil()) {
                return nil;
            }

            // Build new list with same elements
            Object* result = nil;
            Object* tail = nil;

            Object* current = x;
            while (current && !current->is_nil()) {
                if (!current->pair()) {
                    // Improper list: copy the dotted tail
                    if (tail) {
                        set_rest(tail, current);
                    } else {
                        result = current;
                    }
                    break;
                }

                Object* element = first(current);
                Object* new_cell = cons(element, nil);

                if (result == nil) {
                    result = new_cell;
                    tail = new_cell;
                } else {
                    set_rest(tail, new_cell);
                    tail = new_cell;
                }

                current = rest(current);
            }

            return result;
        }

        case LISTSET: {
            // (list-set! list k obj) - mutate kth element of list
            // R7RS Section 6.4
            if (x == NULL || x->is_nil()) {
                return error("list-set!: list is empty");
            }
            if (y == NULL || y->is_nil()) {
                return error("list-set!: index required");
            }

            int k = (int)num(y);
            if (k < 0) {
                return error("list-set!: index must be non-negative");
            }

            // Get third argument (the new value)
            Object* obj = (args && !args->is_nil() &&
                          rest(args) && !rest(args)->is_nil() &&
                          rest(rest(args)) && !rest(rest(args))->is_nil())
                ? first(rest(rest(args)))
                : nil;

            // Find the kth element and mutate it
            Object* current = x;
            for (int i = 0; i < k; i++) {
                if (current == NULL || current->is_nil() || !current->pair()) {
                    return error("list-set!: index out of bounds");
                }
                current = rest(current);
            }

            if (current == NULL || current->is_nil() || !current->pair()) {
                return error("list-set!: index out of bounds");
            }

            // Mutate the car of the kth cons cell
            set_first(current, obj);
            return nil;  // R7RS: list-set! returns unspecified value
        }

        default:
            return error("Unknown list Primitive");
    }
}

// ============================================================================
// Registration
// ============================================================================

void PrimitiveLists::install(Environment* env) {
    const int n = 999999;  // Unlimited arguments marker

    // GC Safety: Use macro to protect name string before allocating primitive
    #define DEF(name, ...) do { \
        gc_root<a_string> _n(gc_new<a_string>(name)); \
        env->define(_n.get(), gc_new<PrimitiveLists>(__VA_ARGS__)); \
    } while(0)

    // Predicates
    DEF("pair?",  PAIRQ, 1, 1);
    DEF("list?",  LISTQ, 1, 1);
    DEF("null?",  NULLQ, 1, 1);

    // Basic constructors and accessors
    DEF("cons",   CONS, 2, 2);
    DEF("car",    CAR, 1, 1);
    DEF("cdr",    CDR, 1, 1);
    DEF("first",  CAR, 1, 1);  // Alias for car
    DEF("rest",   CDR, 1, 1);  // Alias for cdr
    DEF("set-car!", SETCAR, 2, 2);
    DEF("set-cdr!", SETCDR, 2, 2);

    // List operations
    DEF("list",    LIST, 0, n);
    DEF("length",  LENGTH, 1, 1);
    DEF("append",  APPEND, 0, n);
    DEF("reverse", REVERSE, 1, 1);
    DEF("list-tail", ListTAIL, 2, 2);
    DEF("list-ref", ListREF, 2, 2);

    // Additional accessors
    DEF("second",  SECOND, 1, 1);
    DEF("third",   THIRD, 1, 1);

    #undef DEF

    // CXR operations (cadr, caddr, etc.)
    // These need special handling because they use the name to determine the operation
    // GC Safety: Protect both primitive and name string during allocation
    auto register_cxr = [env](const char* name) {
        gc_root<PrimitiveLists> prim(gc_new<PrimitiveLists>(PrimitiveLists::CXR, 1, 1));
        prim.get()->m_name = name;  // Override name for CXR pattern matching
        gc_root<a_string> name_str(gc_new<a_string>(name));
        env->define(name_str.get(), prim.get());
    };

    // Two-level CXR (cXr)
    register_cxr("caar");
    register_cxr("cadr");
    register_cxr("cdar");
    register_cxr("cddr");

    // Three-level CXR (cXXr)
    register_cxr("caaar");
    register_cxr("caadr");
    register_cxr("cadar");
    register_cxr("caddr");
    register_cxr("cdaar");
    register_cxr("cdadr");
    register_cxr("cddar");
    register_cxr("cdddr");

    // Four-level CXR (cXXXr)
    register_cxr("caaaar");
    register_cxr("caaadr");
    register_cxr("caadar");
    register_cxr("caaddr");
    register_cxr("cadaar");
    register_cxr("cadadr");
    register_cxr("caddar");
    register_cxr("cadddr");
    register_cxr("cdaaar");
    register_cxr("cdaadr");
    register_cxr("cdadar");
    register_cxr("cdaddr");
    register_cxr("cddaar");
    register_cxr("cddadr");
    register_cxr("cdddar");
    register_cxr("cddddr");

    // Search operations - use inline gc_root for remaining primitives
    #define DEF(name, ...) do { \
        gc_root<a_string> _n(gc_new<a_string>(name)); \
        env->define(_n.get(), gc_new<PrimitiveLists>(__VA_ARGS__)); \
    } while(0)

    DEF("memq",   MEMQ, 2, 2);
    DEF("memv",   MEMV, 2, 2);
    DEF("member", MEMBER, 2, 2);
    DEF("assq",   ASSQ, 2, 2);
    DEF("assv",   ASSV, 2, 2);
    DEF("assoc",  ASSOC, 2, 2);

    // R7RS additions
    DEF("make-list",  MAKELIST, 1, 2);
    DEF("list-copy",  LISTCOPY, 1, 1);
    DEF("list-set!",  LISTSET, 3, 3);

    #undef DEF
}
