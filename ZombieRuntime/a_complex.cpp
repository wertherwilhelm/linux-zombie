#include "pch.h"
#include "a_complex.h"
#include "numeric_runtime.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include "atom.h"
#include <stdexcept>

namespace zombie { namespace runtime {

// ============================================================================
// Constructor
// ============================================================================

a_complex::a_complex(Object* real_part, Object* imag_part)
    : real_(real_part), imag_(imag_part) {
    if (!numeric::is_number(real_part) || !numeric::is_number(imag_part)) {
        throw std::runtime_error("Complex number requires numeric components");
    }
}

a_complex::a_complex(const a_complex& other)
    : real_(other.real_), imag_(other.imag_) {
}

// ============================================================================
// Object interface
// ============================================================================

Object* a_complex::clone() const {
    return gc_new<a_complex>(real_, imag_);
}

void a_complex::print(std::ostream& out) const {
    // Print in format: a+bi or a-bi
    real_->print(out);

    // Extract imaginary value to determine sign
    double imag_val = numeric::to_double(imag_);

    if (imag_val >= 0) {
        out << "+";
    }
    // If negative, the minus sign will be printed by imag_->print()

    imag_->print(out);
    out << "i";
}

void a_complex::trace_references(Tracer& tracer) {
    // Complex numbers contain Object references
    tracer.visit_field(real_);
    tracer.visit_field(imag_);
}

// ============================================================================
// Predicates
// ============================================================================

bool a_complex::is_real() const {
    // Complex is real if imaginary part is zero
    double imag_val = numeric::to_double(imag_);
    return imag_val == 0.0;
}

bool a_complex::is_exact() const {
    // Complex is exact if both components are exact
    return numeric::is_exact(real_) && numeric::is_exact(imag_);
}

// ============================================================================
// Arithmetic Operations
// ============================================================================

a_complex* a_complex::add(const a_complex* a, const a_complex* b) {
    // (a+bi) + (c+di) = (a+c) + (b+d)i
    Object* real_sum = numeric::add(a->real_, b->real_);
    Object* imag_sum = numeric::add(a->imag_, b->imag_);
    return gc_new<a_complex>(real_sum, imag_sum);
}

a_complex* a_complex::sub(const a_complex* a, const a_complex* b) {
    // (a+bi) - (c+di) = (a-c) + (b-d)i
    Object* real_diff = numeric::sub(a->real_, b->real_);
    Object* imag_diff = numeric::sub(a->imag_, b->imag_);
    return gc_new<a_complex>(real_diff, imag_diff);
}

a_complex* a_complex::mul(const a_complex* a, const a_complex* b) {
    // (a+bi) * (c+di) = (ac - bd) + (ad + bc)i
    Object* ac = numeric::mul(a->real_, b->real_);
    Object* bd = numeric::mul(a->imag_, b->imag_);
    Object* ad = numeric::mul(a->real_, b->imag_);
    Object* bc = numeric::mul(a->imag_, b->real_);

    Object* real_part = numeric::sub(ac, bd);
    Object* imag_part = numeric::add(ad, bc);

    return gc_new<a_complex>(real_part, imag_part);
}

a_complex* a_complex::div(const a_complex* a, const a_complex* b) {
    // (a+bi) / (c+di) = [(ac+bd) + (bc-ad)i] / (c²+d²)

    // Denominator: c² + d²
    Object* c_squared = numeric::mul(b->real_, b->real_);
    Object* d_squared = numeric::mul(b->imag_, b->imag_);
    Object* denom = numeric::add(c_squared, d_squared);

    // Check for zero denominator
    if (numeric::to_double(denom) == 0.0) {
        throw std::runtime_error("Division by zero");
    }

    // Numerator real part: ac + bd
    Object* ac = numeric::mul(a->real_, b->real_);
    Object* bd = numeric::mul(a->imag_, b->imag_);
    Object* num_real = numeric::add(ac, bd);

    // Numerator imaginary part: bc - ad
    Object* bc = numeric::mul(a->imag_, b->real_);
    Object* ad = numeric::mul(a->real_, b->imag_);
    Object* num_imag = numeric::sub(bc, ad);

    // Divide both parts by denominator
    Object* real_part = numeric::div(num_real, denom);
    Object* imag_part = numeric::div(num_imag, denom);

    return gc_new<a_complex>(real_part, imag_part);
}

a_complex* a_complex::negate(const a_complex* a) {
    // -(a+bi) = -a + (-b)i
    Object* real_neg = numeric::negate(a->real_);
    Object* imag_neg = numeric::negate(a->imag_);
    return gc_new<a_complex>(real_neg, imag_neg);
}

// ============================================================================
// Equality Check
// ============================================================================

bool a_complex::equal(const a_complex* a, const a_complex* b) {
    // Two complex numbers are equal if both components are equal
    return numeric::equal(a->real_, b->real_) &&
           numeric::equal(a->imag_, b->imag_);
}

} } // namespace zombie::runtime
