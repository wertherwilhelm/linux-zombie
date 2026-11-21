#pragma once

#include "../Environment.h"

/**
 * @file primitive_symbols.h
 * @brief Symbol primitives (R7RS 6.5): symbol?, symbol->string, string->symbol,
 *        symbol=?. Table-driven (see .cpp); this header is just the install hook.
 */
struct PrimitiveSymbols {
    static void install(zombie::runtime::Environment* env);
};
