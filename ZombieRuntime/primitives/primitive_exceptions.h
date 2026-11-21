#pragma once

#include "../Environment.h"

/**
 * @file primitive_exceptions.h
 * @brief R7RS exception primitives (6.11): raise, raise-continuable, error,
 *        error-object?/-message/-irritants, file-error?/read-error?,
 *        with-exception-handler, and the %guard runtime helper.
 *        Table-driven (see .cpp); this header is just the install hook. The
 *        handler stack + raise_object live in exception_handlers.h.
 */
struct PrimitiveExceptions {
    static void install(zombie::runtime::Environment* env);
};
