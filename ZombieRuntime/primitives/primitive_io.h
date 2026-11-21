#pragma once

#include "../Environment.h"

/**
 * @file primitive_io.h
 * @brief I/O primitives (R7RS 6.13): ports, read/peek/write/display, read-line/
 *        -string, string ports, bytevector I/O, load, the-/interaction-
 *        environment. Table-driven (see .cpp); this header is just the install
 *        hook.
 */
struct PrimitiveIO {
    static void install(zombie::runtime::Environment* env);
};
