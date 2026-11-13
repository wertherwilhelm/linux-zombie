#include "pch.h"
#include "primitive_base.h"
#include "Base.h"

/**
 * @file PrimitiveBase.cpp
 * @brief Implementation of PrimitiveBase abstract Base class
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 *
 * Provides common functionality shared by all Primitive groups:
 * - Arity checking (min/max argument validation)
 * - Error message generation
 */

// ============================================================================
// Constructor
// ============================================================================

PrimitiveBase::PrimitiveBase(int minArgs, int maxArgs)
    : m_minArgs(minArgs), m_maxArgs(maxArgs), m_name("") {
    // Name will be set by derived class constructor based on specific Primitive
}

// ============================================================================
// Arity Checking
// ============================================================================

bool PrimitiveBase::check_arity(Object* args) {
    // Calculate argument count
    int nArgs = length(args);

    // Check against constraints
    if (nArgs < m_minArgs) {
        return false;  // Too few arguments
    }
    if (nArgs > m_maxArgs) {
        return false;  // Too many arguments
    }

    return true;  // Arity is valid
}

// ============================================================================
// Error Handling
// ============================================================================

Object* PrimitiveBase::arity_error(const std::string& context) {
    // Build error message with Primitive name
    std::string msg = "Arity error in Primitive";

    if (!m_name.empty()) {
        msg += " '" + m_name + "'";
    }

    // Add expected argument count information
    if (m_minArgs == m_maxArgs) {
        msg += ": expected " + std::to_string(m_minArgs) + " argument";
        if (m_minArgs != 1) {
            msg += "s";
        }
    } else if (m_maxArgs >= 999999) {
        msg += ": expected at least " + std::to_string(m_minArgs) + " argument";
        if (m_minArgs != 1) {
            msg += "s";
        }
    } else {
        msg += ": expected " + std::to_string(m_minArgs) +
               " to " + std::to_string(m_maxArgs) + " arguments";
    }

    // Add optional context
    if (!context.empty()) {
        msg += " (" + context + ")";
    }

    return error(msg.c_str());
}

Object* PrimitiveBase::type_error(const std::string& expected, Object* got) {
    // Build error message with Primitive name and expected type
    std::string msg = "Type error in Primitive";

    if (!m_name.empty()) {
        msg += " '" + m_name + "'";
    }

    msg += ": expected " + expected;

    // Optionally include information about what was received
    // (Can be expanded to include type of 'got' Object if needed)

    return error(msg.c_str());
}

// ============================================================================
// Note: primitive_registry and install_primitives implementations are in:
// - primitive_registry.cpp (registry implementation)
// - primitive.cpp (install_primitives implementation)
// ============================================================================
