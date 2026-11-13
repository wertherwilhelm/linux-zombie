#include "pch.h"
#include "numeric_runtime.h"
#include "a_rational.h"
#include "a_complex.h"
#include "atom.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include <stdexcept>
#include <cmath>

namespace numeric {

// ============================================================================
// Type Predicates
// ============================================================================

bool is_number(const Object* obj) {
    if (!obj) return false;
    int t = obj->get_type();
    return t == static_cast<int>(Type::INT) || t == static_cast<int>(Type::LONG) || t == static_cast<int>(Type::DOUBLE) ||
           t == static_cast<int>(Type::RATIONAL) || t == static_cast<int>(Type::COMPLEX);
}

bool is_exact(const Object* obj) {
    if (!obj) return false;
    int t = obj->get_type();

    // Integers and rationals are exact
    if (t == static_cast<int>(Type::INT) || t == static_cast<int>(Type::LONG) || t == static_cast<int>(Type::RATIONAL)) {
        return true;
    }

    // Complex is exact if both components are exact
    if (t == static_cast<int>(Type::COMPLEX)) {
        const a_complex* c = as_complex(obj);
        return c && c->is_exact();
    }

    return false;
}

bool is_inexact(const Object* obj) {
    return ::numeric::is_number(obj) && !is_exact(obj);
}

bool is_integer_value(const Object* obj) {
    if (!obj) return false;
    int t = obj->get_type();

    // Direct integer types
    if (t == static_cast<int>(Type::INT) || t == static_cast<int>(Type::LONG)) {
        return true;
    }

    // Rational with denominator 1
    if (t == static_cast<int>(Type::RATIONAL)) {
        const a_rational* r = as_rational(obj);
        return r && r->is_integer();
    }

    return false;
}

bool is_rational_value(const Object* obj) {
    if (!obj) return false;
    int t = obj->get_type();

    // Rationals include integers
    return t == static_cast<int>(Type::INT) || t == static_cast<int>(Type::LONG) || t == static_cast<int>(Type::RATIONAL);
}

bool is_real_value(const Object* obj) {
    if (!obj) return false;
    int t = obj->get_type();

    // All numbers except complex (or complex with zero imaginary part)
    if (t == static_cast<int>(Type::INT) || t == static_cast<int>(Type::LONG) || t == static_cast<int>(Type::DOUBLE) || t == static_cast<int>(Type::RATIONAL)) {
        return true;
    }

    if (t == static_cast<int>(Type::COMPLEX)) {
        const a_complex* c = as_complex(obj);
        return c && c->is_real();
    }

    return false;
}

bool is_complex_value(const Object* obj) {
    return ::numeric::is_number(obj);  // All numbers are complex in the tower
}

// ============================================================================
// Helper: Extract numeric value as double
// ============================================================================

double to_double(Object* n) {
    if (!n) return 0.0;

    int t =n->get_type();

    switch (t) {
        case static_cast<int>(Type::INT):
            return static_cast<double>(static_cast<atom<int>*>(n)->value());
        case static_cast<int>(Type::LONG):
            return static_cast<double>(static_cast<atom<long long>*>(n)->value());
        case static_cast<int>(Type::DOUBLE):
            return static_cast<atom<double>*>(n)->value();
        case static_cast<int>(Type::RATIONAL): {
            const a_rational* r = as_rational(n);
            return r ? r->to_double() : 0.0;
        }
        case static_cast<int>(Type::COMPLEX): {
            const a_complex* c = as_complex(n);
            if (c && c->is_real()) {
                return to_double(c->real_part());
            }
            throw std::runtime_error("Cannot convert complex to double");
        }
        default:
            throw std::runtime_error("Not a number");
    }
}

int64_t to_int64(Object* n) {
    if (!n) return 0;

    int t =n->get_type();

    switch (t) {
        case static_cast<int>(Type::INT):
            return static_cast<int64_t>(static_cast<atom<int>*>(n)->value());
        case static_cast<int>(Type::LONG):
            return static_cast<atom<long long>*>(n)->value();
        case static_cast<int>(Type::RATIONAL): {
            const a_rational* r = as_rational(n);
            return r ? r->to_int64() : 0;
        }
        default:
            return static_cast<int64_t>(to_double(n));
    }
}

// ============================================================================
// Constructors
// ============================================================================

Object* make_integer(int64_t value) {
    // Use int if it fits, otherwise long
    if (value >= INT_MIN && value <= INT_MAX) {
        return gc_new<atom<int>>(static_cast<int>(value));
    } else {
        return gc_new<atom<long long>>(value);
    }
}

Object* make_rational(int64_t num, int64_t den) {
    if (den == 0) {
        throw std::runtime_error("Division by zero in rational");
    }
    return gc_new<a_rational>(num, den);
}

Object* make_complex(Object* real, Object* imag) {
    return gc_new<a_complex>(real, imag);
}

Object* make_inexact(double value) {
    return gc_new<atom<double>>(value);
}

// ============================================================================
// Arithmetic Operations (Forward declarations for complex use)
// ============================================================================

// These will be implemented below, but declared here for a_complex.cpp
Object* add(Object* a, Object* b);
Object* sub(Object* a, Object* b);
Object* mul(Object* a, Object* b);
Object* div(Object* a, Object* b);
Object* negate(Object* n);

// ============================================================================
// Type Promotion Helpers
// ============================================================================

// Determine the result type when combining two numbers
static int promote_type(int a, int b) {
    // Complex is most general
    if (a == static_cast<int>(Type::COMPLEX) || b == static_cast<int>(Type::COMPLEX)) return static_cast<int>(Type::COMPLEX);

    // Double (inexact) is next
    if (a == static_cast<int>(Type::DOUBLE) || b == static_cast<int>(Type::DOUBLE)) return static_cast<int>(Type::DOUBLE);

    // Rational is next
    if (a == static_cast<int>(Type::RATIONAL) || b == static_cast<int>(Type::RATIONAL)) return static_cast<int>(Type::RATIONAL);

    // Long is larger than int
    if (a == static_cast<int>(Type::LONG) || b == static_cast<int>(Type::LONG)) return static_cast<int>(Type::LONG);

    // Both are int
    return static_cast<int>(Type::INT);
}

// ============================================================================
// Integer Arithmetic (exact)
// ============================================================================

static Object* add_integers(int64_t a, int64_t b) {
    return make_integer(a + b);
}

static Object* sub_integers(int64_t a, int64_t b) {
    return make_integer(a - b);
}

static Object* mul_integers(int64_t a, int64_t b) {
    return make_integer(a * b);
}

// ============================================================================
// Addition (Tower-aware)
// ============================================================================

Object* add(Object* a, Object* b) {
    if (!::numeric::is_number(a) || !::numeric::is_number(b)) {
        throw std::runtime_error("Addition requires numbers");
    }

    int ta =a->get_type();
    int tb =b->get_type();
    int result_type = promote_type(ta, tb);

    // Handle complex
    if (result_type == static_cast<int>(Type::COMPLEX)) {
        // Convert both to complex if needed
        Object* real_a = (ta == static_cast<int>(Type::COMPLEX)) ? as_complex(a)->real_part() : a;
        Object* imag_a = (ta == static_cast<int>(Type::COMPLEX)) ? as_complex(a)->imag_part() : make_integer(0);
        Object* real_b = (tb == static_cast<int>(Type::COMPLEX)) ? as_complex(b)->real_part() : b;
        Object* imag_b = (tb == static_cast<int>(Type::COMPLEX)) ? as_complex(b)->imag_part() : make_integer(0);

        return make_complex(add(real_a, real_b), add(imag_a, imag_b));
    }

    // Handle inexact (double)
    if (result_type == static_cast<int>(Type::DOUBLE)) {
        return make_inexact(to_double(a) + to_double(b));
    }

    // Handle rational
    if (result_type == static_cast<int>(Type::RATIONAL)) {
        // Convert both to rational (protect during conversion)
        gc_root<a_rational> ra((ta == static_cast<int>(Type::RATIONAL)) ? as_rational(a) :
                                gc_new<a_rational>(to_int64(a), 1));
        gc_root<a_rational> rb((tb == static_cast<int>(Type::RATIONAL)) ? as_rational(b) :
                                gc_new<a_rational>(to_int64(b), 1));
        return a_rational::add(ra.get(), rb.get());
    }

    // Both are integers (int or long)
    return add_integers(to_int64(a), to_int64(b));
}

// ============================================================================
// Subtraction (Tower-aware)
// ============================================================================

Object* sub(Object* a, Object* b) {
    if (!::numeric::is_number(a) || !::numeric::is_number(b)) {
        throw std::runtime_error("Subtraction requires numbers");
    }

    int ta =a->get_type();
    int tb =b->get_type();
    int result_type = promote_type(ta, tb);

    // Handle complex
    if (result_type == static_cast<int>(Type::COMPLEX)) {
        Object* real_a = (ta == static_cast<int>(Type::COMPLEX)) ? as_complex(a)->real_part() : a;
        Object* imag_a = (ta == static_cast<int>(Type::COMPLEX)) ? as_complex(a)->imag_part() : make_integer(0);
        Object* real_b = (tb == static_cast<int>(Type::COMPLEX)) ? as_complex(b)->real_part() : b;
        Object* imag_b = (tb == static_cast<int>(Type::COMPLEX)) ? as_complex(b)->imag_part() : make_integer(0);

        return make_complex(sub(real_a, real_b), sub(imag_a, imag_b));
    }

    // Handle inexact (double)
    if (result_type == static_cast<int>(Type::DOUBLE)) {
        return make_inexact(to_double(a) - to_double(b));
    }

    // Handle rational
    if (result_type == static_cast<int>(Type::RATIONAL)) {
        a_rational* ra = (ta == static_cast<int>(Type::RATIONAL)) ? as_rational(a) :
                         gc_new<a_rational>(to_int64(a), 1);
        a_rational* rb = (tb == static_cast<int>(Type::RATIONAL)) ? as_rational(b) :
                         gc_new<a_rational>(to_int64(b), 1);
        return a_rational::sub(ra, rb);
    }

    // Both are integers
    return sub_integers(to_int64(a), to_int64(b));
}

// ============================================================================
// Multiplication (Tower-aware)
// ============================================================================

Object* mul(Object* a, Object* b) {
    if (!::numeric::is_number(a) || !::numeric::is_number(b)) {
        throw std::runtime_error("Multiplication requires numbers");
    }

    int ta =a->get_type();
    int tb =b->get_type();
    int result_type = promote_type(ta, tb);

    // Handle complex: (a+bi)(c+di) = (ac-bd) + (ad+bc)i
    if (result_type == static_cast<int>(Type::COMPLEX)) {
        Object* real_a = (ta == static_cast<int>(Type::COMPLEX)) ? as_complex(a)->real_part() : a;
        Object* imag_a = (ta == static_cast<int>(Type::COMPLEX)) ? as_complex(a)->imag_part() : make_integer(0);
        Object* real_b = (tb == static_cast<int>(Type::COMPLEX)) ? as_complex(b)->real_part() : b;
        Object* imag_b = (tb == static_cast<int>(Type::COMPLEX)) ? as_complex(b)->imag_part() : make_integer(0);

        // (ac - bd)
        Object* real_part = sub(mul(real_a, real_b), mul(imag_a, imag_b));
        // (ad + bc)
        Object* imag_part = add(mul(real_a, imag_b), mul(imag_a, real_b));

        return make_complex(real_part, imag_part);
    }

    // Handle inexact (double)
    if (result_type == static_cast<int>(Type::DOUBLE)) {
        return make_inexact(to_double(a) * to_double(b));
    }

    // Handle rational
    if (result_type == static_cast<int>(Type::RATIONAL)) {
        a_rational* ra = (ta == static_cast<int>(Type::RATIONAL)) ? as_rational(a) :
                         gc_new<a_rational>(to_int64(a), 1);
        a_rational* rb = (tb == static_cast<int>(Type::RATIONAL)) ? as_rational(b) :
                         gc_new<a_rational>(to_int64(b), 1);
        return a_rational::mul(ra, rb);
    }

    // Both are integers
    return mul_integers(to_int64(a), to_int64(b));
}

// ============================================================================
// Division (Always returns exact result if both operands are exact)
// ============================================================================

Object* div(Object* a, Object* b) {
    if (!::numeric::is_number(a) || !::numeric::is_number(b)) {
        throw std::runtime_error("Division requires numbers");
    }

    int ta =a->get_type();
    int tb =b->get_type();
    int result_type = promote_type(ta, tb);

    // Handle complex: (a+bi)/(c+di) = [(ac+bd) + (bc-ad)i] / (c²+d²)
    if (result_type == static_cast<int>(Type::COMPLEX)) {
        Object* real_a = (ta == static_cast<int>(Type::COMPLEX)) ? as_complex(a)->real_part() : a;
        Object* imag_a = (ta == static_cast<int>(Type::COMPLEX)) ? as_complex(a)->imag_part() : make_integer(0);
        Object* real_b = (tb == static_cast<int>(Type::COMPLEX)) ? as_complex(b)->real_part() : b;
        Object* imag_b = (tb == static_cast<int>(Type::COMPLEX)) ? as_complex(b)->imag_part() : make_integer(0);

        // denominator = c² + d²
        Object* denom = add(mul(real_b, real_b), mul(imag_b, imag_b));

        // numerator_real = ac + bd
        Object* num_real = add(mul(real_a, real_b), mul(imag_a, imag_b));

        // numerator_imag = bc - ad
        Object* num_imag = sub(mul(imag_a, real_b), mul(real_a, imag_b));

        return make_complex(div(num_real, denom), div(num_imag, denom));
    }

    // Handle inexact (double)
    if (result_type == static_cast<int>(Type::DOUBLE)) {
        double db = to_double(b);
        if (db == 0.0) throw std::runtime_error("Division by zero");
        return make_inexact(to_double(a) / db);
    }

    // Handle exact division (returns rational)
    // Convert both to rational and divide
    a_rational* ra = (ta == static_cast<int>(Type::RATIONAL)) ? as_rational(a) :
                     gc_new<a_rational>(to_int64(a), 1);
    a_rational* rb = (tb == static_cast<int>(Type::RATIONAL)) ? as_rational(b) :
                     gc_new<a_rational>(to_int64(b), 1);
    return a_rational::div(ra, rb);
}

// ============================================================================
// Other Arithmetic Operations
// ============================================================================

Object* negate(Object* n) {
    if (!::numeric::is_number(n)) {
        throw std::runtime_error("Negation requires a number");
    }

    int t =n->get_type();

    switch (t) {
        case static_cast<int>(Type::INT):
            return make_integer(-static_cast<atom<int>*>(n)->value());
        case static_cast<int>(Type::LONG):
            return make_integer(-static_cast<atom<long long>*>(n)->value());
        case static_cast<int>(Type::DOUBLE):
            return make_inexact(-static_cast<atom<double>*>(n)->value());
        case static_cast<int>(Type::RATIONAL):
            return a_rational::negate(as_rational(n));
        case static_cast<int>(Type::COMPLEX): {
            const a_complex* c = as_complex(n);
            return make_complex(negate(c->real_part()), negate(c->imag_part()));
        }
        default:
            throw std::runtime_error("Unknown number type");
    }
}

Object* abs_value(Object* n) {
    if (!::numeric::is_number(n)) {
        throw std::runtime_error("Absolute value requires a number");
    }

    int t =n->get_type();

    switch (t) {
        case static_cast<int>(Type::INT): {
            int val = static_cast<atom<int>*>(n)->value();
            return make_integer(val < 0 ? -val : val);
        }
        case static_cast<int>(Type::LONG): {
            long long val = static_cast<atom<long long>*>(n)->value();
            return make_integer(val < 0 ? -val : val);
        }
        case static_cast<int>(Type::DOUBLE): {
            double val = static_cast<atom<double>*>(n)->value();
            return make_inexact(std::abs(val));
        }
        case static_cast<int>(Type::RATIONAL):
            return a_rational::abs(as_rational(n));
        case static_cast<int>(Type::COMPLEX): {
            // For complex, magnitude = sqrt(re² + im²)
            const a_complex* c = as_complex(n);
            double re = to_double(c->real_part());
            double im = to_double(c->imag_part());
            return make_inexact(std::hypot(re, im));
        }
        default:
            throw std::runtime_error("Unknown number type");
    }
}

// ============================================================================
// Comparison (Real numbers only)
// ============================================================================

int compare(Object* a, Object* b) {
    if (!is_real_value(a) || !is_real_value(b)) {
        throw std::runtime_error("Comparison requires real numbers");
    }

    int ta =a->get_type();
    int tb =b->get_type();
    int result_type = promote_type(ta, tb);

    // Handle inexact comparison
    if (result_type == static_cast<int>(Type::DOUBLE)) {
        double da = to_double(a);
        double db = to_double(b);
        if (da < db) return -1;
        if (da > db) return 1;
        return 0;
    }

    // Handle rational comparison
    if (result_type == static_cast<int>(Type::RATIONAL)) {
        a_rational* ra = (ta == static_cast<int>(Type::RATIONAL)) ? as_rational(a) :
                         gc_new<a_rational>(to_int64(a), 1);
        a_rational* rb = (tb == static_cast<int>(Type::RATIONAL)) ? as_rational(b) :
                         gc_new<a_rational>(to_int64(b), 1);
        return a_rational::compare(ra, rb);
    }

    // Integer comparison
    int64_t ia = to_int64(a);
    int64_t ib = to_int64(b);
    if (ia < ib) return -1;
    if (ia > ib) return 1;
    return 0;
}

bool equal(Object* a, Object* b) {
    if (!::numeric::is_number(a) || !::numeric::is_number(b)) {
        return false;
    }

    int ta =a->get_type();
    int tb =b->get_type();

    // If both are complex, compare components
    if (ta == static_cast<int>(Type::COMPLEX) || tb == static_cast<int>(Type::COMPLEX)) {
        Object* real_a = (ta == static_cast<int>(Type::COMPLEX)) ? as_complex(a)->real_part() : a;
        Object* imag_a = (ta == static_cast<int>(Type::COMPLEX)) ? as_complex(a)->imag_part() : make_integer(0);
        Object* real_b = (tb == static_cast<int>(Type::COMPLEX)) ? as_complex(b)->real_part() : b;
        Object* imag_b = (tb == static_cast<int>(Type::COMPLEX)) ? as_complex(b)->imag_part() : make_integer(0);

        return numeric::equal(real_a, real_b) && numeric::equal(imag_a, imag_b);
    }

    // Otherwise compare as reals
    return compare(a, b) == 0;
}

// ============================================================================
// Conversion Operations
// ============================================================================

Object* to_inexact(Object* n) {
    if (!::numeric::is_number(n)) {
        throw std::runtime_error("to_inexact requires a number");
    }

    int t =n->get_type();

    // Already inexact
    if (t == static_cast<int>(Type::DOUBLE)) {
        return n;
    }

    // Complex: convert components
    if (t == static_cast<int>(Type::COMPLEX)) {
        const a_complex* c = as_complex(n);
        return make_complex(to_inexact(c->real_part()), to_inexact(c->imag_part()));
    }

    // Convert to double
    return make_inexact(to_double(n));
}

Object* to_exact(Object* n) {
    if (!::numeric::is_number(n)) {
        throw std::runtime_error("to_exact requires a number");
    }

    int t =n->get_type();

    // Already exact
    if (t == static_cast<int>(Type::INT) || t == static_cast<int>(Type::LONG) || t == static_cast<int>(Type::RATIONAL)) {
        return n;
    }

    // Complex: convert components
    if (t == static_cast<int>(Type::COMPLEX)) {
        const a_complex* c = as_complex(n);
        return make_complex(to_exact(c->real_part()), to_exact(c->imag_part()));
    }

    // Double: convert to an exact rational from the binary representation.
    // Every finite double is exactly m * 2^e, so decompose with frexp and
    // build num/den. Values whose exponent exceeds int64 range still throw.
    double d = static_cast<atom<double>*>(n)->value();
    if (std::isnan(d) || std::isinf(d)) {
        throw std::runtime_error("Cannot convert non-finite number to exact");
    }
    if (d == 0.0) {
        return make_integer(0);
    }

    int exp = 0;
    double mantissa = std::frexp(d, &exp);     // d = mantissa * 2^exp, |mantissa| in [0.5, 1)
    int64_t num = static_cast<int64_t>(std::ldexp(mantissa, 53));  // exact: 53-bit mantissa
    exp -= 53;

    // Reduce: strip trailing zero bits so the exponent is as small as possible
    while (num != 0 && (num & 1) == 0 && exp < 0) {
        num >>= 1;
        ++exp;
    }

    if (exp >= 0) {
        // Whole number: num * 2^exp must fit in int64
        if (exp >= 63 || std::abs(num) > (INT64_MAX >> exp)) {
            throw std::runtime_error("Cannot convert to exact: value too large");
        }
        return make_integer(num << exp);
    }

    if (-exp >= 63) {
        throw std::runtime_error("Cannot convert to exact: denominator too large");
    }
    return make_rational(num, int64_t(1) << -exp);
}

// ============================================================================
// Integer Division Operations
// ============================================================================

Object* quotient(Object* a, Object* b) {
    if (!is_integer_value(a) || !is_integer_value(b)) {
        throw std::runtime_error("quotient requires integers");
    }

    int64_t ia = to_int64(a);
    int64_t ib = to_int64(b);

    if (ib == 0) {
        throw std::runtime_error("Division by zero");
    }

    return make_integer(ia / ib);
}

Object* remainder(Object* a, Object* b) {
    if (!is_integer_value(a) || !is_integer_value(b)) {
        throw std::runtime_error("remainder requires integers");
    }

    int64_t ia = to_int64(a);
    int64_t ib = to_int64(b);

    if (ib == 0) {
        throw std::runtime_error("Division by zero");
    }

    return make_integer(ia % ib);
}

Object* modulo(Object* a, Object* b) {
    if (!is_integer_value(a) || !is_integer_value(b)) {
        throw std::runtime_error("modulo requires integers");
    }

    int64_t ia = to_int64(a);
    int64_t ib = to_int64(b);

    if (ib == 0) {
        throw std::runtime_error("Division by zero");
    }

    // Modulo always returns non-negative result
    int64_t result = ia % ib;
    if ((result < 0 && ib > 0) || (result > 0 && ib < 0)) {
        result += ib;
    }

    return make_integer(result);
}

// ============================================================================
// Component Accessors
// ============================================================================

Object* real_part(Object* n) {
    if (!::numeric::is_number(n)) {
        throw std::runtime_error("real_part requires a number");
    }

    if (n->get_type() == static_cast<int>(Type::COMPLEX)) {
        return as_complex(n)->real_part();
    }

    // For real numbers, return self
    return n;
}

Object* imag_part(Object* n) {
    if (!::numeric::is_number(n)) {
        throw std::runtime_error("imag_part requires a number");
    }

    if (n->get_type() == static_cast<int>(Type::COMPLEX)) {
        return as_complex(n)->imag_part();
    }

    // For real numbers, imaginary part is 0
    return make_integer(0);
}

bool is_rational_integer(const a_rational* r) {
    return r && r->is_integer();
}

} // namespace numeric
