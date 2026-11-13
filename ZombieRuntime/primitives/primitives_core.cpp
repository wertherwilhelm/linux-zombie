#include "pch.h"
#include "../pch.h"
#include "primitives_core.h"
#include "../Environment.h"
#include "../primitive_base.h"

namespace primitives {

// Install core primitives into environment
// Note: Core primitives are already installed by install_primitives()
// via OOP classes (PrimitiveBooleans, PrimitiveNumbers, PrimitiveEquivalence).
// This registry function is kept for compatibility.
void install_core_group(Environment* env) {
    // No-op: primitives already installed via OOP classes in install_primitives()
}

} // namespace primitives

// ============================================================================
// Auto-Registration with primitive_registry
// ============================================================================

namespace {
    struct CoreGroupRegistrar {
        CoreGroupRegistrar() {
            primitive_registry::instance().register_group("core", primitives::install_core_group);
        }
    };
    static CoreGroupRegistrar core_registrar;  // Auto-registers on static initialization
}
