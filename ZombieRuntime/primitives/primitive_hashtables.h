#pragma once

#include "../Environment.h"

namespace zombie { namespace runtime {

/**
 * @file primitive_hashtables.h
 * @brief Hash table primitives (SRFI-69 subset), backed by HashTable.
 *        Table-driven (see .cpp); this header is just the install hook.
 */
struct PrimitiveHashTables {
    static void install(Environment* env);
};

} } // namespace zombie::runtime
