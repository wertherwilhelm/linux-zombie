#include "zombie_runtime_types.h"
#include "primitive_symbols.h"
#include "../native_primitive.h"
#include "../Base.h"
#include "../atom.h"
#include "../constants.h"  // For intern_symbol()

/**
 * @file primitive_symbols.cpp
 * @brief Symbol primitives (R7RS 6.5), table-driven.
 */

namespace {

using namespace zombie::runtime;

// (symbol? obj)
Object* prim_symbolq(IEvaluator*, Object* args, Environment*) {
    return truth(first(args)->get_type() == static_cast<int>(Type::SYMBOL));
}

// (symbol->string sym) — fresh string with the symbol's name.
Object* prim_symbol_to_string(IEvaluator*, Object* args, Environment*) {
    Object* x = first(args);
    if (!is_symbol(x)) return error("symbol->string: expected a symbol");
    return gc_new<a_string>(((a_symbol*)x)->value());
}

// (string->symbol str) — interned symbol for the string's contents.
Object* prim_string_to_symbol(IEvaluator*, Object* args, Environment*) {
    Object* x = first(args);
    if (!is_string(x)) return error("string->symbol: expected a string");
    return intern_symbol(((a_string*)x)->value());
}

// (symbol=? s1 s2 ...) — #t iff all args are the same symbol (R7RS 6.5).
Object* prim_symboleq(IEvaluator*, Object* args, Environment*) {
    Object* x = first(args);
    if (!is_symbol(x)) return error("symbol=?: arguments must be symbols");
    const std::string& s0 = ((a_symbol*)x)->value();
    for (Object* p = rest(args); p && !p->is_nil(); p = rest(p)) {
        Object* e = first(p);
        if (!is_symbol(e)) return error("symbol=?: arguments must be symbols");
        if (((a_symbol*)e)->value() != s0) return static_cast<Object*>(false_obj);
    }
    return t;
}

}  // namespace

void PrimitiveSymbols::install(zombie::runtime::Environment* env) {
    using namespace zombie::runtime;
    static const NativeEntry entries[] = {
        { "symbol?",        1, 1,      prim_symbolq          },
        { "symbol->string", 1, 1,      prim_symbol_to_string },
        { "string->symbol", 1, 1,      prim_string_to_symbol },
        { "symbol=?",       2, 999999, prim_symboleq         },
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}
