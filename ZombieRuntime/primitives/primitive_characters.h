#pragma once

#include "../Environment.h"

/**
 * @file primitive_characters.h
 * @brief Character primitives (R7RS 6.6): predicates, conversions, and
 *        case-(in)sensitive comparison chains. Table-driven (see .cpp); this
 *        header is just the install hook.
 */
struct PrimitiveCharacters {
    static void install(zombie::runtime::Environment* env);
};
