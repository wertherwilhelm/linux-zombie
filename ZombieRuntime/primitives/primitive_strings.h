#pragma once

#include "../Environment.h"

/**
 * @file primitive_strings.h
 * @brief String primitives (R7RS 6.7): string?/make-string/string, length/ref/
 *        set!/substring, append/copy/upcase/downcase/foldcase/fill!/copy!,
 *        string<->list, case-(in)sensitive comparison chains, map/for-each,
 *        split/contains. Table-driven (see .cpp); this header is the install hook.
 */
struct PrimitiveStrings {
    static void install(zombie::runtime::Environment* env);
};
