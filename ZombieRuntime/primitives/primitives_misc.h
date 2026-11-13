#pragma once

#include "../primitive_base.h"

namespace primitives {

// Install miscellaneous Primitive groups
void install_control_group(Environment* env);         // eq?, eqv?, equal?
void install_continuations_group(Environment* env);   // force
void install_macros_group(Environment* env);          // Macro-expand
void install_environment_group(Environment* env);     // the-environment, make-environment, etc.
void install_reflection_group(Environment* env);      // type-of, type-name, etc. (C++ introspection)
void install_interpreters_group(Environment* env);    // make-interpreter, etc. (reflective tower)
void install_debug_group(Environment* env);           // time-call, error

// Wrapper that installs all misc subgroups at once
void install_misc_group(Environment* env);

} // namespace primitives
