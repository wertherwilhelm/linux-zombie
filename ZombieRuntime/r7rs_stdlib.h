#pragma once

#include "library.h"
#include "library_registry.h"
#include "Environment.h"

// ============================================================================
// R7RS Standard Library Bootstrap
// ============================================================================
//
// Provides standard R7RS libraries by mapping existing primitives
// to R7RS library exports.
//
// **Standard Libraries Provided**:
// - (scheme base) - Core language forms and procedures
// - (scheme read) - Reading S-expressions
// - (scheme write) - Writing S-expressions
// - (scheme file) - File operations
//
// **Usage**:
//   Environment* global_env = /* environment with primitives installed */;
//   r7rs::bootstrap_standard_libraries(global_env);
//
//   // Now libraries can import (scheme base)
//   Library* lib = LibraryRegistry::instance().get_library({"scheme", "base"});
//

namespace r7rs {

// Bootstrap all standard R7RS libraries
// base_env: environment with primitives already installed (via Primitive::install_primitives)
void bootstrap_standard_libraries(Environment* base_env);

// Bootstrap individual standard libraries
void bootstrap_scheme_base(Environment* base_env);
void bootstrap_scheme_read(Environment* base_env);
void bootstrap_scheme_write(Environment* base_env);
void bootstrap_scheme_file(Environment* base_env);

} // namespace r7rs
