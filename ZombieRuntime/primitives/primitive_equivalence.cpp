#include "zombie_runtime_types.h"
#include "primitive_equivalence.h"
#include "../native_primitive.h"
#include "../Base.h"
#include "../constants.h"
#include "../atom.h"

/**
 * @file primitive_equivalence.cpp
 * @brief Equivalence predicates (R7RS 6.1), table-driven.
 */

namespace {

using namespace zombie::runtime;

// (eq? a b) — pointer identity. Symbols are interned, so eq? works on symbols.
Object* prim_eq(IEvaluator*, Object* args, Environment*) {
    return truth(first(args) == second(args));
}

// (eqv? a b) — identity plus number/character equivalence (Base.h eqv).
Object* prim_eqv(IEvaluator*, Object* args, Environment*) {
    return truth(eqv(first(args), second(args)));
}

// (equal? a b) — structural equality (Base.h equal, cycle-safe).
Object* prim_equal(IEvaluator*, Object* args, Environment*) {
    return truth(equal(first(args), second(args)));
}

}  // namespace

void PrimitiveEquivalence::install(zombie::runtime::Environment* env) {
    using namespace zombie::runtime;
    static const NativeEntry entries[] = {
        { "eq?",    2, 2, prim_eq    },
        { "eqv?",   2, 2, prim_eqv   },
        { "equal?", 2, 2, prim_equal },
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}
