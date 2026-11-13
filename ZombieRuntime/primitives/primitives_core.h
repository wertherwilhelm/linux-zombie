#pragma once

#include "../primitive_base.h"

namespace primitives {

// Install core primitives into environment
// Includes: arithmetic (+, -, *, /, abs, sqrt, etc.)
//           comparisons (=, <, >, <=, >=)
//           type predicates (number?, integer?, boolean?, etc.)
//           booleans (not, boolean?)
void install_core_group(Environment* env);

} // namespace primitives
