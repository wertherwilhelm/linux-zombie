#pragma once

#include "../Environment.h"

/**
 * @file primitive_equivalence.h
 * @brief Equivalence predicates (R7RS 6.1): eq?, eqv?, equal?.
 *
 * Table-driven (see primitive_equivalence.cpp); this header is just the install
 * hook. eq? is pointer identity, eqv? adds number/char equivalence, equal? is
 * structural.
 */
struct PrimitiveEquivalence {
    static void install(zombie::runtime::Environment* env);
};
