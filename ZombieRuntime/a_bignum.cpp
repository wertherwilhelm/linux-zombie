#include "zombie_runtime_types.h"
#include "a_bignum.h"
#include "numeric_runtime.h"   // make_integer
#include "Base.h"              // error()
#include "../GarbageCollector/gc_ptr.h"
#include <cstring>
#include <cstdint>

namespace zombie { namespace runtime {

Object* a_bignum::clone() const { return gc_new<a_bignum>(*this); }

a_bignum* a_bignum::from_i64(int64_t x) { return gc_new<a_bignum>(x); }

std::string a_bignum::to_string() const {
    size_t size = 0;
    if (mp_radix_size(&v_, 10, &size) != MP_OKAY) return "<bignum>";
    std::string buf(size, '\0');
    size_t written = 0;
    if (mp_to_radix(&v_, buf.data(), size, &written, 10) != MP_OKAY) return "<bignum>";
    return std::string(buf.c_str());  // trim at the NUL terminator mp_to_radix wrote
}

bool a_bignum::fits_i64() const {
    int bits = mp_count_bits(&v_);
    if (bits <= 63) return true;    // |x| < 2^63 <= INT64_MAX+1, so |x| <= INT64_MAX
    if (bits >= 65) return false;
    // bits == 64: only INT64_MIN (= -2^63) fits.
    if (mp_get_i64(&v_) != INT64_MIN) return false;
    mp_int tmp; mp_init(&tmp); mp_set_i64(&tmp, INT64_MIN);
    bool eq = (mp_cmp(&v_, &tmp) == MP_EQ);
    mp_clear(&tmp);
    return eq;
}

Object* a_bignum::normalized() {
    return fits_i64() ? numeric::make_integer(to_i64()) : static_cast<Object*>(this);
}

a_bignum* a_bignum::from_string(const std::string& s, int radix) {
    a_bignum* b = gc_new<a_bignum>();
    if (mp_read_radix(b->value(), s.c_str(), radix) != MP_OKAY) return nullptr;
    return b;
}

Object* a_bignum::add(const a_bignum* a, const a_bignum* b) {
    gc_root<a_bignum> r(gc_new<a_bignum>());
    if (mp_add(a->value(), b->value(), r->value()) != MP_OKAY) return error("bignum: add failed");
    return r->normalized();
}
Object* a_bignum::sub(const a_bignum* a, const a_bignum* b) {
    gc_root<a_bignum> r(gc_new<a_bignum>());
    if (mp_sub(a->value(), b->value(), r->value()) != MP_OKAY) return error("bignum: sub failed");
    return r->normalized();
}
Object* a_bignum::mul(const a_bignum* a, const a_bignum* b) {
    gc_root<a_bignum> r(gc_new<a_bignum>());
    if (mp_mul(a->value(), b->value(), r->value()) != MP_OKAY) return error("bignum: mul failed");
    return r->normalized();
}
Object* a_bignum::quotient(const a_bignum* a, const a_bignum* b) {
    if (mp_iszero(b->value())) return error("Division by zero");
    gc_root<a_bignum> q(gc_new<a_bignum>());
    if (mp_div(a->value(), b->value(), q->value(), nullptr) != MP_OKAY) return error("bignum: quotient failed");
    return q->normalized();
}
Object* a_bignum::remainder(const a_bignum* a, const a_bignum* b) {
    if (mp_iszero(b->value())) return error("Division by zero");
    gc_root<a_bignum> r(gc_new<a_bignum>());
    if (mp_div(a->value(), b->value(), nullptr, r->value()) != MP_OKAY) return error("bignum: remainder failed");
    return r->normalized();
}
Object* a_bignum::modulo(const a_bignum* a, const a_bignum* b) {
    if (mp_iszero(b->value())) return error("Division by zero");
    gc_root<a_bignum> r(gc_new<a_bignum>());
    if (mp_mod(a->value(), b->value(), r->value()) != MP_OKAY) return error("bignum: modulo failed");
    return r->normalized();
}
Object* a_bignum::neg(const a_bignum* a) {
    gc_root<a_bignum> r(gc_new<a_bignum>());
    if (mp_neg(a->value(), r->value()) != MP_OKAY) return error("bignum: neg failed");
    return r->normalized();
}
Object* a_bignum::abs(const a_bignum* a) {
    gc_root<a_bignum> r(gc_new<a_bignum>());
    if (mp_abs(a->value(), r->value()) != MP_OKAY) return error("bignum: abs failed");
    return r->normalized();
}
Object* a_bignum::expt(const a_bignum* base, int exponent) {
    gc_root<a_bignum> r(gc_new<a_bignum>());
    if (mp_expt_n(base->value(), exponent, r->value()) != MP_OKAY) return error("bignum: expt failed");
    return r->normalized();
}
int a_bignum::compare(const a_bignum* a, const a_bignum* b) {
    mp_ord o = mp_cmp(a->value(), b->value());
    return o == MP_LT ? -1 : (o == MP_GT ? 1 : 0);
}

} } // namespace zombie::runtime
