#pragma once

#include "../primitive_base.h"

namespace primitives {

// Install higher-order function primitives into environment
// Includes: apply, map, for-each, filter, fold-left, fold-right
//           eval, Procedure?
void install_functions_group(Environment* env);

} // namespace primitives
