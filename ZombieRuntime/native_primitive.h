#pragma once

#include "primitive_base.h"

namespace zombie { namespace runtime {

/**
 * @file native_primitive.h
 * @brief One wrapper for primitives implemented as plain functions.
 *
 * Replaces the per-group "enum + constructor switch + apply switch" dispatch:
 * each primitive is an ordinary function, registered declaratively in a table.
 * The wrapper validates arity (via PrimitiveBase) and forwards to the function;
 * the function does the work. Domain files (booleans, numbers, lists, ...) keep
 * their grouping — they just expose a table instead of a dispatch class.
 *
 * The signature mirrors Procedure::apply so primitives that genuinely need the
 * evaluator or environment (map, apply, with-exception-handler, ...) still can;
 * the common case ignores both.
 */
using NativeFn = Object* (*)(IEvaluator* eval, Object* args, Environment* env);

class NativePrimitive : public PrimitiveBase {
public:
    NativePrimitive(const char* name, int minArgs, int maxArgs, NativeFn fn)
        : PrimitiveBase(minArgs, maxArgs), m_fn(fn) {
        m_name = name;
    }

    Object* apply(IEvaluator* eval, Object* args, Environment* env) override {
        if (!check_arity(args)) {
            return arity_error("");
        }
        return m_fn(eval, args, env);
    }

private:
    NativeFn m_fn;
};

/**
 * @brief Declarative registration entry: name, arity range, implementation.
 */
struct NativeEntry {
    const char* name;
    int minArgs;
    int maxArgs;   ///< 999999 = unlimited
    NativeFn fn;
};

/**
 * @brief Install a table of native primitives into @p env.
 *
 * Handles the GC-safe define (root the name string before allocating the
 * primitive) once, instead of repeating it per primitive.
 */
void install_natives(Environment* env, const NativeEntry* entries, size_t count);

} } // namespace zombie::runtime
