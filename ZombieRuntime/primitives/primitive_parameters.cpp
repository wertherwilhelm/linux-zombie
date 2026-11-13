/**
 * @file primitive_parameters.cpp
 * @brief Implementation of R7RS parameter primitives
 */

#include "pch.h"
#include "../pch.h"
#include "primitive_parameters.h"
#include "../a_parameter.h"
#include "../atom.h"
#include "../Base.h"
#include "../constants.h"
#include <vector>

// ============================================================================
// Constructor
// ============================================================================

PrimitiveParameters::PrimitiveParameters(ID id, int minArgs, int maxArgs)
    : PrimitiveBase(minArgs, maxArgs), m_id(id) {
    // Set Primitive name based on ID
    switch (id) {
        case MAKEPARAMETER:  m_name = "make-parameter"; break;
        case PARAMETERQ:     m_name = "parameter?"; break;
        case PARAMETERIZEIMPL: m_name = "%parameterize"; break;
    }
}

// ============================================================================
// Primitive Application
// ============================================================================

Object* PrimitiveParameters::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Arity checking
    if (!check_arity(args)) {
        return arity_error("");
    }

    // Extract common arguments
    Object* x = first(args);
    Object* y = (rest(args) && !rest(args)->is_nil()) ? second(args) : nil;

    // Dispatch based on Primitive ID
    switch (m_id) {
        // ====================================================================
        // Parameter Creation
        // ====================================================================
        case MAKEPARAMETER: {
            // (make-parameter init)
            // (make-parameter init converter)

            Object* init_value = x;
            Object* converter = nil;

            if (y && !y->is_nil()) {
                Procedure* conv = dynamic_cast<Procedure*>(y);
                if (!conv) {
                    return error("make-parameter: converter must be a procedure");
                }
                converter = y;

                // R7RS: the converter is applied to the initial value
                gc_root<Object> guard_init(init_value);
                Object* arg_list = cons(init_value, nil);
                gc_root<Object> guard_args(arg_list);
                init_value = conv->apply(eval, arg_list, env);
            }

            gc_root<Object> guard_converted(init_value);
            return gc_new<a_parameter>(init_value, converter);
        }

        case PARAMETERIZEIMPL: {
            // (%parameterize (list p ...) (list v ...) thunk) — runtime
            // support for compiled parameterize forms. The bytecode compiler
            // rewrites
            //   (parameterize ((p v) ...) body ...)
            // to this call; params and values arrive already evaluated.
            // Mirrors the recursive evaluator's native parameterize
            // (lisp.cpp): converters are applied on entry, restore is RAW
            // (R7RS — the converter must not run again on the old value).
            using zombie::runtime::a_parameter;

            Object* thunk_obj = third(args);
            Procedure* thunk = dynamic_cast<Procedure*>(thunk_obj);
            if (!thunk) {
                return error("%parameterize: third argument must be a procedure");
            }
            gc_root<Object> guard_thunk(thunk_obj);

            std::vector<a_parameter*> params;
            std::vector<Object*> new_values;
            std::vector<gc_ptr<Object>> roots;  // keep params/values alive across converter calls

            Object* p = x;
            Object* v = y;
            for (; p && !p->is_nil(); p = rest(p), v = rest(v)) {
                if (!v || v->is_nil()) {
                    return error("%parameterize: parameter/value count mismatch");
                }
                a_parameter* param = dynamic_cast<a_parameter*>(first(p));
                if (!param) {
                    return error("parameterize: binding target is not a parameter object");
                }
                params.push_back(param);
                new_values.push_back(first(v));
                roots.emplace_back(static_cast<Object*>(param));
                roots.emplace_back(first(v));
            }

            // Save old values, then install a restore guard BEFORE applying
            // converters, so a throwing converter or thunk restores all
            std::vector<Object*> old_values;
            old_values.reserve(params.size());
            for (a_parameter* prm : params) {
                old_values.push_back(prm->get_value());
                roots.emplace_back(prm->get_value());
            }

            struct param_restore {
                std::vector<a_parameter*>& params;
                std::vector<Object*>& olds;
                ~param_restore() {
                    for (size_t i = 0; i < params.size(); ++i) {
                        params[i]->set_value(olds[i]);  // raw, no converter
                    }
                }
            } restore{ params, old_values };

            for (size_t i = 0; i < params.size(); ++i) {
                params[i]->set_value(eval, env, new_values[i]);  // applies converter
            }

            return thunk->apply(eval, nil, env);
        }

        // ====================================================================
        // Parameter Predicate
        // ====================================================================
        case PARAMETERQ:
            // (parameter? obj)
            return is_parameter(x) ? t : static_cast<Object*>(false_obj);

        default:
            return error("Unknown parameter Primitive");
    }
}

// ============================================================================
// Installation
// ============================================================================

void PrimitiveParameters::install(Environment* env) {
    // GC Safety: Protect name string before allocating primitive
    gc_root<a_string> name_make(gc_new<a_string>("make-parameter"));
    env->define(name_make.get(), gc_new<PrimitiveParameters>(MAKEPARAMETER, 1, 2));

    gc_root<a_string> name_pred(gc_new<a_string>("parameter?"));
    env->define(name_pred.get(), gc_new<PrimitiveParameters>(PARAMETERQ, 1, 1));

    // Runtime support for compiled parameterize forms (bytecode VM)
    gc_root<a_string> name_pz(gc_new<a_string>("%parameterize"));
    env->define(name_pz.get(), gc_new<PrimitiveParameters>(PARAMETERIZEIMPL, 3, 3));
}
