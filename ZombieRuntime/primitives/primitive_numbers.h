#pragma once

#include "../Environment.h"

/**
 * @file primitive_numbers.h
 * @brief Numeric primitives (R7RS 6.2): predicates, comparisons, arithmetic,
 *        division, rounding, math/transcendental, tower conversions, rational
 *        and complex ops, number<->string, and the R7RS div/sqrt extras.
 *        Table-driven (see .cpp); this header is just the install hook.
 */
struct PrimitiveNumbers {
    static void install(zombie::runtime::Environment* env);
};
