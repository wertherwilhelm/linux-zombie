#pragma once

/**
 * @file a_bytevector.h
 * @brief Bytevector type for R7RS - a resizable array of bytes (0-255)
 *
 * R7RS Section 6.9 - Bytevectors
 * Bytevectors are objects of a disjoint type whose elements are bytes (exact
 * integers in the range [0, 255]).
 */

#include "Object.h"
#include "atom.h"
#include <vector>
#include <cstdint>

namespace zombie { namespace runtime {

/**
 * @brief Bytevector type for Scheme - a resizable array of bytes
 *
 * Inherits from ValueType (not Object directly) to indicate this is a
 * typed atomic value with a runtime type discriminant (Type::BYTEVECTOR).
 * Bytevectors are conceptually atoms (not pairs), and contain raw bytes
 * that do not need GC tracing.
 */
class a_bytevector : public ValueType
{
public:
    a_bytevector();
    a_bytevector(int size);
    a_bytevector(int size, uint8_t fill);
    a_bytevector(const std::vector<uint8_t>& bytes);
    virtual ~a_bytevector();

    Object* clone() const override;
    void print(std::ostream& os) const override;
    int is_atom() const override { return 1; }
    int get_type() const override { return static_cast<int>(Type::BYTEVECTOR); }
    int pair() const override { return 0; }

    // Bytevector operations
    int length() const { return static_cast<int>(m_bytes.size()); }
    uint8_t get(int index) const;
    void set(int index, uint8_t value);

    // Direct access to underlying bytes
    const std::vector<uint8_t>& bytes() const { return m_bytes; }
    std::vector<uint8_t>& bytes() { return m_bytes; }

    // No GC tracing needed - bytevectors contain only raw bytes
    void trace_references(Tracer& tracer) override {
        // Bytevectors don't contain Object pointers, so nothing to trace
    }

private:
    std::vector<uint8_t> m_bytes;
};

// Helper functions
bool is_bytevector(const Object* obj);

} } // namespace zombie::runtime
