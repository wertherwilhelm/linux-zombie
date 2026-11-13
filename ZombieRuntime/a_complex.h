#pragma once

#include "Object.h"
#include "atom.h"
#include <iostream>

namespace zombie { namespace runtime {

/**
 * @file a_complex.h
 * @brief Complex number type (real + imaginary parts).
 *
 * Represents complex numbers in Scheme's numeric tower.
 * Components (real and imag) can be:
 * - Exact integers (atom<int>, atom<long long>)
 * - Exact rationals (a_rational)
 * - Inexact doubles (atom<double>)
 *
 * This preserves exactness:
 * - 3+4i        -> exact (both components are exact integers)
 * - 1/2+3/4i    -> exact (both components are exact rationals)
 * - 1.5+2.0i    -> inexact (components are doubles)
 * - 3+2.0i      -> inexact (mixed exact/inexact promotes to inexact)
 *
 * Special cases:
 * - a+0i where a is real -> can be simplified to just a
 * - 0+bi where b is real -> pure imaginary
 *
 * Inherits from ValueType (not Object directly) to indicate this is a
 * typed atomic value with a runtime type discriminant (Type::COMPLEX).
 */

/**
 * @brief Complex number (real + imaginary parts).
 */
class a_complex : public ValueType {
public:
    /**
     * @brief Construct from real and imaginary parts.
     * @param real_part Real component.
     * @param imag_part Imaginary component.
     */
    a_complex(Object* real_part, Object* imag_part);

    /**
     * @brief Copy constructor.
     */
    a_complex(const a_complex& other);

    /**
     * @brief Return type code for complex numbers.
     */
    int get_type() const override { return static_cast<int>(Type::COMPLEX); }

    /**
     * @brief Return runtime instance type code.
     */
    int instance_of() override { return static_cast<int>(Type::COMPLEX); }

    /**
     * @brief Complex numbers are atoms.
     */
    int is_atom() const override { return 1; }

    /**
     * @brief Complex numbers are not pairs.
     */
    int pair() const override { return 0; }

    /**
     * @brief Clone this complex number.
     */
    Object* clone() const override;

    /**
     * @brief Print this complex number to a stream.
     */
    void print(std::ostream& out) const override;

    /**
     * @brief Trace referenced objects for GC.
     */
    void trace_references(Tracer& tracer) override;

    /**
     * @brief Get real component.
     */
    Object* real_part() const { return real_; }

    /**
     * @brief Get imaginary component.
     */
    Object* imag_part() const { return imag_; }

    /**
     * @brief Check if imaginary part is zero (i.e., actually a real number).
     */
    bool is_real() const;

    /**
     * @brief Check if this complex number is exact (both parts exact).
     */
    bool is_exact() const;

    /**
     * @brief Add two complex numbers.
     */
    static a_complex* add(const a_complex* a, const a_complex* b);

    /**
     * @brief Subtract two complex numbers.
     */
    static a_complex* sub(const a_complex* a, const a_complex* b);

    /**
     * @brief Multiply two complex numbers.
     */
    static a_complex* mul(const a_complex* a, const a_complex* b);

    /**
     * @brief Divide two complex numbers.
     */
    static a_complex* div(const a_complex* a, const a_complex* b);

    /**
     * @brief Negate a complex number.
     */
    static a_complex* negate(const a_complex* a);

    /**
     * @brief Equality check for complex numbers.
     */
    static bool equal(const a_complex* a, const a_complex* b);

private:
    Object* real_;  // Real part (can be int/long/rational/double)
    Object* imag_;  // Imaginary part (can be int/long/rational/double)
};

/**
 * @brief Check if Object is a complex number.
 */
inline bool is_complex(const Object* obj) {
    return obj && obj->get_type() == static_cast<int>(Type::COMPLEX);
}

/**
 * @brief Extract complex pointer (returns nullptr if not complex).
 */
inline a_complex* as_complex(Object* obj) {
    return is_complex(obj) ? static_cast<a_complex*>(obj) : nullptr;
}

/**
 * @brief Extract complex pointer (returns nullptr if not complex).
 */
inline const a_complex* as_complex(const Object* obj) {
    return is_complex(obj) ? static_cast<const a_complex*>(obj) : nullptr;
}

} } // namespace zombie::runtime
