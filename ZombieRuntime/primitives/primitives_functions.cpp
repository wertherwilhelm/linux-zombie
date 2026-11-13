#include "pch.h"
#include "../pch.h"
#include "primitives_functions.h"
#include "../Environment.h"
#include "../primitive_base.h"

namespace primitives {

// Install higher-order function primitives into environment
// Note: Function primitives are already installed by install_primitives()
// via OOP classes (PrimitiveControl, PrimitiveSystem).
// This registry function is kept for compatibility.
void install_functions_group(Environment* env) {
    // No-op: primitives already installed via OOP classes in install_primitives()
}

} // namespace primitives

// ============================================================================
// Auto-Registration with primitive_registry
// ============================================================================

namespace {
    struct FunctionsGroupRegistrar {
        FunctionsGroupRegistrar() {
            primitive_registry::instance().register_group("functions", primitives::install_functions_group);
        }
    };
    static FunctionsGroupRegistrar functions_registrar;
}
