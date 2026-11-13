/**
 * @file a_exception.cpp
 * @brief R7RS Exception Object implementation
 */

#include "pch.h"
#include "a_exception.h"
#include "Base.h"
#include "constants.h"
#include <sstream>

namespace zombie { namespace runtime {

// ============================================================================
// Constructor
// ============================================================================

a_exception::a_exception(const std::string& message, Object* irritants, ErrorKind kind)
    : m_message(message)
    , m_irritants(irritants ? irritants : nil)
    , m_kind(kind) {
}

// ============================================================================
// Object interface implementation
// ============================================================================

Object* a_exception::clone() const {
    return gc_new<a_exception>(m_message, m_irritants, m_kind);
}

void a_exception::print(std::ostream& os) const {
    os << "#<error: " << m_message;

    // Print kind if not general
    if (m_kind == FILE) {
        os << " (file-error)";
    } else if (m_kind == READ) {
        os << " (read-error)";
    }

    // Print irritants if present
    if (m_irritants && !m_irritants->is_nil()) {
        os << " irritants: ";
        m_irritants->print(os);
    }

    os << ">";
}

bool a_exception::is_equal(Object* val) {
    if (!val || !is_exception(val)) {
        return false;
    }

    a_exception* other = static_cast<a_exception*>(val);
    return m_message == other->m_message && m_kind == other->m_kind;
}

void a_exception::trace_references(Tracer& tracer) {
    // Trace irritants list
    if (m_irritants && m_irritants != nil) {
        tracer.visit_field(m_irritants);
    }
}

// ============================================================================
// Helper functions
// ============================================================================

bool is_exception(const Object* obj) {
    return obj && dynamic_cast<const a_exception*>(obj) != nullptr;
}

bool is_file_exception(const Object* obj) {
    const a_exception* exc = dynamic_cast<const a_exception*>(obj);
    return exc && exc->is_file_error();
}

bool is_read_exception(const Object* obj) {
    const a_exception* exc = dynamic_cast<const a_exception*>(obj);
    return exc && exc->is_read_error();
}

// ============================================================================
// scheme_error (C++ exception wrapper)
// ============================================================================

scheme_error::scheme_error(const std::string& message)
    : std::runtime_error(message)
    , m_message(message)
    , m_kind(a_exception::GENERAL)
    , m_exception(nullptr) {
    // Try to create a_exception if GC is available
    // Note: We can't use gc_new here safely because exception construction
    // might happen when GC is not initialized or during stack unwinding.
    // The exception Object will be created lazily if needed.
}

scheme_error::scheme_error(const std::string& message, Object* irritants)
    : std::runtime_error(message)
    , m_message(message)
    , m_kind(a_exception::GENERAL)
    , m_exception(nullptr) {
    // Same as above - lazy creation
}

scheme_error::scheme_error(const std::string& message, Object* irritants, a_exception::ErrorKind kind)
    : std::runtime_error(message)
    , m_message(message)
    , m_kind(kind)
    , m_exception(nullptr) {
    // Same as above - lazy creation
}

scheme_error::scheme_error(a_exception* exc)
    : std::runtime_error(exc ? exc->message() : "Unknown error")
    , m_message(exc ? exc->message() : "Unknown error")
    , m_kind(exc ? exc->kind() : a_exception::GENERAL)
    , m_exception(exc) {
}

} } // namespace zombie::runtime
