/**
 * @file a_bytevector.cpp
 * @brief Implementation of bytevector operations
 */

#include "pch.h"
#include "a_bytevector.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace zombie { namespace runtime {

// ============================================================================
// Constructors and Destructor
// ============================================================================

a_bytevector::a_bytevector()
    : ValueType()
    , m_bytes()
{
}

a_bytevector::a_bytevector(int size)
    : ValueType()
    , m_bytes(size, 0)  // Initialize with zeros
{
    if (size < 0) {
        throw std::invalid_argument("bytevector size cannot be negative");
    }
}

a_bytevector::a_bytevector(int size, uint8_t fill)
    : ValueType()
    , m_bytes(size, fill)
{
    if (size < 0) {
        throw std::invalid_argument("bytevector size cannot be negative");
    }
}

a_bytevector::a_bytevector(const std::vector<uint8_t>& bytes)
    : ValueType()
    , m_bytes(bytes)
{
}

a_bytevector::~a_bytevector()
{
    // std::vector handles cleanup automatically
}

// ============================================================================
// Object Interface Implementation
// ============================================================================

Object* a_bytevector::clone() const
{
    return gc_new<a_bytevector>(m_bytes);
}

void a_bytevector::print(std::ostream& os) const
{
    // R7RS external representation: #u8(byte ...)
    os << "#u8(";
    for (size_t i = 0; i < m_bytes.size(); ++i) {
        if (i > 0) os << " ";
        os << static_cast<int>(m_bytes[i]);
    }
    os << ")";
}

// ============================================================================
// Bytevector Operations
// ============================================================================

uint8_t a_bytevector::get(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_bytes.size())) {
        throw std::out_of_range("bytevector-u8-ref: index out of range");
    }
    return m_bytes[index];
}

void a_bytevector::set(int index, uint8_t value)
{
    if (index < 0 || index >= static_cast<int>(m_bytes.size())) {
        throw std::out_of_range("bytevector-u8-set!: index out of range");
    }
    m_bytes[index] = value;
}

// ============================================================================
// Helper Functions
// ============================================================================

bool is_bytevector(const Object* obj)
{
    return obj && obj->get_type() == static_cast<int>(Type::BYTEVECTOR);
}

} } // namespace zombie::runtime
