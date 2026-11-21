#pragma once

#include "../Environment.h"

/**
 * @file primitive_system.h
 * @brief System interface primitives (R7RS 6.14): exit, emergency-exit,
 *        command-line, features, void. Table-driven (see .cpp); this header is
 *        just the install hook.
 */
struct PrimitiveSystem {
    static void install(zombie::runtime::Environment* env);
};

/// Record the process argv for (command-line).
void set_command_line_args(int argc, char* argv[]);
