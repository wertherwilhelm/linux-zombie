#pragma once

#include "Object.h"
#include "atom.h"
#include <cstdint>
#include <iostream>

namespace zombie { namespace runtime {

/**
 * @file a_rational.h
 * @brief Exact rational number type (numerator / denominator).
 *
 * Represents exact fractions in Scheme's numeric tower. Always stored in
 * normalized form:
 * - gcd(numerator, denominator) == 1
 * - denominator > 0
 * - denominator == 1 means it's an integer value
 *
 * Examples:
 * - 6/8   -> normalized to 3/4
 * - -6/8  -> normalized to -3/4
 * - 5/1   -> represents integer 5 (still exact)
 * - 0/1   -> represents zero
 *
 * Inherits from ValueType (not Object directly) to indicate this is a
 * typed atomic value with a runtime type discriminant (Type::RATIONAL).
 */

/**
 * @brief Exact rational number (numerator / denominator).
 */
class a_rational : public ValueType {
public:
    /**
     * @brief Construct from numerator and denominator (auto-normalizes).
     */
    a_rational(int64_t numerator, int64_t denominator);

    /**
     * @brief Copy constructor.
     */
    a_rational(const a_rational& other);

    /**
     * @brief Return type code for this value.
     */
    int get_type() const override { return static_cast<int>(Type::RATIONAL); }

    /**
     * @brief Return runtime instance type code.
     */
    int instance_of() override { return static_cast<int>(Type::RATIONAL); }

    /**
     * @brief Rationals are atoms.
     */
    int is_atom() const override { return 1; }

    /**
     * @brief Rationals are not pairs.
     */
    int pair() const override { return 0; }

    /**
     * @brief Clone this rational.
     */
    Object* clone() const override;

    /**
     * @brief Print this rational to a stream.
     */
    void print(std::ostream& out) const override;

    /**
     * @brief Trace referenced objects (none for rationals).
     */
    void trace_references(Tracer& tracer) override {
        // Rationals don't contain Object references
    }

    /**
     * @brief Get numerator.
     */
    int64_t numerator() const { return num_; }

    /**
     * @brief Get denominator.
     */
    int64_t denominator() const { return den_; }

    /**
     * @brief Check if this rational is an integer (den == 1).
     */
    bool is_integer() const { return den_ == 1; }

    /**
     * @brief Convert to double (for inexact operations).
     */
    double to_double() const { return static_cast<double>(num_) / static_cast<double>(den_); }

    /**
     * @brief Convert to int64 if integer; returns 0 otherwise.
     */
    int64_t to_int64() const {
        return is_integer() ? num_ : 0;
    }

    /**
     * @brief Add two rationals.
     */
    static a_rational* add(const a_rational* a, const a_rational* b);

    /**
     * @brief Subtract two rationals.
     */
    static a_rational* sub(const a_rational* a, const a_rational* b);

    /**
     * @brief Multiply two rationals.
     */
    static a_rational* mul(const a_rational* a, const a_rational* b);

    /**
     * @brief Divide two rationals.
     */
    static a_rational* div(const a_rational* a, const a_rational* b);

    /**
     * @brief Negate a rational.
     */
    static a_rational* negate(const a_rational* a);

    /**
     * @brief Absolute value of a rational.
     */
    static a_rational* abs(const a_rational* a);

    /**
     * @brief Compare two rationals.
     * @return -1 if a<b, 0 if a==b, 1 if a>b.
     */
    static int compare(const a_rational* a, const a_rational* b);

private:
    int64_t num_;  // Numerator
    int64_t den_;  // Denominator (always > 0)

    /**
     * @brief Normalize to lowest terms and ensure den > 0.
     */
    void normalize();

    /**
     * @brief Compute GCD using Euclid's algorithm.
     */
    static int64_t gcd(int64_t a, int64_t b);
};

/**
 * @brief Check if Object is a rational.
 */
inline bool is_rational(const Object* obj) {
    return obj && obj->get_type() == static_cast<int>(Type::RATIONAL);
}

/**
 * @brief Extract rational pointer (returns nullptr if not rational).
 */
inline a_rational* as_rational(Object* obj) {
    return is_rational(obj) ? static_cast<a_rational*>(obj) : nullptr;
}

/**
 * @brief Extract rational pointer (returns nullptr if not rational).
 */
inline const a_rational* as_rational(const Object* obj) {
    return is_rational(obj) ? static_cast<const a_rational*>(obj) : nullptr;
}

} } // namespace zombie::runtime
