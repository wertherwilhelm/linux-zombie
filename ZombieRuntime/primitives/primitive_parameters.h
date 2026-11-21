#pragma once

#include "../Environment.h"

/**
 * @file primitive_parameters.h
 * @brief Parameter objects (R7RS 4.2.6): make-parameter, parameter?, and the
 *        %parameterize runtime helper the Expander desugars `parameterize` to.
 *        Table-driven (see .cpp); this header is just the install hook.
 */
struct PrimitiveParameters {
    static void install(zombie::runtime::Environment* env);
};
