#include "pch.h"
#include "../pch.h"
#include "primitive_control.h"
#include "../Base.h"
#include "../atom.h"
#include "../procedure.h"
#include "../values_object.h"
#include "../cons_cell.h"
#include "../i_evaluator.h"
#include "../escape_continuation.h"
#include "gc_ptr.h"  // For gc_root
#include <stdexcept>

/**
 * @file primitive_control.cpp
 * @brief Implementation of control primitives (R5RS Section 6.9)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 */

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Check if Object is a Procedure
 * @param x Object to check
 * @return True if Object is a Procedure (Primitive, Closure, or Macro)
 */
static bool is_procedure(Object* x) {
    if (x == NULL || x == nil) {
        return false;
    }
    Base* b = dynamic_cast<Base*>(x);
    if (b == NULL) {
        return false;
    }
    int type = b->instance_of();
    return (type == static_cast<int>(Base::atype::PROCEDURE) ||
            type == static_cast<int>(Base::atype::PRIMITIVE) ||
            type == static_cast<int>(Base::atype::CLOSURE) ||
            type == static_cast<int>(Base::atype::MACRO));
}

/**
 * @brief Append two lists for apply (properly builds new list)
 * @param list1 First list
 * @param list2 Second list
 * @return New list with list1 elements followed by list2 elements
 */
static Object* append_lists_for_apply(Object* list1, Object* list2) {
    // If first list is nil, return second list
    if (list1 == NULL || list1 == nil) {
        return list2;
    }

    // Build a new list by recursively consing elements
    Object* first_elem = first(list1);
    Object* rest_of_list1 = rest(list1);

    if (rest_of_list1 == NULL || rest_of_list1 == nil) {
        // Last element of list1 - cons it with list2
        return gc_new<ConsCell>(first_elem, list2);
    } else {
        // Recursively append - protect intermediate result
        Object* rest_result = append_lists_for_apply(rest_of_list1, list2);
        gc_root<Object> rest_guard(rest_result);
        return gc_new<ConsCell>(first_elem, rest_result);
    }
}

// ============================================================================
// Constructor
// ============================================================================

PrimitiveControl::PrimitiveControl(ID id, int minArgs, int maxArgs)
    : PrimitiveBase(minArgs, maxArgs), m_id(id) {
    // Set Primitive name based on ID
    switch (id) {
        case EVAL:           m_name = "eval"; break;
        case PROCEDUREQ:     m_name = "Procedure?"; break;
        case APPLY:          m_name = "apply"; break;
        case MAP:            m_name = "map"; break;
        case FOREACH:        m_name = "for-each"; break;
        case FILTER:         m_name = "filter"; break;
        case FOLDLEFT:       m_name = "fold-left"; break;
        case FOLDRIGHT:      m_name = "fold-right"; break;
        case FORCE:          m_name = "force"; break;
        case VALUES:         m_name = "values"; break;
        case CALLWITHVALUES: m_name = "call-with-values"; break;
        case DYNAMICWIND:    m_name = "dynamic-wind"; break;
        case CALLCC:         m_name = "call/cc"; break;
    }
}

// ============================================================================
// Primitive Application
// ============================================================================

Object* PrimitiveControl::apply(IEvaluator* evaluator, Object* args, Environment* env) {
    // Arity checking
    if (!check_arity(args)) {
        return arity_error("");
    }

    // Extract arguments
    Object* x = first(args);
    Object* y = (args != nil && rest(args) != nil) ? second(args) : nil;

    // Dispatch based on Primitive ID
    switch (m_id) {
        // ====================================================================
        // Evaluation
        // ====================================================================

        case EVAL: {
            // (eval expr) - evaluates in the current environment
            // (eval expr env) - evaluates in the specified environment
            if (y != NULL && y != nil) {
                // Second argument provided - use it as the environment
                Environment* target_env = dynamic_cast<Environment*>(y);
                if (!target_env) {
                    return error("eval: second argument must be an environment");
                }
                // Evaluate the expression in the target environment
                return evaluator->eval(x, target_env);
            } else {
                // No second argument - evaluate in current environment
                return evaluator->eval(x, env);
            }
        }

        // ====================================================================
        // Predicates
        // ====================================================================

        case PROCEDUREQ: {
            // Check if Object is a Procedure (Primitive, Closure, or Macro)
            if (x == NULL || x == nil) {
                return truth(false);
            }
            Base* b = dynamic_cast<Base*>(x);
            if (b == NULL) {
                return truth(false);
            }
            int type = b->instance_of();
            return truth(type == static_cast<int>(Base::atype::PROCEDURE) ||
                         type == static_cast<int>(Base::atype::PRIMITIVE) ||
                         type == static_cast<int>(Base::atype::CLOSURE) ||
                         type == static_cast<int>(Base::atype::MACRO));
        }

        // ====================================================================
        // Function Application
        // ====================================================================

        case APPLY: {
            // Apply a Procedure to a list of arguments
            // (apply + 1 2 '(3 4)) => (+ 1 2 3 4)
            if (!is_procedure(x)) {
                return error("apply: first argument must be a Procedure");
            }

            // Get the remaining arguments (after the Procedure)
            Object* remaining_args = rest(args);

            // If there are no arguments, return error
            if (remaining_args == NULL || remaining_args == nil) {
                return error("apply: requires at least one argument (the list)");
            }

            // Check how many arguments we have
            Object* second_arg = rest(remaining_args);
            bool has_multiple_args = (second_arg != NULL && second_arg != nil);

            // If there's only one argument, it should be the list to apply
            if (!has_multiple_args) {
                Object* arg_list = first(remaining_args);
                Procedure* proc = dynamic_cast<Procedure*>(x);
                if (proc) {
                    return proc->apply(evaluator, arg_list, env);
                }
                return error("apply: invalid Procedure");
            }

            // Multiple arguments: collect all except the last into a list, then append the final list
            // Build the list of individual arguments first
            Object* individual_args = nil;
            Object* current = remaining_args;

            // Collect all arguments except the last one
            while (current != NULL && current != nil) {
                Object* next = rest(current);
                if (next == NULL || next == nil) {
                    // This is the last element - should be the list to apply
                    break;
                } else {
                    // Collect this argument
                    Object* arg = first(current);
                    if (individual_args == nil) {
                        individual_args = cons(arg, nil);
                    } else {
                        individual_args = append_lists_for_apply(individual_args, cons(arg, nil));
                    }
                    current = next;
                }
            }

            // Now append the final list
            Object* last_list = first(current);
            Object* collected_args;
            if (individual_args == nil) {
                collected_args = last_list;
            } else {
                collected_args = append_lists_for_apply(individual_args, last_list);
            }

            // Apply the Procedure to the collected arguments
            Procedure* proc = dynamic_cast<Procedure*>(x);
            if (proc) {
                return proc->apply(evaluator, collected_args, env);
            }
            return error("apply: invalid Procedure");
        }

        // ====================================================================
        // Higher-Order Functions
        // ====================================================================

        case MAP: {
            // Map a Procedure over one or more lists
            // (map + '(1 2 3) '(4 5 6)) => (5 7 9)
            if (!is_procedure(x)) {
                return error("map: first argument must be a Procedure");
            }

            Object* lists = rest(args);
            if (lists == NULL || lists == nil) {
                return nil;
            }

            // Check if we have just one list (common case)
            Object* first_list = first(lists);
            if (rest(lists) == NULL || rest(lists) == nil) {
                // Single list case - check if the list is empty first
                if (first_list == NULL || first_list == nil ||
                    (first(first_list) == nil && rest(first_list) == nil)) {
                    return nil;  // Mapping over empty list returns empty list
                }

                Object* result = nil;
                Object* current = first_list;

                // Protect result during loop allocations
                gc_root<Object> guard_result(result);

                while (current != NULL && current != nil && !current->is_atom()) {
                    Object* elem = first(current);
                    Object* arg_list = cons(elem, nil);
                    gc_root<Object> guard_arg_list(arg_list);

                    Procedure* proc = dynamic_cast<Procedure*>(x);
                    if (proc) {
                        Object* value = proc->apply(evaluator, arg_list, env);
                        gc_root<Object> guard_value(value);

                        if (result == nil) {
                            result = cons(value, nil);
                            guard_result = gc_root<Object>(result);
                        } else {
                            result = append_lists_for_apply(result, cons(value, nil));
                            guard_result = gc_root<Object>(result);
                        }
                    }
                    current = rest(current);
                }
                return result;
            }

            // Multiple lists case: (map + '(1 2 3) '(4 5 6)) => (5 7 9)
            // Build a list of lists
            Object* all_lists = lists;
            Object* result = nil;

            // Continue until any list is exhausted
            bool done = false;
            while (!done) {
                // Collect one element from each list
                Object* args_for_proc = nil;
                Object* list_ptr = all_lists;

                while (list_ptr != NULL && list_ptr != nil && !list_ptr->is_atom()) {
                    Object* current_list = first(list_ptr);

                    // Check if any list is exhausted
                    if (current_list == NULL || current_list == nil) {
                        done = true;
                        break;
                    }

                    // Get the first element from this list
                    Object* elem = first(current_list);
                    args_for_proc = append_lists_for_apply(args_for_proc, cons(elem, nil));

                    list_ptr = rest(list_ptr);
                }

                if (!done) {
                    // Apply Procedure to collected arguments
                    Procedure* proc = dynamic_cast<Procedure*>(x);
                    if (proc) {
                        Object* value = proc->apply(evaluator, args_for_proc, env);
                        if (result == nil) {
                            result = cons(value, nil);
                        } else {
                            result = append_lists_for_apply(result, cons(value, nil));
                        }
                    }

                    // Advance all lists to their rest
                    Object* new_lists = nil;
                    list_ptr = all_lists;
                    while (list_ptr != NULL && list_ptr != nil && !list_ptr->is_atom()) {
                        Object* current_list = first(list_ptr);
                        new_lists = append_lists_for_apply(new_lists, cons(rest(current_list), nil));
                        list_ptr = rest(list_ptr);
                    }
                    all_lists = new_lists;

                    // Check if all lists are exhausted
                    list_ptr = all_lists;
                    done = true;
                    while (list_ptr != NULL && list_ptr != nil && !list_ptr->is_atom()) {
                        Object* current_list = first(list_ptr);
                        if (current_list != NULL && current_list != nil) {
                            done = false;
                            break;
                        }
                        list_ptr = rest(list_ptr);
                    }
                }
            }

            return result;
        }

        case FOREACH: {
            // For-each: like map but for side effects, returns nil
            // (for-each display '(1 2 3)) => prints 1 2 3, returns nil
            // (for-each (lambda (x y) ...) '(1 2 3) '(4 5 6)) => processes pairs
            if (!is_procedure(x)) {
                return error("for-each: first argument must be a Procedure");
            }

            Object* lists = rest(args);
            if (lists == NULL || lists == nil) {
                return nil;
            }

            // Check if any list is empty
            bool any_empty = false;
            for (Object* l = lists; l != NULL && l != nil && !l->is_atom(); l = rest(l)) {
                Object* current_list = first(l);
                if (current_list == NULL || current_list == nil) {
                    any_empty = true;
                    break;
                }
            }

            if (any_empty) {
                return nil;
            }

            // Create list of current positions (starting at each list's head)
            Object* currents = nil;
            for (Object* l = lists; l != NULL && l != nil && !l->is_atom(); l = rest(l)) {
                currents = cons(first(l), currents);
            }
            // Reverse to maintain order
            Object* reversed_currents = nil;
            for (Object* c = currents; c != NULL && c != nil && !c->is_atom(); c = rest(c)) {
                reversed_currents = cons(first(c), reversed_currents);
            }
            currents = reversed_currents;

            // Iterate while all lists have elements
            while (true) {
                // Check if any list is exhausted
                bool any_exhausted = false;
                for (Object* c = currents; c != NULL && c != nil && !c->is_atom(); c = rest(c)) {
                    Object* current_list = first(c);
                    if (current_list == NULL || current_list == nil || current_list->is_atom()) {
                        any_exhausted = true;
                        break;
                    }
                }

                if (any_exhausted) {
                    break;
                }

                // Collect one element from each list
                Object* elements = nil;
                for (Object* c = currents; c != NULL && c != nil && !c->is_atom(); c = rest(c)) {
                    Object* current_list = first(c);
                    if (current_list != NULL && current_list != nil && !current_list->is_atom()) {
                        elements = cons(first(current_list), elements);
                    }
                }

                // Reverse to maintain order
                Object* reversed_elements = nil;
                for (Object* e = elements; e != NULL && e != nil && !e->is_atom(); e = rest(e)) {
                    reversed_elements = cons(first(e), reversed_elements);
                }

                // Apply Procedure (for side effects, ignore return value)
                Procedure* proc = dynamic_cast<Procedure*>(x);
                if (proc) {
                    proc->apply(evaluator, reversed_elements, env);
                }

                // Advance all lists
                Object* new_currents = nil;
                for (Object* c = currents; c != NULL && c != nil && !c->is_atom(); c = rest(c)) {
                    Object* current_list = first(c);
                    if (current_list != NULL && current_list != nil && !current_list->is_atom()) {
                        new_currents = cons(rest(current_list), new_currents);
                    }
                }

                // Reverse to maintain order
                Object* reversed_new_currents = nil;
                for (Object* nc = new_currents; nc != NULL && nc != nil && !nc->is_atom(); nc = rest(nc)) {
                    reversed_new_currents = cons(first(nc), reversed_new_currents);
                }
                currents = reversed_new_currents;
            }

            return nil;  // for-each always returns nil
        }

        case FILTER: {
            // (filter predicate list) - return elements that satisfy predicate
            // (filter even? '(1 2 3 4 5 6)) => (2 4 6)
            if (!is_procedure(x)) {
                return error("filter: first argument must be a Procedure");
            }

            Object* list = y;
            if (list == NULL || list == nil) {
                return nil;
            }

            Object* result = nil;
            Object* current = list;

            while (current != NULL && current != nil && !current->is_atom()) {
                Object* elem = first(current);
                Object* arg_list = cons(elem, nil);

                Procedure* proc = dynamic_cast<Procedure*>(x);
                if (proc) {
                    Object* test_result = proc->apply(evaluator, arg_list, env);
                    // If predicate returns true (non-false), include element
                    if (test_result != NULL && test_result != false_obj) {
                        if (result == nil) {
                            result = cons(elem, nil);
                        } else {
                            result = append_lists_for_apply(result, cons(elem, nil));
                        }
                    }
                }
                current = rest(current);
            }

            return result;
        }

        case FOLDLEFT: {
            // (fold-left proc init list1 list2 ...) - fold from left
            // (fold-left + 0 '(1 2 3)) => 6
            // (fold-left cons '() '(1 2 3)) => (((() . 1) . 2) . 3)
            if (!is_procedure(x)) {
                return error("fold-left: first argument must be a Procedure");
            }

            Object* accumulator = y;
            Object* lists = rest(rest(args));

            if (lists == NULL || lists == nil) {
                return accumulator;
            }

            // Single list case for now
            Object* list = first(lists);
            if (list == NULL || list == nil) {
                return accumulator;
            }

            Object* current = list;
            while (current != NULL && current != nil && !current->is_atom()) {
                Object* elem = first(current);
                // Build argument list: (accumulator elem)
                Object* arg_list = cons(accumulator, cons(elem, nil));

                Procedure* proc = dynamic_cast<Procedure*>(x);
                if (proc) {
                    accumulator = proc->apply(evaluator, arg_list, env);
                }
                current = rest(current);
            }

            return accumulator;
        }

        case FOLDRIGHT: {
            // (fold-right proc init list1 list2 ...) - fold from right
            // (fold-right + 0 '(1 2 3)) => 6
            // (fold-right cons '() '(1 2 3)) => (1 . (2 . (3 . ())))
            if (!is_procedure(x)) {
                return error("fold-right: first argument must be a Procedure");
            }

            Object* init = y;
            Object* lists = rest(rest(args));

            if (lists == NULL || lists == nil) {
                return init;
            }

            // Single list case
            Object* list = first(lists);
            if (list == NULL || list == nil) {
                return init;
            }

            // Reverse the list first to process from right
            Object* reversed = nil;
            Object* current = list;
            while (current != NULL && current != nil && !current->is_atom()) {
                reversed = cons(first(current), reversed);
                current = rest(current);
            }

            // Now fold from the "right" (which is left on the reversed list)
            Object* accumulator = init;
            current = reversed;
            while (current != NULL && current != nil && !current->is_atom()) {
                Object* elem = first(current);
                // For fold-right, arguments are (elem accumulator)
                Object* arg_list = cons(elem, cons(accumulator, nil));

                Procedure* proc = dynamic_cast<Procedure*>(x);
                if (proc) {
                    accumulator = proc->apply(evaluator, arg_list, env);
                }
                current = rest(current);
            }

            return accumulator;
        }

        // ====================================================================
        // Promises and Lazy Evaluation
        // ====================================================================

        case FORCE: {
            // Force evaluation of a delayed expression (promise/thunk)
            // If x is a Procedure (thunk), call it with no arguments
            Procedure* proc = dynamic_cast<Procedure*>(x);
            if (proc) {
                // It's a Procedure - call it with no arguments to force evaluation
                return proc->apply(evaluator, nil, env);
            }
            // Not a Procedure - just return the value as-is
            return x;
        }

        // ====================================================================
        // Multiple Values
        // ====================================================================

        case VALUES: {
            // (values obj ...) - Return multiple values
            // Special case: (values x) should return x directly, not wrapped
            if (args == nil) {
                // (values) with no args - return void/unspecified
                return nil;
            } else if (rest(args) == nil) {
                // Single value - return it directly (R5RS requirement)
                return first(args);
            } else {
                // Multiple values - wrap in ValuesObject
                return new ValuesObject(args);
            }
        }

        case CALLWITHVALUES: {
            // (call-with-values producer consumer)
            // Call producer (should return values), then call consumer with those values
            Procedure* producer = dynamic_cast<Procedure*>(x);
            if (!producer) {
                throw std::runtime_error("call-with-values: first argument must be a Procedure");
            }

            Procedure* consumer = dynamic_cast<Procedure*>(y);
            if (!consumer) {
                throw std::runtime_error("call-with-values: second argument must be a Procedure");
            }

            // Call producer with no arguments
            Object* producer_result = producer->apply(evaluator, nil, env);

            // Check if result is a ValuesObject
            ValuesObject* vals = dynamic_cast<ValuesObject*>(producer_result);
            if (vals) {
                // Multiple values - call consumer with them as separate arguments
                return consumer->apply(evaluator, vals->get_values(), env);
            } else {
                // Single value - call consumer with it
                Object* single_arg = cons(producer_result, nil);
                return consumer->apply(evaluator, single_arg, env);
            }
        }

        // ====================================================================
        // Dynamic Extent
        // ====================================================================

        case DYNAMICWIND: {
            // (dynamic-wind before thunk after)
            // Call before, then thunk, then after (ensuring after is called even on exception)
            Object* before_proc = first(args);
            Object* thunk = second(args);
            Object* after_proc = third(args);

            Procedure* before = dynamic_cast<Procedure*>(before_proc);
            if (!before) {
                throw std::runtime_error("dynamic-wind: first argument must be a Procedure");
            }

            Procedure* main_thunk = dynamic_cast<Procedure*>(thunk);
            if (!main_thunk) {
                throw std::runtime_error("dynamic-wind: second argument must be a Procedure");
            }

            Procedure* after = dynamic_cast<Procedure*>(after_proc);
            if (!after) {
                throw std::runtime_error("dynamic-wind: third argument must be a Procedure");
            }

            // Call before thunk
            before->apply(evaluator, nil, env);

            // Call main thunk, ensuring after is called even on exception
            Object* result = nil;
            try {
                result = main_thunk->apply(evaluator, nil, env);
            } catch (...) {
                // Call after thunk even on exception
                after->apply(evaluator, nil, env);
                throw;  // Re-throw the exception
            }

            // Call after thunk on normal completion
            after->apply(evaluator, nil, env);

            return result;
        }

        // ====================================================================
        // Continuations
        // ====================================================================

        case CALLCC: {
            // (call/cc receiver) — escape-only continuation for the
            // recursive evaluator. Invoking the continuation throws
            // EscapeInvoked, unwinding the C++ stack back to this frame.
            // Re-entrant continuations are handled by the toy/bytecode VMs,
            // which intercept call/cc before this primitive is reached.
            using zombie::runtime::EscapeContinuation;
            using zombie::runtime::EscapeInvoked;

            Procedure* receiver = dynamic_cast<Procedure*>(x);
            if (!receiver) {
                throw std::runtime_error("call/cc: argument must be a Procedure");
            }

            EscapeContinuation* k = gc_new<EscapeContinuation>();
            gc_root<Object> guard_k(k);
            Object* arg_list = cons(k, nil);
            gc_root<Object> guard_args(arg_list);

            try {
                Object* result = receiver->apply(evaluator, arg_list, env);
                k->invalidate();
                return result;
            } catch (const EscapeInvoked& ei) {
                k->invalidate();
                if (ei.token != k) {
                    throw;  // belongs to an outer call/cc frame
                }
                return ei.value;
            } catch (...) {
                k->invalidate();
                throw;
            }
        }

        default:
            return error("Unknown control Primitive");
    }
}

// ============================================================================
// Registration
// ============================================================================

void PrimitiveControl::install(Environment* env) {
    const int n = 999999;

    // GC Safety: Use macro to protect name string before allocating primitive
    #define DEF(name, ...) do { \
        gc_root<a_string> _n(gc_new<a_string>(name)); \
        env->define(_n.get(), gc_new<PrimitiveControl>(__VA_ARGS__)); \
    } while(0)

    // Evaluation
    DEF("eval", EVAL, 1, 2);

    // Predicates
    DEF("Procedure?", PROCEDUREQ, 1, 1);

    // Function application
    DEF("apply", APPLY, 2, n);

    // Higher-order functions
    DEF("map",        MAP, 2, n);
    DEF("for-each",   FOREACH, 2, n);
    DEF("filter",     FILTER, 2, 2);
    DEF("fold-left",  FOLDLEFT, 3, n);
    DEF("fold-right", FOLDRIGHT, 3, n);

    // Promises and lazy evaluation
    DEF("force", FORCE, 1, 1);

    // Multiple values
    DEF("values",           VALUES, 0, n);
    DEF("call-with-values", CALLWITHVALUES, 2, 2);

    // Dynamic extent
    DEF("dynamic-wind", DYNAMICWIND, 3, 3);

    // Continuations
    DEF("call/cc",                        CALLCC, 1, 1);
    DEF("call-with-current-continuation", CALLCC, 1, 1);

    #undef DEF
}
