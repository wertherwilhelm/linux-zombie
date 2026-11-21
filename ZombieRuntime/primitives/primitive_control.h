#pragma once

#include "../Environment.h"

/**
 * @file primitive_control.h
 * @brief Control primitives (R7RS 6.10 + higher-order): eval, procedure?, apply,
 *        map/for-each/filter/fold-left/fold-right/reduce/sort/list-sort, force,
 *        values/call-with-values, dynamic-wind, call/cc. Table-driven (see .cpp);
 *        this header is just the install hook.
 */
struct PrimitiveControl {
    static void install(zombie::runtime::Environment* env);
};
