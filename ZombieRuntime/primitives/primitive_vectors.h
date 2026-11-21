#pragma once

#include "../Environment.h"

/**
 * @file primitive_vectors.h
 * @brief Vector primitives (R7RS 6.8): vector?/make-vector/vector, length/ref/
 *        set!/fill!, vector<->list, copy/copy!/append, map/for-each,
 *        string<->vector. Table-driven (see .cpp); this header is the install hook.
 */
struct PrimitiveVectors {
    static void install(zombie::runtime::Environment* env);
};
