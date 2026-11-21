#pragma once

#include "../Environment.h"

/**
 * @file primitive_bytevectors.h
 * @brief Bytevector primitives (R7RS 6.9): bytevector?/make-bytevector/
 *        bytevector, length/u8-ref/u8-set!, copy/copy!/append, utf8<->string.
 *        Table-driven (see .cpp); this header is just the install hook.
 */
struct PrimitiveBytevectors {
    static void install(zombie::runtime::Environment* env);
};
