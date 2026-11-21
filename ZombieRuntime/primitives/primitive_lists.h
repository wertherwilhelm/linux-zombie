#pragma once

#include "../Environment.h"

/**
 * @file primitive_lists.h
 * @brief List / pair primitives (R7RS 6.4): pair?/null?/list?, cons/car/cdr and
 *        the cXr family, set-car!/cdr!, list/length/append/reverse, list-tail/
 *        -ref/-set!/-copy, make-list, last-pair, memq/memv/member, assq/assv/
 *        assoc. Table-driven (see .cpp); this header is just the install hook.
 */
struct PrimitiveLists {
    static void install(zombie::runtime::Environment* env);
};
