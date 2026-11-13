#include "pch.h"
#include "../pch.h"
#include "primitives_misc.h"
#include "../Environment.h"
#include "../primitive_base.h"

namespace primitives {

// Install control group
// Note: Primitives already installed via OOP classes (PrimitiveEquivalence, PrimitiveControl)
void install_control_group(Environment* env) {
    // No-op: primitives already installed via OOP classes
}

// Install continuations group
// Note: Primitives already installed via OOP classes (PrimitiveControl)
void install_continuations_group(Environment* env) {
    // No-op: primitives already installed via OOP classes
}

// Install macros group
// Note: Macros are handled by the expander, not primitives
void install_macros_group(Environment* env) {
    // No-op: macro-expand is not a standard primitive
}

// Install environment group
// Note: Primitives already installed via OOP classes (PrimitiveSystem)
void install_environment_group(Environment* env) {
    // No-op: primitives already installed via OOP classes
}

// Install reflection group
// Note: Reflection primitives are not in standard R5RS
void install_reflection_group(Environment* env) {
    // No-op: reflection primitives not in standard install
}

// Install interpreters group
// Note: Sub-interpreter primitives are not in standard R5RS
void install_interpreters_group(Environment* env) {
    // No-op: interpreter primitives not in standard install
}

// Install debug group
// Note: Debug primitives already handled by PrimitiveExceptions
void install_debug_group(Environment* env) {
    // No-op: error is in PrimitiveExceptions
}

// Wrapper function that installs all misc subgroups
void install_misc_group(Environment* env) {
    // All subgroups are now no-ops
    install_control_group(env);
    install_continuations_group(env);
    install_macros_group(env);
    install_environment_group(env);
    install_reflection_group(env);
    install_interpreters_group(env);
    install_debug_group(env);
}

} // namespace primitives

// ============================================================================
// Auto-Registration with primitive_registry
// ============================================================================

namespace {
    struct MiscGroupRegistrar {
        MiscGroupRegistrar() {
            primitive_registry::instance().register_group("misc", primitives::install_misc_group);
        }
    };
    static MiscGroupRegistrar misc_registrar;
}
