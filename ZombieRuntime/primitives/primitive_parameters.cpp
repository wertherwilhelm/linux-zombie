#include "zombie_runtime_types.h"
#include "primitive_parameters.h"
#include "../native_primitive.h"
#include "../a_parameter.h"
#include "../atom.h"
#include "../Base.h"
#include "../constants.h"
#include <vector>

/**
 * @file primitive_parameters.cpp
 * @brief Parameter objects (R7RS 4.2.6), table-driven.
 */

namespace {

using namespace zombie::runtime;

// (make-parameter init [converter]) — the converter, if given, is applied to the
// initial value (R7RS).
Object* prim_make_parameter(IEvaluator* eval, Object* args, Environment* env) {
    Object* init_value = first(args);
    Object* converter = nil;
    Object* y = (rest(args) && !rest(args)->is_nil()) ? second(args) : nil;

    if (y && !y->is_nil()) {
        Procedure* conv = dynamic_cast<Procedure*>(y);
        if (!conv) return error("make-parameter: converter must be a procedure");
        converter = y;
        gc_root<Object> guard_init(init_value);
        Object* arg_list = cons(init_value, nil);
        gc_root<Object> guard_args(arg_list);
        init_value = conv->apply(eval, arg_list, env);
    }
    gc_root<Object> guard_converted(init_value);
    return gc_new<a_parameter>(init_value, converter);
}

// (parameter? obj)
Object* prim_parameterq(IEvaluator*, Object* args, Environment*) {
    return is_parameter(first(args)) ? t : static_cast<Object*>(false_obj);
}

// (%parameterize (list p ...) (list v ...) thunk) — runtime support for the
// Expander's parameterize desugaring. Converters apply on entry; the C++
// param_restore destructor restores old values RAW on exit (incl. escape), so
// the converter never re-runs on restore (R7RS).
Object* prim_parameterize(IEvaluator* eval, Object* args, Environment* env) {
    Object* thunk_obj = third(args);
    Procedure* thunk = dynamic_cast<Procedure*>(thunk_obj);
    if (!thunk) return error("%parameterize: third argument must be a procedure");
    gc_root<Object> guard_thunk(thunk_obj);

    std::vector<a_parameter*> params;
    std::vector<Object*> new_values;
    std::vector<gc_ptr<Object>> roots;  // keep params/values alive across converters

    Object* p = first(args);
    Object* v = second(args);
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

    // Save old values, install the restore guard BEFORE applying converters so a
    // throwing converter or thunk still restores all of them.
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

}  // namespace

void PrimitiveParameters::install(zombie::runtime::Environment* env) {
    using namespace zombie::runtime;
    static const NativeEntry entries[] = {
        { "make-parameter", 1, 2, prim_make_parameter },
        { "parameter?",     1, 1, prim_parameterq     },
        { "%parameterize",  3, 3, prim_parameterize   },
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}
