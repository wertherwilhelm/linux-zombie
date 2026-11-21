#pragma once

#include "../Environment.h"

/**
 * @file primitive_booleans.h
 * @brief Boolean primitives (R7RS 6.1 / 6.3): not, boolean?, procedure?, boolean=?
 *
 * Each operation is a plain function registered via a NativeEntry table (see
 * primitive_booleans.cpp); there is no per-operation dispatch class. This header
 * exposes only the installation hook.
 */
struct PrimitiveBooleans {
    /// Register not / boolean? / procedure? / boolean=? into @p env.
    static void install(zombie::runtime::Environment* env);
};
