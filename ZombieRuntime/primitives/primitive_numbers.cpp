#include "zombie_runtime_types.h"
#include "primitive_numbers.h"
#include "../native_primitive.h"
#include "../Base.h"
#include "../atom.h"
#include "../numeric_runtime.h"
#include "../a_complex.h"
#include "../a_rational.h"
#include "../a_bignum.h"
#include "../../GarbageCollector/gc_ptr.h"
#include <cmath>
#include <limits>
#include <sstream>
#include <iomanip>
#include <bitset>

/**
 * @file primitive_numbers.cpp
 * @brief Numeric primitives (R7RS 6.2), table-driven.
 */

// Constants for arithmetic operations
#define ZERO numeric::make_integer(0)
#define ONE numeric::make_integer(1)

// ============================================================================
// Shared helpers (comparison / arithmetic folds, number<->string, gcd/lcm)
// ============================================================================

// Compare numbers: op '<' '>' '=' 'L'(<=) 'G'(>=). Holds iff it holds for every
// adjacent pair (R7RS). Any NaN operand makes it #f (IEEE).
static Object* num_compare(Object *args, char op) {
    while (rest(args)->pair()) {
        Object* x = first(args);
        args = rest(args);
        Object* y = first(args);

        if (!numeric::is_number(x) || !numeric::is_number(y)) {
            return error("comparison requires numbers");
        }

        // IEEE/R7RS: every ordered comparison involving NaN is false (including
        // (= +nan.0 +nan.0)). numeric::compare returns 0 for NaN and
        // numeric::equal returns #t, which wrongly made =, <= and >= yield #t.
        if (std::isnan(num(x)) || std::isnan(num(y))) {
            return false_obj;
        }

        int cmp = (op == '=') ? (numeric::equal(x, y) ? 0 : 1) : numeric::compare(x, y);
        switch (op) {
            case '>': if (!(cmp > 0)) return false_obj; break;
            case '<': if (!(cmp < 0)) return false_obj; break;
            case '=': if (!(cmp == 0)) return false_obj; break;
            case 'L': if (!(cmp <= 0)) return false_obj; break;
            case 'G': if (!(cmp >= 0)) return false_obj; break;
            default: error("internal error: unrecognized op: " + op); break;
        }
    }
    return t;
}

// Fold arithmetic over args: op '+' '-' '*' '/' 'X'(max) 'N'(min), starting from
// `result`. The unary case ((- x) / (/ x)) arrives as rest(args) == nil, so test
// "no more args" with !pair() (negate / reciprocal), not args==NULL.
static Object* num_compute(Object *args, char op, Object* result) {
    if (args == NULL || !args->pair()) {
        switch (op) {
            case '-': return numeric::negate(result);
            case '/': { Object* one = numeric::make_integer(1); return numeric::div(one, result); }
            default:  return result;  // identity
        }
    } else {
        Object* acc = result;
        bool any_inexact = false;  // for min/max inexactness contagion
        while (args->pair()) {
            Object* x = first(args);
            args = rest(args);
            if (!numeric::is_number(x)) return error("arithmetic operation requires numbers");
            if (!numeric::is_exact(x)) any_inexact = true;
            switch (op) {
                case 'X': { int cmp = numeric::compare(x, acc); if (cmp > 0) acc = x; break; }
                case 'N': { int cmp = numeric::compare(x, acc); if (cmp < 0) acc = x; break; }
                case '+': acc = numeric::add(acc, x); break;
                case '-': acc = numeric::sub(acc, x); break;
                case '*': acc = numeric::mul(acc, x); break;
                case '/': acc = numeric::div(acc, x); break;
                default: error("internal error: unrecognized op: " + op); break;
            }
        }
        // R7RS: min/max return inexact if any argument is inexact.
        if ((op == 'X' || op == 'N') && any_inexact && numeric::is_exact(acc)) {
            acc = numeric::make_inexact(numeric::to_double(acc));
        }
        return acc;
    }
}

static Object* numberToString(Object* x, Object* y) {
    if (!numeric::is_number(x)) {
        return error("number->string: expected a number");
    }
    int radix = 10;
    if (y != NULL && y != nil) {
        if (!numeric::is_number(y)) return error("number->string: radix must be a number");
        radix = (int)num(y);
        if (radix != 2 && radix != 8 && radix != 10 && radix != 16) {
            return error("number->string: radix must be 2, 8, 10, or 16");
        }
    }
    long long value = (long long)num(x);
    std::ostringstream oss;
    if (radix == 10) {
        // The value's own printer keeps the exact/inexact distinction ("2", "1/2",
        // "2.0", "+inf.0").
        x->print(oss);
    } else {
        // Non-decimal radix: format the MAGNITUDE and prepend '-' (R7RS "-ff").
        bool neg = value < 0;
        unsigned long long mag = neg ? (unsigned long long)(-(value + 1)) + 1ULL
                                     : (unsigned long long)value;
        std::string digits;
        if (mag == 0) {
            digits = "0";
        } else if (radix == 2) {
            while (mag) { digits.insert(digits.begin(), char('0' + (mag & 1))); mag >>= 1; }
        } else if (radix == 8) {
            std::ostringstream t; t << std::oct << mag; digits = t.str();
        } else {  // radix == 16
            std::ostringstream t; t << std::hex << mag; digits = t.str();
        }
        return gc_new<a_string>(neg ? "-" + digits : digits);
    }
    return gc_new<a_string>(oss.str());
}

static Object* stringToNumber(Object* x, Object* y) {
    if (!is_string(x)) return error("string->number: expected a string");
    int radix = 10;
    if (y != NULL && y != nil) {
        if (!numeric::is_number(y)) return error("string->number: radix must be a number");
        radix = (int)numeric::to_int64(y);
        if (radix != 2 && radix != 8 && radix != 10 && radix != 16) {
            return error("string->number: radix must be 2, 8, 10, or 16");
        }
    }
    std::string str = ((a_string*)x)->value();
    if (str.empty()) return false_obj;

    // R7RS 6.2.6: the string may carry radix / exactness prefixes (#x #b #o #d
    // #e #i, combinable, e.g. "#e#xff"); the same grammar the reader accepts.
    int exactness = 0;  // 0 = as-written, 1 = exact (#e), -1 = inexact (#i)
    size_t i = 0;
    while (i + 1 < str.size() && str[i] == '#') {
        char c = (char)std::tolower((unsigned char)str[i + 1]);
        if      (c == 'x') radix = 16;
        else if (c == 'b') radix = 2;
        else if (c == 'o') radix = 8;
        else if (c == 'd') radix = 10;
        else if (c == 'e') exactness = 1;
        else if (c == 'i') exactness = -1;
        else return false_obj;
        i += 2;
    }
    std::string tok = str.substr(i);
    if (tok.empty()) return false_obj;

    if (tok == "+inf.0") return numeric::make_inexact(std::numeric_limits<double>::infinity());
    if (tok == "-inf.0") return numeric::make_inexact(-std::numeric_limits<double>::infinity());
    if (tok == "+nan.0" || tok == "-nan.0") return numeric::make_inexact(std::numeric_limits<double>::quiet_NaN());

    Object* val = numeric::parse_number_token(tok, radix, exactness);
    return val ? val : static_cast<Object*>(false_obj);
}

static long long gcd_helper(long long a, long long b) {
    a = std::abs(a); b = std::abs(b);
    while (b != 0) { long long t = b; b = a % b; a = t; }
    return a;
}

static Object* gcd(Object* args) {
    if (args == NULL || args == nil) return ZERO;
    if (!numeric::is_number(first(args))) return error("gcd: expected numbers");
    long long result = (long long)num(first(args));
    args = rest(args);
    while (args != NULL && args != nil) {
        if (!numeric::is_number(first(args))) return error("gcd: expected numbers");
        result = gcd_helper(result, (long long)num(first(args)));
        args = rest(args);
    }
    return numeric::make_integer(std::abs(result));
}

static long long lcm_helper(long long a, long long b) {
    if (a == 0 || b == 0) return 0;
    return std::abs(a * b) / gcd_helper(a, b);
}

static Object* lcm(Object* args) {
    if (args == NULL || args == nil) return ONE;
    if (!numeric::is_number(first(args))) return error("lcm: expected numbers");
    long long result = (long long)num(first(args));
    args = rest(args);
    while (args != NULL && args != nil) {
        if (!numeric::is_number(first(args))) return error("lcm: expected numbers");
        result = lcm_helper(result, (long long)num(first(args)));
        args = rest(args);
    }
    return numeric::make_integer(std::abs(result));
}

// ============================================================================
// Primitives (table-driven)
// ============================================================================

namespace {

using namespace zombie::runtime;

inline Object* arg1(Object* a) { return (a && !a->is_nil()) ? first(a) : nil; }
inline Object* arg2(Object* a) {
    return (a && !a->is_nil() && rest(a) && !rest(a)->is_nil()) ? second(a) : nil;
}

// ---- Predicates ----
Object* prim_numberq(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    return truth(x->get_type() == static_cast<int>(Type::DOUBLE) ||
                 x->get_type() == static_cast<int>(Type::INT) ||
                 x->get_type() == static_cast<int>(Type::LONG));
}
Object* prim_oddq(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (x->get_type() == static_cast<int>(Type::DOUBLE)) return truth(std::fmod(std::abs(((a_double*)x)->value()), 2.0) == 1.0);
    if (x->get_type() == static_cast<int>(Type::LONG)) return truth(std::abs(((a_long*)x)->value()) % 2 != 0);
    if (x->get_type() == static_cast<int>(Type::INT)) return truth(std::abs(((a_int*)x)->value()) % 2 != 0);
    return nil;
}
Object* prim_evenq(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (x->get_type() == static_cast<int>(Type::DOUBLE)) return truth(std::fmod(std::abs(((a_double*)x)->value()), 2.0) == 0.0);
    if (x->get_type() == static_cast<int>(Type::LONG)) return truth(std::abs(((a_long*)x)->value()) % 2 == 0);
    if (x->get_type() == static_cast<int>(Type::INT)) return truth(std::abs(((a_int*)x)->value()) % 2 == 0);
    return nil;
}
Object* prim_zeroq(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (x->get_type() == static_cast<int>(Type::DOUBLE)) return truth((abs(((a_double*)x)->value())) == 0);
    if (x->get_type() == static_cast<int>(Type::LONG)) return truth((std::abs(((a_long*)x)->value())) == 0);
    if (x->get_type() == static_cast<int>(Type::INT)) return truth((abs(((a_int*)x)->value())) == 0);
    return nil;
}
Object* prim_positiveq(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (x->get_type() == static_cast<int>(Type::DOUBLE)) return truth(((a_double*)x)->value() > 0);
    if (x->get_type() == static_cast<int>(Type::LONG)) return truth(((a_long*)x)->value() > 0);
    if (x->get_type() == static_cast<int>(Type::INT)) return truth(((a_int*)x)->value() > 0);
    return nil;
}
Object* prim_negativeq(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (x->get_type() == static_cast<int>(Type::DOUBLE)) return truth(((a_double*)x)->value() < 0);
    if (x->get_type() == static_cast<int>(Type::LONG)) return truth(((a_long*)x)->value() < 0);
    if (x->get_type() == static_cast<int>(Type::INT)) return truth(((a_int*)x)->value() < 0);
    return nil;
}
Object* prim_integerq(IEvaluator*, Object* a, Environment*) { return truth(numeric::is_integer_value(arg1(a))); }
Object* prim_exact_integerq(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    return truth(x != nullptr && (x->get_type() == static_cast<int>(Type::INT) ||
                                  x->get_type() == static_cast<int>(Type::LONG)));
}
Object* prim_nanq(IEvaluator*, Object* a, Environment*) { Object* x = arg1(a); return truth(numeric::is_number(x) && std::isnan(num(x))); }
Object* prim_infiniteq(IEvaluator*, Object* a, Environment*) { Object* x = arg1(a); return truth(numeric::is_number(x) && std::isinf(num(x))); }
Object* prim_finiteq(IEvaluator*, Object* a, Environment*) { Object* x = arg1(a); return truth(numeric::is_number(x) && std::isfinite(num(x))); }
Object* prim_exactq(IEvaluator*, Object* a, Environment*) { return truth(numeric::is_exact(arg1(a))); }
Object* prim_inexactq(IEvaluator*, Object* a, Environment*) { return truth(numeric::is_inexact(arg1(a))); }
Object* prim_rationalq(IEvaluator*, Object* a, Environment*) { return truth(numeric::is_rational_value(arg1(a))); }
Object* prim_realq(IEvaluator*, Object* a, Environment*) { return truth(numeric::is_real_value(arg1(a))); }
Object* prim_complexq(IEvaluator*, Object* a, Environment*) { return truth(numeric::is_complex_value(arg1(a))); }

// ---- Comparisons ----
Object* prim_num_eq(IEvaluator*, Object* a, Environment*) { return num_compare(a, '='); }
Object* prim_num_lt(IEvaluator*, Object* a, Environment*) { return num_compare(a, '<'); }
Object* prim_num_gt(IEvaluator*, Object* a, Environment*) { return num_compare(a, '>'); }
Object* prim_num_le(IEvaluator*, Object* a, Environment*) { return num_compare(a, 'L'); }
Object* prim_num_ge(IEvaluator*, Object* a, Environment*) { return num_compare(a, 'G'); }

// ---- Arithmetic ----
Object* prim_plus(IEvaluator*, Object* a, Environment*) { return num_compute(a, '+', ZERO); }
Object* prim_minus(IEvaluator*, Object* a, Environment*) { return num_compute(rest(a), '-', arg1(a)); }
Object* prim_times(IEvaluator*, Object* a, Environment*) { return num_compute(a, '*', ONE); }
Object* prim_divide(IEvaluator*, Object* a, Environment*) { return num_compute(rest(a), '/', arg1(a)); }
Object* prim_max(IEvaluator*, Object* a, Environment*) { return num_compute(a, 'X', arg1(a)); }
Object* prim_min(IEvaluator*, Object* a, Environment*) { return num_compute(a, 'N', arg1(a)); }

// ---- Division ----
Object* prim_quotient(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a), *y = arg2(a);
    if (!numeric::is_number(x) || !numeric::is_number(y)) return error("quotient: expected two numbers");
    return numeric::quotient(x, y);
}
Object* prim_remainder(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a), *y = arg2(a);
    if (!numeric::is_number(x) || !numeric::is_number(y)) return error("remainder: expected two numbers");
    return numeric::remainder(x, y);
}
Object* prim_modulo(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a), *y = arg2(a);
    if (!numeric::is_number(x) || !numeric::is_number(y)) return error("modulo: expected two numbers");
    return numeric::modulo(x, y);
}
Object* prim_gcd(IEvaluator*, Object* a, Environment*) { return (a == NULL || a->is_nil()) ? ZERO : gcd(a); }
Object* prim_lcm(IEvaluator*, Object* a, Environment*) { return (a == NULL || a->is_nil()) ? ONE : lcm(a); }

// ---- Rounding (exactness-preserving) ----
Object* prim_round(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_number(x)) return error("round: expected a number");
    if (numeric::is_integer_value(x)) return x;
    // ties-to-even (nearbyint under FE_TONEAREST), not std::round's away-from-zero.
    double r = std::nearbyint(numeric::to_double(x));
    return numeric::is_exact(x) ? numeric::make_integer(static_cast<int64_t>(r)) : numeric::make_inexact(r);
}
Object* prim_floor(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_number(x)) return error("floor: expected a number");
    if (numeric::is_integer_value(x)) return x;
    double r = std::floor(numeric::to_double(x));
    return numeric::is_exact(x) ? numeric::make_integer((int64_t)r) : numeric::make_inexact(r);
}
Object* prim_ceiling(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_number(x)) return error("ceiling: expected a number");
    if (numeric::is_integer_value(x)) return x;
    double r = std::ceil(numeric::to_double(x));
    return numeric::is_exact(x) ? numeric::make_integer((int64_t)r) : numeric::make_inexact(r);
}
Object* prim_truncate(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_number(x)) return error("truncate: expected a number");
    if (numeric::is_integer_value(x)) return x;
    double r = std::trunc(numeric::to_double(x));
    return numeric::is_exact(x) ? numeric::make_integer((int64_t)r) : numeric::make_inexact(r);
}

// ---- Math ----
Object* prim_abs(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_number(x)) return error("abs: expected a number");
    return numeric::abs_value(x);
}
Object* prim_sqrt(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_number(x)) return error("sqrt: expected a number");
    return numeric::make_inexact(std::sqrt(numeric::to_double(x)));
}
Object* prim_expt(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a), *y = arg2(a);
    if (!numeric::is_number(x) || !numeric::is_number(y)) return error("expt: expected two numbers");
    // Exact base + exact integer exponent -> exact result (R7RS).
    if (numeric::is_exact(x) && numeric::is_exact(y) && numeric::is_integer_value(y)) {
        int64_t e = numeric::to_int64(y);
        int64_t n = e < 0 ? -e : e;
        Object* acc = numeric::make_integer(1);
        gc_root<Object> aguard(acc);
        for (int64_t i = 0; i < n; ++i) { acc = numeric::mul(acc, x); aguard = gc_root<Object>(acc); }
        if (e < 0) return numeric::div(numeric::make_integer(1), acc);  // exact 1/x^|e|
        return acc;
    }
    return numeric::make_inexact(std::pow(numeric::to_double(x), numeric::to_double(y)));
}

// ---- Transcendental (all inexact) ----
#define UNARY_MATH(fn, who, cfn) \
    Object* fn(IEvaluator*, Object* a, Environment*) { \
        Object* x = arg1(a); \
        if (!numeric::is_number(x)) return error(who ": expected a number"); \
        return numeric::make_inexact(cfn(numeric::to_double(x))); \
    }
UNARY_MATH(prim_exp,  "exp",  std::exp)
UNARY_MATH(prim_log,  "log",  std::log)
UNARY_MATH(prim_sin,  "sin",  std::sin)
UNARY_MATH(prim_cos,  "cos",  std::cos)
UNARY_MATH(prim_tan,  "tan",  std::tan)
UNARY_MATH(prim_asin, "asin", std::asin)
UNARY_MATH(prim_acos, "acos", std::acos)
UNARY_MATH(prim_atan, "atan", std::atan)
#undef UNARY_MATH

// ---- Tower conversions ----
Object* prim_exact_to_inexact(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_number(x)) return error("exact->inexact: expected a number");
    return numeric::to_inexact(x);
}
Object* prim_inexact_to_exact(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_number(x)) return error("inexact->exact: expected a number");
    try { return numeric::to_exact(x); }
    catch (const std::exception& e) { return error(std::string("inexact->exact: ") + e.what()); }
}

// ---- Rational ----
Object* prim_numerator(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_rational_value(x)) return error("numerator: expected a rational number");
    if (x->get_type() == static_cast<int>(Type::RATIONAL)) return numeric::make_integer(static_cast<a_rational*>(x)->numerator());
    return x;
}
Object* prim_denominator(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_rational_value(x)) return error("denominator: expected a rational number");
    if (x->get_type() == static_cast<int>(Type::RATIONAL)) return numeric::make_integer(static_cast<a_rational*>(x)->denominator());
    return ONE;
}
Object* prim_rationalize(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a), *y = arg2(a);
    if (!numeric::is_number(x) || !numeric::is_number(y)) return error("rationalize: expected two numbers");
    double val = numeric::to_double(x);
    double eps = numeric::to_double(y);
    if (eps < 0) eps = -eps;
    if (eps == 0) return x;
    double lower = val - eps, upper = val + eps;
    int64_t p0 = 0, q0 = 1, p1 = 1, q1 = 0;  // Stern-Brocot mediant search
    while (true) {
        int64_t p = p0 + p1, q = q0 + q1;
        if (q > 1000000) break;
        double mediant = (double)p / (double)q;
        if (mediant < lower) { p0 = p; q0 = q; }
        else if (mediant > upper) { p1 = p; q1 = q; }
        else return numeric::make_inexact(mediant);
    }
    return x;
}

// ---- Complex ----
Object* prim_real_part(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_number(x)) return error("real-part: expected a number");
    return numeric::real_part(x);
}
Object* prim_imag_part(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_number(x)) return error("imag-part: expected a number");
    return numeric::imag_part(x);
}
Object* prim_make_rectangular(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a), *y = arg2(a);
    if (!numeric::is_number(x)) return error("make-rectangular: expected a number for real part");
    if (!numeric::is_number(y)) return error("make-rectangular: expected a number for imaginary part");
    if (numeric::equal(y, ZERO)) return x;
    return gc_new<a_complex>(x, y);
}
Object* prim_make_polar(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a), *y = arg2(a);
    if (!numeric::is_number(x)) return error("make-polar: expected a number for magnitude");
    if (!numeric::is_number(y)) return error("make-polar: expected a number for angle");
    double mag = numeric::to_double(x), angle = numeric::to_double(y);
    Object* real_part = gc_new<a_double>(mag * std::cos(angle));
    Object* imag_part = gc_new<a_double>(mag * std::sin(angle));
    if (std::abs(mag * std::sin(angle)) < 1e-10) return real_part;
    return gc_new<a_complex>(real_part, imag_part);
}
Object* prim_magnitude(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_number(x)) return error("magnitude: expected a number");
    if (is_complex(x)) {
        a_complex* c = as_complex(x);
        double real = numeric::to_double(c->real_part()), imag = numeric::to_double(c->imag_part());
        return gc_new<a_double>(std::sqrt(real * real + imag * imag));
    }
    return numeric::abs_value(x);
}
Object* prim_angle(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_number(x)) return error("angle: expected a number");
    if (is_complex(x)) {
        a_complex* c = as_complex(x);
        return gc_new<a_double>(std::atan2(numeric::to_double(c->imag_part()), numeric::to_double(c->real_part())));
    }
    double val = numeric::to_double(x);
    return gc_new<a_double>(val >= 0 ? 0.0 : 3.14159265358979323846);
}

// ---- String conversions ----
Object* prim_number_to_string(IEvaluator*, Object* a, Environment*) { return numberToString(arg1(a), arg2(a)); }
Object* prim_string_to_number(IEvaluator*, Object* a, Environment*) { return stringToNumber(arg1(a), arg2(a)); }

// ---- R7RS additions ----
Object* prim_exact(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_number(x)) return error("exact: expected a number");
    try { return numeric::to_exact(x); }
    catch (const std::exception& e) { return error(std::string("exact: ") + e.what()); }
}
Object* prim_inexact(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!numeric::is_number(x)) return error("inexact: expected a number");
    return numeric::to_inexact(x);
}
Object* prim_square(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!x || x->is_nil()) return error("square: requires one argument");
    if (!numeric::is_number(x)) return error("square: expected a number");
    return num_compute(cons(x, cons(x, nil)), '*', ONE);
}
Object* prim_exact_integer_sqrt(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (!x || x->is_nil()) return error("exact-integer-sqrt: requires one argument");
    if (!numeric::is_integer_value(x)) return error("exact-integer-sqrt: argument must be an exact non-negative integer");
    // Arbitrary-precision path: isqrt via libtommath, remainder r = x - s*s.
    if (zombie::runtime::is_bignum(x)) {
        const zombie::runtime::a_bignum* bx = zombie::runtime::as_bignum(x);
        if (mp_isneg(bx->value())) return error("exact-integer-sqrt: argument must be non-negative");
        gc_root<zombie::runtime::a_bignum> s(gc_new<zombie::runtime::a_bignum>());
        if (mp_sqrt(bx->value(), s->value()) != MP_OKAY) return error("exact-integer-sqrt: failed");
        gc_root<zombie::runtime::a_bignum> sq(gc_new<zombie::runtime::a_bignum>());
        gc_root<zombie::runtime::a_bignum> r(gc_new<zombie::runtime::a_bignum>());
        mp_mul(s->value(), s->value(), sq->value());
        mp_sub(bx->value(), sq->value(), r->value());
        return gc_new<ValuesObject>(cons(s->normalized(), cons(r->normalized(), nil)));
    }
    double val = num(x);
    if (val < 0) return error("exact-integer-sqrt: argument must be non-negative");
    long long s = (long long)std::floor(std::sqrt(val));
    // Correct for double-rounding near perfect squares.
    while ((s + 1) * (s + 1) <= (long long)val) ++s;
    while (s * s > (long long)val) --s;
    long long r = (long long)val - (s * s);
    // R7RS: returns TWO values (s and r).
    return gc_new<ValuesObject>(cons(numeric::make_integer(s), cons(numeric::make_integer(r), nil)));
}
// Shared body for floor/ and truncate/ (both return two values: quotient, remainder).
Object* div_two_values(Object* a, const char* who, double (*qfn)(double)) {
    Object* x = arg1(a), *y = arg2(a);
    if (!x || x->is_nil() || !y || y->is_nil()) return error(std::string(who) + ": requires two arguments");
    double dividend = num(x), divisor = num(y);
    if (divisor == 0.0) return error(std::string(who) + ": division by zero");
    bool exact = numeric::is_integer_value(x) && numeric::is_integer_value(y);
    double q = qfn(dividend / divisor);
    double r = dividend - divisor * q;
    Object* qo = exact ? numeric::make_integer((long long)q) : numeric::make_inexact(q);
    Object* ro = exact ? numeric::make_integer((long long)r) : numeric::make_inexact(r);
    return gc_new<ValuesObject>(cons(qo, cons(ro, nil)));
}
Object* prim_floor_div(IEvaluator*, Object* a, Environment*) { return div_two_values(a, "floor/", std::floor); }
Object* prim_truncate_div(IEvaluator*, Object* a, Environment*) { return div_two_values(a, "truncate/", std::trunc); }

// Shared body for floor-quotient/remainder and truncate-quotient/remainder.
Object* div_single(Object* a, const char* who, double (*qfn)(double), bool want_remainder) {
    Object* x = arg1(a), *y = arg2(a);
    if (!x || x->is_nil() || !y || y->is_nil()) return error(std::string(who) + ": requires two arguments");
    double dividend = num(x), divisor = num(y);
    if (divisor == 0.0) return error(std::string(who) + ": division by zero");
    double q = qfn(dividend / divisor);
    double result = want_remainder ? (dividend - divisor * q) : q;
    if (numeric::is_integer_value(x) && numeric::is_integer_value(y)) return numeric::make_integer((int)result);
    return numeric::make_inexact(result);
}
Object* prim_floor_quotient(IEvaluator*, Object* a, Environment*) { return div_single(a, "floor-quotient", std::floor, false); }
Object* prim_floor_remainder(IEvaluator*, Object* a, Environment*) { return div_single(a, "floor-remainder", std::floor, true); }
Object* prim_truncate_quotient(IEvaluator*, Object* a, Environment*) { return div_single(a, "truncate-quotient", std::trunc, false); }
Object* prim_truncate_remainder(IEvaluator*, Object* a, Environment*) { return div_single(a, "truncate-remainder", std::trunc, true); }

}  // namespace

void PrimitiveNumbers::install(zombie::runtime::Environment* env) {
    using namespace zombie::runtime;
    static const NativeEntry entries[] = {
        // Predicates
        { "number?",   1, 1, prim_numberq },
        { "odd?",      1, 1, prim_oddq },
        { "even?",     1, 1, prim_evenq },
        { "zero?",     1, 1, prim_zeroq },
        { "positive?", 1, 1, prim_positiveq },
        { "negative?", 1, 1, prim_negativeq },
        { "integer?",  1, 1, prim_integerq },
        { "exact-integer?", 1, 1, prim_exact_integerq },
        { "nan?",      1, 1, prim_nanq },
        { "infinite?", 1, 1, prim_infiniteq },
        { "finite?",   1, 1, prim_finiteq },
        { "exact?",    1, 1, prim_exactq },
        { "inexact?",  1, 1, prim_inexactq },
        { "rational?", 1, 1, prim_rationalq },
        { "real?",     1, 1, prim_realq },
        { "complex?",  1, 1, prim_complexq },
        // Comparisons
        { "=",  2, 999999, prim_num_eq },
        { "<",  2, 999999, prim_num_lt },
        { ">",  2, 999999, prim_num_gt },
        { "<=", 2, 999999, prim_num_le },
        { ">=", 2, 999999, prim_num_ge },
        // Arithmetic
        { "+",   0, 999999, prim_plus },
        { "-",   1, 999999, prim_minus },
        { "*",   0, 999999, prim_times },
        { "/",   1, 999999, prim_divide },
        { "max", 1, 999999, prim_max },
        { "min", 1, 999999, prim_min },
        // Division
        { "quotient",  2, 2, prim_quotient },
        { "remainder", 2, 2, prim_remainder },
        { "modulo",    2, 2, prim_modulo },
        { "gcd",       0, 999999, prim_gcd },
        { "lcm",       0, 999999, prim_lcm },
        // Rounding
        { "round",    1, 1, prim_round },
        { "floor",    1, 1, prim_floor },
        { "ceiling",  1, 1, prim_ceiling },
        { "truncate", 1, 1, prim_truncate },
        // Math
        { "abs",  1, 1, prim_abs },
        { "sqrt", 1, 1, prim_sqrt },
        { "expt", 2, 2, prim_expt },
        // Transcendental
        { "exp",  1, 1, prim_exp },
        { "log",  1, 1, prim_log },
        { "sin",  1, 1, prim_sin },
        { "cos",  1, 1, prim_cos },
        { "tan",  1, 1, prim_tan },
        { "asin", 1, 1, prim_asin },
        { "acos", 1, 1, prim_acos },
        { "atan", 1, 1, prim_atan },
        // Conversions
        { "exact->inexact", 1, 1, prim_exact_to_inexact },
        { "inexact->exact", 1, 1, prim_inexact_to_exact },
        // Rational
        { "numerator",   1, 1, prim_numerator },
        { "denominator", 1, 1, prim_denominator },
        { "rationalize", 2, 2, prim_rationalize },
        // Complex
        { "real-part",        1, 1, prim_real_part },
        { "imag-part",        1, 1, prim_imag_part },
        { "make-rectangular", 2, 2, prim_make_rectangular },
        { "make-polar",       2, 2, prim_make_polar },
        { "magnitude",        1, 1, prim_magnitude },
        { "angle",            1, 1, prim_angle },
        // String conversions
        { "number->string", 1, 2, prim_number_to_string },
        { "string->number", 1, 2, prim_string_to_number },
        // R7RS additions
        { "exact",              1, 1, prim_exact },
        { "inexact",            1, 1, prim_inexact },
        { "square",             1, 1, prim_square },
        { "exact-integer-sqrt", 1, 1, prim_exact_integer_sqrt },
        { "floor/",             2, 2, prim_floor_div },
        { "truncate/",          2, 2, prim_truncate_div },
        { "floor-quotient",     2, 2, prim_floor_quotient },
        { "floor-remainder",    2, 2, prim_floor_remainder },
        { "truncate-quotient",  2, 2, prim_truncate_quotient },
        { "truncate-remainder", 2, 2, prim_truncate_remainder },
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}
