#pragma once

#include "Object.h"
#include "atom.h"
#include "tommath.h"
#include <cstdint>
#include <string>
#include <iostream>

namespace zombie { namespace runtime {

/**
 * @file a_bignum.h
 * @brief Arbitrary-precision exact integer (wraps a libtommath mp_int).
 *
 * The fixnum types (a_int / a_long) cover values that fit in int64; a_bignum
 * holds integers beyond that range. Arithmetic that overflows int64 promotes to
 * a_bignum, and a bignum result that fits back into int64 is demoted to a fixnum
 * (see normalized()), so a value has exactly one representation for a given
 * magnitude. Inherits ValueType (Type::BIGNUM), like a_rational.
 */
class a_bignum : public ValueType {
public:
    a_bignum() { mp_init(&v_); }
    explicit a_bignum(int64_t x) { mp_init(&v_); mp_set_i64(&v_, x); }
    a_bignum(const a_bignum& other) { mp_init_copy(&v_, &other.v_); }
    ~a_bignum() override { mp_clear(&v_); }

    int get_type() const override { return static_cast<int>(Type::BIGNUM); }
    int instance_of() override { return static_cast<int>(Type::BIGNUM); }
    int is_atom() const override { return 1; }
    int pair() const override { return 0; }
    Object* clone() const override;
    void print(std::ostream& out) const override { out << to_string(); }
    void trace_references(Tracer&) override {}  // no Object references

    mp_int* value() { return &v_; }
    const mp_int* value() const { return &v_; }

    std::string to_string() const;              // decimal
    double to_double() const { return mp_get_double(&v_); }
    bool fits_i64() const;
    int64_t to_i64() const { return mp_get_i64(&v_); }

    /// Demote to a fixnum (a_int / a_long) if the value fits int64, else return this.
    Object* normalized();

    /// Parse a signed integer string in the given radix; nullptr on parse failure.
    static a_bignum* from_string(const std::string& s, int radix = 10);
    static a_bignum* from_i64(int64_t x);

    // Each returns a NORMALIZED result (demoted to a fixnum when it fits int64).
    // Division operations return an error Object when the divisor is zero.
    static Object* add(const a_bignum* a, const a_bignum* b);
    static Object* sub(const a_bignum* a, const a_bignum* b);
    static Object* mul(const a_bignum* a, const a_bignum* b);
    static Object* quotient(const a_bignum* a, const a_bignum* b);   // truncates toward zero
    static Object* remainder(const a_bignum* a, const a_bignum* b);  // sign of dividend
    static Object* modulo(const a_bignum* a, const a_bignum* b);     // sign of divisor
    static Object* neg(const a_bignum* a);
    static Object* abs(const a_bignum* a);
    static Object* expt(const a_bignum* base, int exponent);         // exponent >= 0
    static int compare(const a_bignum* a, const a_bignum* b);        // -1 / 0 / 1

private:
    mp_int v_;
};

inline bool is_bignum(const Object* o) {
    return o && o->get_type() == static_cast<int>(Type::BIGNUM);
}
inline a_bignum* as_bignum(Object* o) {
    return is_bignum(o) ? static_cast<a_bignum*>(o) : nullptr;
}
inline const a_bignum* as_bignum(const Object* o) {
    return is_bignum(o) ? static_cast<const a_bignum*>(o) : nullptr;
}

} } // namespace zombie::runtime
