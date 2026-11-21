#include "zombie_runtime_types.h"
#include "primitive_booleans.h"
#include "../native_primitive.h"
#include "../Base.h"
#include "../constants.h"
#include "../atom.h"
#include "../procedure.h"

/**
 * @file primitive_booleans.cpp
 * @brief Boolean primitives (R7RS 6.1 / 6.3).
 *
 * Pilot for the table-driven primitive style: one named function per operation,
 * registered declaratively; NativePrimitive handles arity + error context.
 */

namespace {

using namespace zombie::runtime;

// (not obj) — only #f is false (R7RS).
Object* prim_not(IEvaluator*, Object* args, Environment*) {
    return truth(first(args) == false_obj);
}

// (boolean? obj) — #t only for #t / #f.
Object* prim_booleanq(IEvaluator*, Object* args, Environment*) {
    return truth(is_boolean(first(args)));
}

// (procedure? obj) — #t for anything applicable (closure, primitive, continuation).
Object* prim_procedureq(IEvaluator*, Object* args, Environment*) {
    return truth(dynamic_cast<Procedure*>(first(args)) != nullptr);
}

// (boolean=? b1 b2 ...) — #t iff all args are the same boolean (R7RS 6.3).
// #t / #f are singletons, so identity compares values.
Object* prim_booleaneq(IEvaluator*, Object* args, Environment*) {
    Object* x = first(args);
    if (!is_boolean(x)) return static_cast<Object*>(false_obj);
    for (Object* p = rest(args); p && !p->is_nil(); p = rest(p)) {
        Object* e = first(p);
        if (!is_boolean(e) || e != x) return static_cast<Object*>(false_obj);
    }
    return t;
}

}  // namespace

void PrimitiveBooleans::install(zombie::runtime::Environment* env) {
    using namespace zombie::runtime;
    static const NativeEntry entries[] = {
        { "not",        1, 1,      prim_not        },
        { "boolean?",   1, 1,      prim_booleanq   },
        { "procedure?", 1, 1,      prim_procedureq },
        { "boolean=?",  2, 999999, prim_booleaneq  },
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}
