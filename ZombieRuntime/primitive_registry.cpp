/**
 * @file primitive_registry.cpp
 * @brief Implementation of Primitive group registry
 * @author chrislk@gmail.com
 * @date 2026-01
 *
 * Provides centralized registry for Primitive group installation functions.
 * Allows applications to explicitly register and install custom Primitive
 * groups, supporting modular embedding and clean separation between core
 * interpreter primitives and application-specific extensions.
 *
 * @see primitive_registry class in Primitive.h
 */
#include "pch.h"
#include "primitive.h"
#include "Environment.h"
#include <stdexcept>

namespace zombie { namespace runtime {

// ============================================================================
// Singleton Access
// ============================================================================

primitive_registry& primitive_registry::instance() {
    static primitive_registry reg;
    return reg;
}

// ============================================================================
// Registration
// ============================================================================

void primitive_registry::register_group(const std::string& name, InstallFunc func) {
    groups_[name] = func;
}

// ============================================================================
// Installation
// ============================================================================

void primitive_registry::install_groups(Environment* env, const std::vector<std::string>& names) {
    for (const auto& name : names) {
        auto it = groups_.find(name);
        if (it != groups_.end()) {
            it->second(env);  // Call installation function
        }
    }
}

void primitive_registry::install_all(Environment* env) {
    for (const auto& [name, func] : groups_) {
        func(env);
    }
}

// ============================================================================
// Query
// ============================================================================

bool primitive_registry::has_group(const std::string& name) const {
    return groups_.find(name) != groups_.end();
}

} } // namespace zombie::runtime
