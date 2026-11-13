#pragma once

/**
 * @file a_vector.h
 * @brief Scheme vector type (resizable array of objects).
 */

#include "Object.h"
#include "atom.h"
#include <vector>

namespace zombie { namespace runtime {

/**
 * @brief Vector type for Scheme - a resizable array of objects.
 *
 * Inherits from ValueType (not Object directly) to indicate this is a
 * typed atomic value with a runtime type discriminant (Type::VECTOR).
 * Vectors are conceptually atoms (not pairs), but contain GC-managed
 * Object pointers that must be traced during garbage collection.
 */
class a_vector : public ValueType
{
public:
    /**
     * @brief Construct an empty vector.
     */
    a_vector();

    /**
     * @brief Construct a vector of the given size.
     * @param size Number of elements (initialized to nil).
     */
    a_vector(int size);

    /**
     * @brief Construct a vector of the given size filled with a value.
     * @param size Number of elements.
     * @param fill Fill value for each element.
     */
    a_vector(int size, Object* fill);

    /**
     * @brief Construct a vector from existing elements.
     * @param elements Elements to copy into the vector.
     */
    a_vector(const std::vector<Object*>& elements);

    /**
     * @brief Destroy the vector.
     */
    virtual ~a_vector();

    /**
     * @brief Clone this vector.
     * @return Newly allocated vector with the same element pointers.
     */
    Object* clone() const override;

    /**
     * @brief Print this vector to a stream.
     */
    void print(std::ostream& os) const override;

    /**
     * @brief Vectors are atoms.
     */
    int is_atom() const override { return 1; }

    /**
     * @brief Return type code for vectors.
     */
    int get_type() const override { return static_cast<int>(Type::VECTOR); }

    /**
     * @brief Vectors are not pairs.
     */
    int pair() const override { return 0; }

    // Vector operations
    /**
     * @brief Get number of elements.
     */
    int length() const { return static_cast<int>(m_elements.size()); }

    /**
     * @brief Get element at index.
     * @param index Zero-based index.
     * @return Element at index.
     */
    Object* get(int index) const;

    /**
     * @brief Set element at index.
     * @param index Zero-based index.
     * @param value New element value.
     */
    void set(int index, Object* value);

    // Conversion
    /**
     * @brief Convert to std::vector of Object pointers.
     */
    std::vector<Object*> to_std_vector() const { return m_elements; }

    /**
     * @brief Trace referenced objects for GC.
     */
    void trace_references(Tracer& tracer) override;

private:
    std::vector<Object*> m_elements;
};

// Helper functions
/**
 * @brief Check if an Object is a vector.
 */
bool is_vector(const Object* obj);

} } // namespace zombie::runtime
