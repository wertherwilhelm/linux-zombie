#include "zombie_runtime_types.h"
#include "primitive_control.h"
#include "../native_primitive.h"
#include "../Base.h"
#include "../atom.h"
#include "../procedure.h"
#include "../values_object.h"
#include "../cons_cell.h"
#include "../i_evaluator.h"
#include "../escape_continuation.h"
#include "gc_ptr.h"
#include <algorithm>
#include <vector>
#include <stdexcept>

/**
 * @file primitive_control.cpp
 * @brief Control primitives (R7RS 6.10 + higher-order), table-driven.
 */

namespace {

using namespace zombie::runtime;

bool is_procedure(Object* x) {
    if (x == NULL || x == nil) return false;
    Base* b = dynamic_cast<Base*>(x);
    if (b == NULL) return false;
    int type = b->instance_of();
    return (type == static_cast<int>(Base::atype::PROCEDURE) ||
            type == static_cast<int>(Base::atype::PRIMITIVE) ||
            type == static_cast<int>(Base::atype::CLOSURE) ||
            type == static_cast<int>(Base::atype::MACRO));
}

// Append list1 ++ list2, building fresh cons cells for list1's spine.
Object* append_lists_for_apply(Object* list1, Object* list2) {
    if (list1 == NULL || list1 == nil) return list2;
    Object* first_elem = first(list1);
    Object* rest_of_list1 = rest(list1);
    if (rest_of_list1 == NULL || rest_of_list1 == nil) {
        return gc_new<ConsCell>(first_elem, list2);
    }
    Object* rest_result = append_lists_for_apply(rest_of_list1, list2);
    gc_root<Object> rest_guard(rest_result);
    return gc_new<ConsCell>(first_elem, rest_result);
}

// (eval expr [env])
Object* prim_eval(IEvaluator* evaluator, Object* args, Environment* env) {
    Object* x = first(args);
    Object* y = (args != nil && rest(args) != nil) ? second(args) : nil;
    if (y != NULL && y != nil) {
        Environment* target_env = dynamic_cast<Environment*>(y);
        if (!target_env) return error("eval: second argument must be an environment");
        return evaluator->eval(x, target_env);
    }
    return evaluator->eval(x, env);
}

// (procedure? obj)
Object* prim_procedureq(IEvaluator*, Object* args, Environment*) {
    return truth(is_procedure(first(args)));
}

// (apply proc arg ... list)
Object* prim_apply(IEvaluator* evaluator, Object* args, Environment* env) {
    Object* x = first(args);
    if (!is_procedure(x)) return error("apply: first argument must be a Procedure");
    Object* remaining_args = rest(args);
    if (remaining_args == NULL || remaining_args == nil) {
        return error("apply: requires at least one argument (the list)");
    }
    Object* second_arg = rest(remaining_args);
    bool has_multiple_args = (second_arg != NULL && second_arg != nil);
    if (!has_multiple_args) {
        Object* arg_list = first(remaining_args);
        Procedure* proc = dynamic_cast<Procedure*>(x);
        if (proc) return proc->apply(evaluator, arg_list, env);
        return error("apply: invalid Procedure");
    }
    Object* individual_args = nil;
    Object* current = remaining_args;
    while (current != NULL && current != nil) {
        Object* next = rest(current);
        if (next == NULL || next == nil) break;  // last element is the list to apply
        Object* arg = first(current);
        if (individual_args == nil) individual_args = cons(arg, nil);
        else individual_args = append_lists_for_apply(individual_args, cons(arg, nil));
        current = next;
    }
    Object* last_list = first(current);
    Object* collected_args = (individual_args == nil) ? last_list
                             : append_lists_for_apply(individual_args, last_list);
    Procedure* proc = dynamic_cast<Procedure*>(x);
    if (proc) return proc->apply(evaluator, collected_args, env);
    return error("apply: invalid Procedure");
}

// (map proc list1 list2 ...) — stops at the shortest list.
Object* prim_map(IEvaluator* evaluator, Object* args, Environment* env) {
    Object* x = first(args);
    if (!is_procedure(x)) return error("map: first argument must be a Procedure");
    Object* lists = rest(args);
    if (lists == NULL || lists == nil) return nil;

    Object* first_list = first(lists);
    if (rest(lists) == NULL || rest(lists) == nil) {  // single-list fast path
        if (first_list == NULL || first_list == nil ||
            (first(first_list) == nil && rest(first_list) == nil)) {
            return nil;
        }
        Object* result = nil;
        Object* current = first_list;
        gc_root<Object> guard_result(result);
        while (current != NULL && current != nil && !current->is_atom()) {
            Object* elem = first(current);
            Object* arg_list = cons(elem, nil);
            gc_root<Object> guard_arg_list(arg_list);
            Procedure* proc = dynamic_cast<Procedure*>(x);
            if (proc) {
                Object* value = proc->apply(evaluator, arg_list, env);
                gc_root<Object> guard_value(value);
                if (result == nil) { result = cons(value, nil); guard_result = gc_root<Object>(result); }
                else { result = append_lists_for_apply(result, cons(value, nil)); guard_result = gc_root<Object>(result); }
            }
            current = rest(current);
        }
        return result;
    }

    Object* all_lists = lists;
    Object* result = nil;
    bool done = false;
    while (!done) {
        Object* args_for_proc = nil;
        Object* list_ptr = all_lists;
        while (list_ptr != NULL && list_ptr != nil && !list_ptr->is_atom()) {
            Object* current_list = first(list_ptr);
            if (current_list == NULL || current_list == nil) { done = true; break; }
            Object* elem = first(current_list);
            args_for_proc = append_lists_for_apply(args_for_proc, cons(elem, nil));
            list_ptr = rest(list_ptr);
        }
        if (!done) {
            Procedure* proc = dynamic_cast<Procedure*>(x);
            if (proc) {
                Object* value = proc->apply(evaluator, args_for_proc, env);
                if (result == nil) result = cons(value, nil);
                else result = append_lists_for_apply(result, cons(value, nil));
            }
            Object* new_lists = nil;
            list_ptr = all_lists;
            while (list_ptr != NULL && list_ptr != nil && !list_ptr->is_atom()) {
                Object* current_list = first(list_ptr);
                new_lists = append_lists_for_apply(new_lists, cons(rest(current_list), nil));
                list_ptr = rest(list_ptr);
            }
            all_lists = new_lists;
            list_ptr = all_lists;
            done = true;
            while (list_ptr != NULL && list_ptr != nil && !list_ptr->is_atom()) {
                Object* current_list = first(list_ptr);
                if (current_list != NULL && current_list != nil) { done = false; break; }
                list_ptr = rest(list_ptr);
            }
        }
    }
    return result;
}

// (for-each proc list1 list2 ...)
Object* prim_foreach(IEvaluator* evaluator, Object* args, Environment* env) {
    Object* x = first(args);
    if (!is_procedure(x)) return error("for-each: first argument must be a Procedure");
    Object* lists = rest(args);
    if (lists == NULL || lists == nil) return nil;

    bool any_empty = false;
    for (Object* l = lists; l != NULL && l != nil && !l->is_atom(); l = rest(l)) {
        Object* current_list = first(l);
        if (current_list == NULL || current_list == nil) { any_empty = true; break; }
    }
    if (any_empty) return nil;

    Object* currents = nil;
    for (Object* l = lists; l != NULL && l != nil && !l->is_atom(); l = rest(l)) {
        currents = cons(first(l), currents);
    }
    Object* reversed_currents = nil;
    for (Object* c = currents; c != NULL && c != nil && !c->is_atom(); c = rest(c)) {
        reversed_currents = cons(first(c), reversed_currents);
    }
    currents = reversed_currents;

    while (true) {
        bool any_exhausted = false;
        for (Object* c = currents; c != NULL && c != nil && !c->is_atom(); c = rest(c)) {
            Object* current_list = first(c);
            if (current_list == NULL || current_list == nil || current_list->is_atom()) { any_exhausted = true; break; }
        }
        if (any_exhausted) break;

        Object* elements = nil;
        for (Object* c = currents; c != NULL && c != nil && !c->is_atom(); c = rest(c)) {
            Object* current_list = first(c);
            if (current_list != NULL && current_list != nil && !current_list->is_atom()) {
                elements = cons(first(current_list), elements);
            }
        }
        Object* reversed_elements = nil;
        for (Object* e = elements; e != NULL && e != nil && !e->is_atom(); e = rest(e)) {
            reversed_elements = cons(first(e), reversed_elements);
        }
        Procedure* proc = dynamic_cast<Procedure*>(x);
        if (proc) proc->apply(evaluator, reversed_elements, env);

        Object* new_currents = nil;
        for (Object* c = currents; c != NULL && c != nil && !c->is_atom(); c = rest(c)) {
            Object* current_list = first(c);
            if (current_list != NULL && current_list != nil && !current_list->is_atom()) {
                new_currents = cons(rest(current_list), new_currents);
            }
        }
        Object* reversed_new_currents = nil;
        for (Object* nc = new_currents; nc != NULL && nc != nil && !nc->is_atom(); nc = rest(nc)) {
            reversed_new_currents = cons(first(nc), reversed_new_currents);
        }
        currents = reversed_new_currents;
    }
    return nil;
}

// (filter pred list)
Object* prim_filter(IEvaluator* evaluator, Object* args, Environment* env) {
    Object* x = first(args);
    Object* y = (args != nil && rest(args) != nil) ? second(args) : nil;
    if (!is_procedure(x)) return error("filter: first argument must be a Procedure");
    Object* list = y;
    if (list == NULL || list == nil) return nil;
    Object* result = nil;
    Object* current = list;
    while (current != NULL && current != nil && !current->is_atom()) {
        Object* elem = first(current);
        Object* arg_list = cons(elem, nil);
        Procedure* proc = dynamic_cast<Procedure*>(x);
        if (proc) {
            Object* test_result = proc->apply(evaluator, arg_list, env);
            if (test_result != NULL && test_result != false_obj) {
                if (result == nil) result = cons(elem, nil);
                else result = append_lists_for_apply(result, cons(elem, nil));
            }
        }
        current = rest(current);
    }
    return result;
}

// (fold-left proc init list) — calls (proc acc elem).
Object* prim_fold_left(IEvaluator* evaluator, Object* args, Environment* env) {
    Object* x = first(args);
    Object* y = (args != nil && rest(args) != nil) ? second(args) : nil;
    if (!is_procedure(x)) return error("fold-left: first argument must be a Procedure");
    Object* accumulator = y;
    Object* lists = rest(rest(args));
    if (lists == NULL || lists == nil) return accumulator;
    Object* list = first(lists);
    if (list == NULL || list == nil) return accumulator;
    Object* current = list;
    while (current != NULL && current != nil && !current->is_atom()) {
        Object* elem = first(current);
        Object* arg_list = cons(accumulator, cons(elem, nil));
        Procedure* proc = dynamic_cast<Procedure*>(x);
        if (proc) accumulator = proc->apply(evaluator, arg_list, env);
        current = rest(current);
    }
    return accumulator;
}

// (fold-right proc init list) — calls (proc elem acc).
Object* prim_fold_right(IEvaluator* evaluator, Object* args, Environment* env) {
    Object* x = first(args);
    Object* y = (args != nil && rest(args) != nil) ? second(args) : nil;
    if (!is_procedure(x)) return error("fold-right: first argument must be a Procedure");
    Object* init = y;
    Object* lists = rest(rest(args));
    if (lists == NULL || lists == nil) return init;
    Object* list = first(lists);
    if (list == NULL || list == nil) return init;
    Object* reversed = nil;
    Object* current = list;
    while (current != NULL && current != nil && !current->is_atom()) {
        reversed = cons(first(current), reversed);
        current = rest(current);
    }
    Object* accumulator = init;
    current = reversed;
    while (current != NULL && current != nil && !current->is_atom()) {
        Object* elem = first(current);
        Object* arg_list = cons(elem, cons(accumulator, nil));
        Procedure* proc = dynamic_cast<Procedure*>(x);
        if (proc) accumulator = proc->apply(evaluator, arg_list, env);
        current = rest(current);
    }
    return accumulator;
}

// (reduce proc ridentity list) — SRFI-1: (proc elem acc), seed = first element.
Object* prim_reduce(IEvaluator* evaluator, Object* args, Environment* env) {
    Object* x = first(args);
    Object* y = (args != nil && rest(args) != nil) ? second(args) : nil;
    if (!is_procedure(x)) return error("reduce: first argument must be a Procedure");
    Object* ridentity = y;
    Object* list = (rest(rest(args)) != nil) ? first(rest(rest(args))) : nil;
    if (list == NULL || list->is_nil()) return ridentity;
    Procedure* proc = dynamic_cast<Procedure*>(x);
    Object* acc = first(list);
    gc_root<Object> guard_acc(acc);
    for (Object* cur = rest(list); cur && !cur->is_nil() && !cur->is_atom(); cur = rest(cur)) {
        Object* arg_list = cons(first(cur), cons(acc, nil));
        gc_root<Object> guard_args(arg_list);
        acc = proc->apply(evaluator, arg_list, env);
        guard_acc = gc_root<Object>(acc);
    }
    return acc;
}

// Shared body: stable sort `list` with (proc a b) meaning "a before b".
Object* do_sort(IEvaluator* evaluator, Environment* env, Object* list, Object* proc_obj) {
    if (!is_procedure(proc_obj)) return error("sort: comparison argument must be a Procedure");
    Procedure* proc = dynamic_cast<Procedure*>(proc_obj);
    std::vector<Object*> elems;
    for (Object* p = list; p && !p->is_nil() && !p->is_atom(); p = rest(p)) elems.push_back(first(p));
    std::stable_sort(elems.begin(), elems.end(), [&](Object* a, Object* b) {
        return truth(proc->apply(evaluator, cons(a, cons(b, nil)), env));
    });
    Object* result = nil;
    gc_root<Object> guard_result(result);
    for (auto it = elems.rbegin(); it != elems.rend(); ++it) {
        result = cons(*it, result);
        guard_result = gc_root<Object>(result);
    }
    return result;
}
Object* prim_sort(IEvaluator* evaluator, Object* args, Environment* env) {       // (sort list proc)
    return do_sort(evaluator, env, first(args), second(args));
}
Object* prim_list_sort(IEvaluator* evaluator, Object* args, Environment* env) {  // (list-sort proc list)
    return do_sort(evaluator, env, second(args), first(args));
}

// (force obj) — naive thunk force. NOTE: the prelude shadows this with the
// memoizing, promise-aware force; kept for the raw-thunk fallback.
Object* prim_force(IEvaluator* evaluator, Object* args, Environment* env) {
    Object* x = first(args);
    Procedure* proc = dynamic_cast<Procedure*>(x);
    if (proc) return proc->apply(evaluator, nil, env);
    return x;
}

// (values obj ...) — single value unwrapped, several wrapped in a ValuesObject.
Object* prim_values(IEvaluator*, Object* args, Environment*) {
    if (args == nil) return nil;
    if (rest(args) == nil) return first(args);
    return gc_new<ValuesObject>(args);
}

// (call-with-values producer consumer)
Object* prim_call_with_values(IEvaluator* evaluator, Object* args, Environment* env) {
    Object* x = first(args), *y = second(args);
    Procedure* producer = dynamic_cast<Procedure*>(x);
    if (!producer) throw std::runtime_error("call-with-values: first argument must be a Procedure");
    Procedure* consumer = dynamic_cast<Procedure*>(y);
    if (!consumer) throw std::runtime_error("call-with-values: second argument must be a Procedure");
    Object* producer_result = producer->apply(evaluator, nil, env);
    ValuesObject* vals = dynamic_cast<ValuesObject*>(producer_result);
    if (vals) return consumer->apply(evaluator, vals->get_values(), env);
    return consumer->apply(evaluator, cons(producer_result, nil), env);
}

// (dynamic-wind before thunk after) — after runs even if thunk unwinds.
Object* prim_dynamic_wind(IEvaluator* evaluator, Object* args, Environment* env) {
    Procedure* before = dynamic_cast<Procedure*>(first(args));
    if (!before) throw std::runtime_error("dynamic-wind: first argument must be a Procedure");
    Procedure* main_thunk = dynamic_cast<Procedure*>(second(args));
    if (!main_thunk) throw std::runtime_error("dynamic-wind: second argument must be a Procedure");
    Procedure* after = dynamic_cast<Procedure*>(third(args));
    if (!after) throw std::runtime_error("dynamic-wind: third argument must be a Procedure");
    before->apply(evaluator, nil, env);
    Object* result = nil;
    try {
        result = main_thunk->apply(evaluator, nil, env);
    } catch (...) {
        after->apply(evaluator, nil, env);
        throw;
    }
    after->apply(evaluator, nil, env);
    return result;
}

// (call/cc receiver) — escape-only continuation for the recursive evaluator
// (the toy/bytecode VMs intercept call/cc earlier for re-entrant continuations).
Object* prim_callcc(IEvaluator* evaluator, Object* args, Environment* env) {
    using zombie::runtime::EscapeContinuation;
    using zombie::runtime::EscapeInvoked;
    Procedure* receiver = dynamic_cast<Procedure*>(first(args));
    if (!receiver) throw std::runtime_error("call/cc: argument must be a Procedure");
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
        if (ei.token != k) throw;  // belongs to an outer call/cc frame
        return ei.value;
    } catch (...) {
        k->invalidate();
        throw;
    }
}

}  // namespace

void PrimitiveControl::install(zombie::runtime::Environment* env) {
    using namespace zombie::runtime;
    static const NativeEntry entries[] = {
        { "eval",             1, 2,      prim_eval             },
        { "Procedure?",       1, 1,      prim_procedureq       },  // legacy capital-P alias (procedure? is in booleans)
        { "apply",            2, 999999, prim_apply            },
        { "map",              2, 999999, prim_map              },
        { "for-each",         2, 999999, prim_foreach          },
        { "filter",           2, 2,      prim_filter           },
        { "fold-left",        3, 999999, prim_fold_left        },
        { "fold-right",       3, 999999, prim_fold_right       },
        { "reduce",           3, 3,      prim_reduce           },
        { "sort",             2, 2,      prim_sort             },
        { "list-sort",        2, 2,      prim_list_sort        },
        { "force",            1, 1,      prim_force            },
        { "values",           0, 999999, prim_values           },
        { "call-with-values", 2, 2,      prim_call_with_values },
        { "dynamic-wind",     3, 3,      prim_dynamic_wind     },
        { "call/cc",                        1, 1, prim_callcc  },
        { "call-with-current-continuation", 1, 1, prim_callcc  },
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}
