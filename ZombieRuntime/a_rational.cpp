#include "pch.h"
#include "a_rational.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include <stdexcept>
#include <cmath>

namespace zombie { namespace runtime {

// ============================================================================
// Constructor and Normalization
// ============================================================================

a_rational::a_rational(int64_t numerator, int64_t denominator)
    : num_(numerator), den_(denominator) {
    if (den_ == 0) {
        throw std::runtime_error("Division by zero in rational number");
    }
    normalize();
}

a_rational::a_rational(const a_rational& other)
    : num_(other.num_), den_(other.den_) {
}

void a_rational::normalize() {
    // Ensure denominator is positive
    if (den_ < 0) {
        num_ = -num_;
        den_ = -den_;
    }

    // Handle zero numerator
    if (num_ == 0) {
        den_ = 1;
        return;
    }

    // Reduce to lowest terms
    int64_t g = gcd(std::abs(num_), den_);
    num_ /= g;
    den_ /= g;
}

int64_t a_rational::gcd(int64_t a, int64_t b) {
    // Euclid's algorithm
    while (b != 0) {
        int64_t temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

// ============================================================================
// Object interface
// ============================================================================

Object* a_rational::clone() const {
    return gc_new<a_rational>(*this);
}

void a_rational::print(std::ostream& out) const {
    if (den_ == 1) {
        // Print as integer if denominator is 1
        out << num_;
    } else {
        out << num_ << "/" << den_;
    }
}

// ============================================================================
// Arithmetic Operations
// ============================================================================

a_rational* a_rational::add(const a_rational* a, const a_rational* b) {
    // a/b + c/d = (ad + bc) / bd
    int64_t num = a->num_ * b->den_ + b->num_ * a->den_;
    int64_t den = a->den_ * b->den_;
    return gc_new<a_rational>(num, den);
}

a_rational* a_rational::sub(const a_rational* a, const a_rational* b) {
    // a/b - c/d = (ad - bc) / bd
    int64_t num = a->num_ * b->den_ - b->num_ * a->den_;
    int64_t den = a->den_ * b->den_;
    return gc_new<a_rational>(num, den);
}

a_rational* a_rational::mul(const a_rational* a, const a_rational* b) {
    // (a/b) * (c/d) = (ac) / (bd)
    int64_t num = a->num_ * b->num_;
    int64_t den = a->den_ * b->den_;
    return gc_new<a_rational>(num, den);
}

a_rational* a_rational::div(const a_rational* a, const a_rational* b) {
    if (b->num_ == 0) {
        throw std::runtime_error("Division by zero");
    }
    // (a/b) / (c/d) = (a/b) * (d/c) = (ad) / (bc)
    int64_t num = a->num_ * b->den_;
    int64_t den = a->den_ * b->num_;
    return gc_new<a_rational>(num, den);
}

a_rational* a_rational::negate(const a_rational* a) {
    return gc_new<a_rational>(-a->num_, a->den_);
}

a_rational* a_rational::abs(const a_rational* a) {
    return gc_new<a_rational>(std::abs(a->num_), a->den_);
}

// ============================================================================
// Comparison
// ============================================================================

int a_rational::compare(const a_rational* a, const a_rational* b) {
    // Compare a/b with c/d by comparing ad with bc
    int64_t lhs = a->num_ * b->den_;
    int64_t rhs = b->num_ * a->den_;

    if (lhs < rhs) return -1;
    if (lhs > rhs) return 1;
    return 0;
}

} } // namespace zombie::runtime
