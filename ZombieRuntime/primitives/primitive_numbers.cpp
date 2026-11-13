#include "pch.h"
#include "../pch.h"
#include "primitive_numbers.h"
#include "../Base.h"
#include "../atom.h"
#include "../numeric_runtime.h"
#include "../a_complex.h"
#include "../a_rational.h"
#include "../../GarbageCollector/gc_ptr.h"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <bitset>

/**
 * @file primitive_numbers.cpp
 * @brief Implementation of numeric primitives (R5RS Section 6.5)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 */

// Constants for arithmetic operations
#define ZERO numeric::make_integer(0)
#define ONE numeric::make_integer(1)

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Compare numbers using the specified operation
 * @param args List of numbers to compare
 * @param op Comparison operator: '<', '>', '=', 'L' (<=), 'G' (>=)
 * @return #t if comparison holds for all adjacent pairs, #f otherwise
 */
static Object* num_compare(Object *args, char op) {
    // Use numeric runtime for tower-aware comparison
    while (rest(args)->pair()) {
        Object* x = first(args);
        args = rest(args);
        Object* y = first(args);

        // Validate both are numbers
        if (!numeric::is_number(x) || !numeric::is_number(y)) {
            return error("comparison requires numbers");
        }

        // Use numeric::compare (returns -1 if x<y, 0 if x==y, 1 if x>y)
        // For equality, use numeric::equal which works with complex too
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
    // R5RS: return #t singleton
    return t;
}

/**
 * @brief Compute arithmetic operations on a list of numbers
 * @param args List of arguments
 * @param op Operator: '+', '-', '*', '/', 'X' (max), 'N' (min)
 * @param result Initial accumulator value
 * @return Result of the computation
 */
static Object* num_compute(Object *args, char op, Object* result) {
    // Use numeric runtime for tower-aware arithmetic
    if (args == NULL) {
        // Unary operations on result
        switch (op) {
            case '-': return numeric::negate(result);
            case '/': {
                // 1/x
                Object* one = numeric::make_integer(1);
                return numeric::div(one, result);
            }
            default:  return result;  // Identity
        }
    } else {
        Object* acc = result;

        while (args->pair()) {
            Object* x = first(args);
            args = rest(args);

            // Validate it's a number
            if (!numeric::is_number(x)) {
                return error("arithmetic operation requires numbers");
            }

            switch (op) {
                case 'X': {  // max
                    int cmp = numeric::compare(x, acc);
                    if (cmp > 0) acc = x;
                    break;
                }
                case 'N': {  // min
                    int cmp = numeric::compare(x, acc);
                    if (cmp < 0) acc = x;
                    break;
                }
                case '+': acc = numeric::add(acc, x); break;
                case '-': acc = numeric::sub(acc, x); break;
                case '*': acc = numeric::mul(acc, x); break;
                case '/': acc = numeric::div(acc, x); break;
                default: error("internal error: unrecognized op: " + op); break;
            }
        }
        return acc;
    }
}

/**
 * @brief Convert number to string representation
 * @param x Number to convert
 * @param y Optional radix (2, 8, 10, or 16)
 * @return String representation
 */
static Object* numberToString(Object* x, Object* y) {
    if (!numeric::is_number(x)) {
        return error("number->string: expected a number");
    }

    int radix = 10;
    if (y != NULL && y != nil) {
        if (!numeric::is_number(y)) {
            return error("number->string: radix must be a number");
        }
        radix = (int)num(y);
        if (radix != 2 && radix != 8 && radix != 10 && radix != 16) {
            return error("number->string: radix must be 2, 8, 10, or 16");
        }
    }

    long long value = (long long)num(x);
    std::ostringstream oss;

    if (radix == 10) {
        // For Base 10, handle floating point
        double dval = num(x);
        if (dval == (double)value) {
            // Integer value
            oss << value;
        } else {
            // Floating point value
            oss << dval;
        }
    } else {
        // For other bases, use integer conversion
        if (radix == 2) {
            oss << std::bitset<64>(value);
            // Remove leading zeros
            std::string str = oss.str();
            size_t pos = str.find('1');
            if (pos != std::string::npos) {
                return gc_new<a_string>(str.substr(pos));
            } else {
                return gc_new<a_string>("0");
            }
        } else if (radix == 8) {
            oss << std::oct << value;
        } else if (radix == 16) {
            oss << std::hex << value;
        }
    }

    return gc_new<a_string>(oss.str());
}

/**
 * @brief Convert string to number
 * @param x String to convert
 * @param y Optional radix (2, 8, 10, or 16)
 * @return Number or #f if conversion fails
 */
static Object* stringToNumber(Object* x, Object* y) {
    if (!is_string(x)) {
        return error("string->number: expected a string");
    }

    int radix = 10;
    if (y != NULL && y != nil) {
        if (!is_double(y)) {
            return error("string->number: radix must be a number");
        }
        radix = (int)num(y);
        if (radix != 2 && radix != 8 && radix != 10 && radix != 16) {
            return error("string->number: radix must be 2, 8, 10, or 16");
        }
    }

    std::string str = ((a_string*)x)->value();

    try {
        if (radix == 10) {
            // Try to parse as double
            size_t pos;
            double value = std::stod(str, &pos);
            if (pos == str.length()) {
                return numeric::make_inexact(value);
            }
        } else {
            // Parse as integer in given radix
            size_t pos;
            long long value = std::stoll(str, &pos, radix);
            if (pos == str.length()) {
                return numeric::make_integer(value);
            }
        }
    } catch (...) {
        // Return #f for invalid conversion
        return nil;  // R5RS: string->number returns #f on failure
    }

    // Return #f if parsing failed
    return nil;
}

/**
 * @brief Compute GCD of two numbers
 */
static long long gcd_helper(long long a, long long b) {
    a = std::abs(a);
    b = std::abs(b);
    while (b != 0) {
        long long temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

/**
 * @brief Compute GCD of multiple numbers
 */
static Object* gcd(Object* args) {
    if (args == NULL || args == nil) {
        return ZERO;
    }

    // Get first number
    if (!numeric::is_number(first(args))) {
        return error("gcd: expected numbers");
    }

    long long result = (long long)num(first(args));
    args = rest(args);

    while (args != NULL && args != nil) {
        if (!numeric::is_number(first(args))) {
            return error("gcd: expected numbers");
        }
        long long next = (long long)num(first(args));
        result = gcd_helper(result, next);
        args = rest(args);
    }

    return numeric::make_integer(std::abs(result));
}

/**
 * @brief Compute LCM of two numbers
 */
static long long lcm_helper(long long a, long long b) {
    if (a == 0 || b == 0) {
        return 0;
    }
    return std::abs(a * b) / gcd_helper(a, b);
}

/**
 * @brief Compute LCM of multiple numbers
 */
static Object* lcm(Object* args) {
    if (args == NULL || args == nil) {
        return ONE;
    }

    // Get first number
    if (!numeric::is_number(first(args))) {
        return error("lcm: expected numbers");
    }

    long long result = (long long)num(first(args));
    args = rest(args);

    while (args != NULL && args != nil) {
        if (!numeric::is_number(first(args))) {
            return error("lcm: expected numbers");
        }
        long long next = (long long)num(first(args));
        result = lcm_helper(result, next);
        args = rest(args);
    }

    return numeric::make_integer(std::abs(result));
}

/**
 * @brief Check if a number is exact
 */
static bool isExact(Object* x) {
    return numeric::is_exact(x);
}

// ============================================================================
// Constructor
// ============================================================================

PrimitiveNumbers::PrimitiveNumbers(ID id, int minArgs, int maxArgs)
    : PrimitiveBase(minArgs, maxArgs), m_id(id) {
    // Set Primitive name based on ID
    switch (id) {
        // Predicates
        case NUMBERQ:    m_name = "number?"; break;
        case ODDQ:       m_name = "odd?"; break;
        case EVENQ:      m_name = "even?"; break;
        case ZEROQ:      m_name = "zero?"; break;
        case POSITIVEQ:  m_name = "positive?"; break;
        case NEGATIVEQ:  m_name = "negative?"; break;
        case INTEGERQ:   m_name = "integer?"; break;
        case EXACTQ:     m_name = "exact?"; break;
        case INEXACTQ:   m_name = "inexact?"; break;
        case RATIONALQ:  m_name = "rational?"; break;
        case REALQ:      m_name = "real?"; break;
        case COMPLEXQ:   m_name = "complex?"; break;

        // Comparisons
        case EQ:         m_name = "="; break;
        case LT:         m_name = "<"; break;
        case GT:         m_name = ">"; break;
        case LE:         m_name = "<="; break;
        case GE:         m_name = ">="; break;

        // Arithmetic
        case PLUS:       m_name = "+"; break;
        case MINUS:      m_name = "-"; break;
        case TIMES:      m_name = "*"; break;
        case DIVIDE:     m_name = "/"; break;
        case MAX:        m_name = "max"; break;
        case MIN:        m_name = "min"; break;

        // Division
        case QUOTIENT:   m_name = "quotient"; break;
        case REMAINDER:  m_name = "remainder"; break;
        case MODULO:     m_name = "modulo"; break;
        case GCD:        m_name = "gcd"; break;
        case LCM:        m_name = "lcm"; break;

        // Rounding
        case ROUND:      m_name = "round"; break;
        case FLOOR:      m_name = "floor"; break;
        case CEILING:    m_name = "ceiling"; break;
        case TRUNCATE:   m_name = "truncate"; break;

        // Math
        case ABS:        m_name = "abs"; break;
        case SQRT:       m_name = "sqrt"; break;
        case EXPT:       m_name = "expt"; break;

        // Transcendental
        case EXP:        m_name = "exp"; break;
        case LOG:        m_name = "log"; break;
        case SIN:        m_name = "sin"; break;
        case COS:        m_name = "cos"; break;
        case TAN:        m_name = "tan"; break;
        case ASIN:       m_name = "asin"; break;
        case ACOS:       m_name = "acos"; break;
        case ATAN:       m_name = "atan"; break;

        // Conversions
        case EXACTTOINEXACT: m_name = "exact->inexact"; break;
        case INEXACTTOEXACT: m_name = "inexact->exact"; break;

        // Rational
        case NUMERATOR:   m_name = "numerator"; break;
        case DENOMINATOR: m_name = "denominator"; break;
        case RATIONALIZE: m_name = "rationalize"; break;

        // Complex
        case REALPART:        m_name = "real-part"; break;
        case IMAGPART:        m_name = "imag-part"; break;
        case MAKERECTANGULAR: m_name = "make-rectangular"; break;
        case MAKEPOLAR:       m_name = "make-polar"; break;
        case MAGNITUDE:       m_name = "magnitude"; break;
        case ANGLE:           m_name = "angle"; break;

        // String conversions
        case NUMBERTOSTRING: m_name = "number->string"; break;
        case STRINGTONUMBER: m_name = "string->number"; break;

        // R7RS additions
        case EXACT:          m_name = "exact"; break;
        case INEXACT:        m_name = "inexact"; break;
        case SQUARE:         m_name = "square"; break;
        case EXACTINTEGERSQRT: m_name = "exact-integer-sqrt"; break;
        case FLOORDIV:       m_name = "floor/"; break;
        case TRUNCATEDIV:    m_name = "truncate/"; break;
        case FLOORQUOTIENT:  m_name = "floor-quotient"; break;
        case FLOORREMAINDER: m_name = "floor-remainder"; break;
        case TRUNCATEQUOTIENT: m_name = "truncate-quotient"; break;
        case TRUNCATEREMAINDER: m_name = "truncate-remainder"; break;
    }
}

// ============================================================================
// Primitive Application
// ============================================================================

Object* PrimitiveNumbers::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Arity checking
    if (!check_arity(args)) {
        return arity_error("");
    }

    // Extract common arguments
    Object* x = (args && !args->is_nil()) ? first(args) : nil;
    Object* y = (args && !args->is_nil() && rest(args) && !rest(args)->is_nil()) ? second(args) : nil;

    // Dispatch based on Primitive ID
    switch (m_id) {
        // ====================================================================
        // Predicates
        // ====================================================================

        case NUMBERQ:
            return truth(x->get_type() == static_cast<int>(Type::DOUBLE) ||
                        x->get_type() == static_cast<int>(Type::INT) ||
                        x->get_type() == static_cast<int>(Type::LONG));

        case ODDQ: {
            if (x->get_type() == static_cast<int>(Type::DOUBLE)) {
                double val = ((atom<double>*)x)->value();
                return truth(std::fmod(std::abs(val), 2.0) == 1.0);
            }
            if (x->get_type() == static_cast<int>(Type::LONG))
                return truth(std::abs(((atom<long long>*)x)->value()) % 2 != 0);
            if (x->get_type() == static_cast<int>(Type::INT))
                return truth(std::abs(((atom<int>*)x)->value()) % 2 != 0);
            return nil;
        }

        case EVENQ: {
            if (x->get_type() == static_cast<int>(Type::DOUBLE)) {
                double val = ((atom<double>*)x)->value();
                return truth(std::fmod(std::abs(val), 2.0) == 0.0);
            }
            if (x->get_type() == static_cast<int>(Type::LONG))
                return truth(std::abs(((atom<long long>*)x)->value()) % 2 == 0);
            if (x->get_type() == static_cast<int>(Type::INT))
                return truth(std::abs(((atom<int>*)x)->value()) % 2 == 0);
            return nil;
        }

        case ZEROQ: {
            if (x->get_type() == static_cast<int>(Type::DOUBLE))
                return truth((abs(((atom<double>*)x)->value())) == 0);
            if (x->get_type() == static_cast<int>(Type::LONG))
                return truth((std::abs(((atom<long long>*)x)->value())) == 0);
            if (x->get_type() == static_cast<int>(Type::INT))
                return truth((abs(((atom<int>*)x)->value())) == 0);
            return nil;
        }

        case POSITIVEQ: {
            if (x->get_type() == static_cast<int>(Type::DOUBLE))
                return truth(((atom<double>*)x)->value() > 0);
            if (x->get_type() == static_cast<int>(Type::LONG))
                return truth(((atom<long long>*)x)->value() > 0);
            if (x->get_type() == static_cast<int>(Type::INT))
                return truth(((atom<int>*)x)->value() > 0);
            return nil;
        }

        case NEGATIVEQ: {
            if (x->get_type() == static_cast<int>(Type::DOUBLE))
                return truth(((atom<double>*)x)->value() < 0);
            if (x->get_type() == static_cast<int>(Type::LONG))
                return truth(((atom<long long>*)x)->value() < 0);
            if (x->get_type() == static_cast<int>(Type::INT))
                return truth(((atom<int>*)x)->value() < 0);
            return nil;
        }

        case INTEGERQ:   return truth(numeric::is_integer_value(x));
        case EXACTQ:     return truth(numeric::is_exact(x));
        case INEXACTQ:   return truth(numeric::is_inexact(x));
        case RATIONALQ:  return truth(numeric::is_rational_value(x));
        case REALQ:      return truth(numeric::is_real_value(x));
        case COMPLEXQ:   return truth(numeric::is_complex_value(x));

        // ====================================================================
        // Comparisons
        // ====================================================================

        case EQ:  return num_compare(args, '=');
        case LT:  return num_compare(args, '<');
        case GT:  return num_compare(args, '>');
        case LE:  return num_compare(args, 'L');
        case GE:  return num_compare(args, 'G');

        // ====================================================================
        // Arithmetic
        // ====================================================================

        case PLUS:   return num_compute(args, '+', ZERO);
        case MINUS:  return num_compute(rest(args), '-', x);
        case TIMES:  return num_compute(args, '*', ONE);
        case DIVIDE: return num_compute(rest(args), '/', x);
        case MAX:    return num_compute(args, 'X', x);
        case MIN:    return num_compute(args, 'N', x);

        // ====================================================================
        // Division Operations
        // ====================================================================

        case QUOTIENT: {
            if (!numeric::is_number(x) || !numeric::is_number(y)) {
                return error("quotient: expected two numbers");
            }
            return numeric::quotient(x, y);
        }

        case REMAINDER: {
            if (!numeric::is_number(x) || !numeric::is_number(y)) {
                return error("remainder: expected two numbers");
            }
            return numeric::remainder(x, y);
        }

        case MODULO: {
            if (!numeric::is_number(x) || !numeric::is_number(y)) {
                return error("modulo: expected two numbers");
            }
            return numeric::modulo(x, y);
        }

        case GCD:
            return (args == NULL || args->is_nil()) ? ZERO : gcd(args);

        case LCM:
            return (args == NULL || args->is_nil()) ? ONE : lcm(args);

        // ====================================================================
        // Rounding Functions
        // ====================================================================

        case ROUND: {
            if (!numeric::is_number(x)) {
                return error("round: expected a number");
            }
            return numeric::make_inexact(std::round(numeric::to_double(x)));
        }

        case FLOOR: {
            if (!numeric::is_number(x)) {
                return error("floor: expected a number");
            }
            return numeric::make_inexact(std::floor(numeric::to_double(x)));
        }

        case CEILING: {
            if (!numeric::is_number(x)) {
                return error("ceiling: expected a number");
            }
            return numeric::make_inexact(std::ceil(numeric::to_double(x)));
        }

        case TRUNCATE: {
            if (!numeric::is_number(x)) {
                return error("truncate: expected a number");
            }
            return numeric::make_inexact(std::trunc(numeric::to_double(x)));
        }

        // ====================================================================
        // Math Functions
        // ====================================================================

        case ABS: {
            if (!numeric::is_number(x)) {
                return error("abs: expected a number");
            }
            return numeric::abs_value(x);
        }

        case SQRT: {
            if (!numeric::is_number(x)) {
                return error("sqrt: expected a number");
            }
            return numeric::make_inexact(std::sqrt(numeric::to_double(x)));
        }

        case EXPT: {
            if (!numeric::is_number(x) || !numeric::is_number(y)) {
                return error("expt: expected two numbers");
            }
            return numeric::make_inexact(std::pow(numeric::to_double(x), numeric::to_double(y)));
        }

        // ====================================================================
        // Transcendental Functions
        // ====================================================================

        case EXP: {
            if (!numeric::is_number(x)) {
                return error("exp: expected a number");
            }
            return numeric::make_inexact(std::exp(numeric::to_double(x)));
        }

        case LOG: {
            if (!numeric::is_number(x)) {
                return error("log: expected a number");
            }
            return numeric::make_inexact(std::log(numeric::to_double(x)));
        }

        case SIN: {
            if (!numeric::is_number(x)) {
                return error("sin: expected a number");
            }
            return numeric::make_inexact(std::sin(numeric::to_double(x)));
        }

        case COS: {
            if (!numeric::is_number(x)) {
                return error("cos: expected a number");
            }
            return numeric::make_inexact(std::cos(numeric::to_double(x)));
        }

        case TAN: {
            if (!numeric::is_number(x)) {
                return error("tan: expected a number");
            }
            return numeric::make_inexact(std::tan(numeric::to_double(x)));
        }

        case ASIN: {
            if (!numeric::is_number(x)) {
                return error("asin: expected a number");
            }
            return numeric::make_inexact(std::asin(numeric::to_double(x)));
        }

        case ACOS: {
            if (!numeric::is_number(x)) {
                return error("acos: expected a number");
            }
            return numeric::make_inexact(std::acos(numeric::to_double(x)));
        }

        case ATAN: {
            if (!numeric::is_number(x)) {
                return error("atan: expected a number");
            }
            return numeric::make_inexact(std::atan(numeric::to_double(x)));
        }

        // ====================================================================
        // Numeric Tower Conversions
        // ====================================================================

        case EXACTTOINEXACT: {
            if (!numeric::is_number(x)) {
                return error("exact->inexact: expected a number");
            }
            return numeric::to_inexact(x);
        }

        case INEXACTTOEXACT: {
            if (!numeric::is_number(x)) {
                return error("inexact->exact: expected a number");
            }
            try {
                return numeric::to_exact(x);
            } catch (const std::exception& e) {
                return error(std::string("inexact->exact: ") + e.what());
            }
        }

        // ====================================================================
        // Rational Operations
        // ====================================================================

        case NUMERATOR: {
            if (!numeric::is_rational_value(x)) {
                return error("numerator: expected a rational number");
            }
            if (x->get_type() == static_cast<int>(Type::RATIONAL)) {
                a_rational* r = static_cast<a_rational*>(x);
                return numeric::make_integer(r->numerator());
            }
            // For integers, return the integer itself (denominator is 1)
            return x;
        }

        case DENOMINATOR: {
            if (!numeric::is_rational_value(x)) {
                return error("denominator: expected a rational number");
            }
            if (x->get_type() == static_cast<int>(Type::RATIONAL)) {
                a_rational* r = static_cast<a_rational*>(x);
                return numeric::make_integer(r->denominator());
            }
            // For integers, denominator is 1
            return ONE;
        }

        case RATIONALIZE: {
            if (!numeric::is_number(x) || !numeric::is_number(y)) {
                return error("rationalize: expected two numbers");
            }

            double val = numeric::to_double(x);
            double eps = numeric::to_double(y);

            // Handle special cases
            if (eps < 0) eps = -eps;
            if (eps == 0) return x;  // No tolerance, return as-is

            // Simple algorithm: find simplest fraction in range [val-eps, val+eps]
            double lower = val - eps;
            double upper = val + eps;

            // Start with bounds as fractions
            int64_t p0 = 0, q0 = 1;  // lower bound: 0/1
            int64_t p1 = 1, q1 = 0;  // upper bound: 1/0 (infinity)

            while (true) {
                // Mediant of p0/q0 and p1/q1
                int64_t p = p0 + p1;
                int64_t q = q0 + q1;

                if (q > 1000000) break;  // Prevent overflow

                double mediant = (double)p / (double)q;

                if (mediant < lower) {
                    p0 = p;
                    q0 = q;
                } else if (mediant > upper) {
                    p1 = p;
                    q1 = q;
                } else {
                    // Mediant is in range
                    return numeric::make_inexact(mediant);
                }
            }

            // Fallback
            return x;
        }

        // ====================================================================
        // Complex Operations
        // ====================================================================

        case REALPART: {
            if (!numeric::is_number(x)) {
                return error("real-part: expected a number");
            }
            return numeric::real_part(x);
        }

        case IMAGPART: {
            if (!numeric::is_number(x)) {
                return error("imag-part: expected a number");
            }
            return numeric::imag_part(x);
        }

        case MAKERECTANGULAR: {
            if (!numeric::is_number(x)) {
                return error("make-rectangular: expected a number for real part");
            }
            if (!numeric::is_number(y)) {
                return error("make-rectangular: expected a number for imaginary part");
            }
            // If imaginary part is zero, just return the real part
            if (numeric::equal(y, ZERO)) {
                return x;
            }
            return gc_new<a_complex>(x, y);
        }

        case MAKEPOLAR: {
            if (!numeric::is_number(x)) {
                return error("make-polar: expected a number for magnitude");
            }
            if (!numeric::is_number(y)) {
                return error("make-polar: expected a number for angle");
            }
            // Convert polar to rectangular
            double mag = numeric::to_double(x);
            double angle = numeric::to_double(y);
            double real = mag * std::cos(angle);
            double imag = mag * std::sin(angle);

            Object* real_part = gc_new<atom<double>>(real);
            Object* imag_part = gc_new<atom<double>>(imag);

            if (std::abs(imag) < 1e-10) {
                return real_part;
            }
            return gc_new<a_complex>(real_part, imag_part);
        }

        case MAGNITUDE: {
            if (!numeric::is_number(x)) {
                return error("magnitude: expected a number");
            }

            if (is_complex(x)) {
                a_complex* c = as_complex(x);
                double real = numeric::to_double(c->real_part());
                double imag = numeric::to_double(c->imag_part());
                double mag = std::sqrt(real * real + imag * imag);
                return gc_new<atom<double>>(mag);
            }

            // For real numbers, magnitude is absolute value
            return numeric::abs_value(x);
        }

        case ANGLE: {
            if (!numeric::is_number(x)) {
                return error("angle: expected a number");
            }

            if (is_complex(x)) {
                a_complex* c = as_complex(x);
                double real = numeric::to_double(c->real_part());
                double imag = numeric::to_double(c->imag_part());
                double angle = std::atan2(imag, real);
                return gc_new<atom<double>>(angle);
            }

            // For positive real numbers, angle is 0; for negative, angle is pi
            double val = numeric::to_double(x);
            if (val >= 0) {
                return gc_new<atom<double>>(0.0);
            } else {
                return gc_new<atom<double>>(3.14159265358979323846);  // pi
            }
        }

        // ====================================================================
        // String Conversions
        // ====================================================================

        case NUMBERTOSTRING:
            return numberToString(x, y);

        case STRINGTONUMBER:
            return stringToNumber(x, y);

        // ====================================================================
        // R7RS Additions
        // ====================================================================

        case EXACT:
            // (exact z) - convert to exact (alias for inexact->exact)
            if (!numeric::is_number(x)) {
                return error("exact: expected a number");
            }
            try {
                return numeric::to_exact(x);
            } catch (const std::exception& e) {
                return error(std::string("exact: ") + e.what());
            }

        case INEXACT:
            // (inexact z) - convert to inexact (alias for exact->inexact)
            if (!numeric::is_number(x)) {
                return error("inexact: expected a number");
            }
            return numeric::to_inexact(x);

        case SQUARE: {
            // (square z) - square the number
            if (!x || x->is_nil()) {
                return error("square: requires one argument");
            }
            if (!numeric::is_number(x)) {
                return error("square: expected a number");
            }
            // Create a list (x x) and use multiplication
            Object* args_list = cons(x, cons(x, nil));
            return num_compute(args_list, '*', ONE);
        }

        case EXACTINTEGERSQRT: {
            // (exact-integer-sqrt k) - returns two values: s and r where k = s^2 + r
            if (!x || x->is_nil()) {
                return error("exact-integer-sqrt: requires one argument");
            }
            if (!numeric::is_integer_value(x)) {
                return error("exact-integer-sqrt: argument must be an exact non-negative integer");
            }

            double val = num(x);
            if (val < 0) {
                return error("exact-integer-sqrt: argument must be non-negative");
            }

            // Compute integer square root
            int s = (int)std::floor(std::sqrt(val));
            int r = (int)val - (s * s);

            // Return as multiple values (list for now)
            return cons(numeric::make_integer(s), cons(numeric::make_integer(r), nil));
        }

        case FLOORDIV: {
            // (floor/ n d) - returns quotient of floor division
            if (!x || x->is_nil()) {
                return error("floor/: requires two arguments");
            }
            if (!y || y->is_nil()) {
                return error("floor/: requires two arguments");
            }

            double dividend = num(x);
            double divisor = num(y);

            if (divisor == 0.0) {
                return error("floor/: division by zero");
            }

            // Floor division - return integer if both inputs are integers
            if (numeric::is_integer_value(x) && numeric::is_integer_value(y)) {
                int quotient = (int)std::floor(dividend / divisor);
                return numeric::make_integer(quotient);
            } else {
                double quotient = std::floor(dividend / divisor);
                return numeric::make_inexact(quotient);
            }
        }

        case TRUNCATEDIV: {
            // (truncate/ n d) - returns quotient of truncate division
            if (!x || x->is_nil()) {
                return error("truncate/: requires two arguments");
            }
            if (!y || y->is_nil()) {
                return error("truncate/: requires two arguments");
            }

            double dividend = num(x);
            double divisor = num(y);

            if (divisor == 0.0) {
                return error("truncate/: division by zero");
            }

            // Truncate division - return integer if both inputs are integers
            if (numeric::is_integer_value(x) && numeric::is_integer_value(y)) {
                int quotient = (int)std::trunc(dividend / divisor);
                return numeric::make_integer(quotient);
            } else {
                double quotient = std::trunc(dividend / divisor);
                return numeric::make_inexact(quotient);
            }
        }

        case FLOORQUOTIENT: {
            // (floor-quotient n d) - returns quotient of floor division
            if (!x || x->is_nil()) {
                return error("floor-quotient: requires two arguments");
            }
            if (!y || y->is_nil()) {
                return error("floor-quotient: requires two arguments");
            }

            double dividend = num(x);
            double divisor = num(y);

            if (divisor == 0.0) {
                return error("floor-quotient: division by zero");
            }

            // Floor quotient
            if (numeric::is_integer_value(x) && numeric::is_integer_value(y)) {
                int quotient = (int)std::floor(dividend / divisor);
                return numeric::make_integer(quotient);
            } else {
                double quotient = std::floor(dividend / divisor);
                return numeric::make_inexact(quotient);
            }
        }

        case FLOORREMAINDER: {
            // (floor-remainder n d) - returns remainder of floor division
            // r = n - d * floor(n/d)
            if (!x || x->is_nil()) {
                return error("floor-remainder: requires two arguments");
            }
            if (!y || y->is_nil()) {
                return error("floor-remainder: requires two arguments");
            }

            double dividend = num(x);
            double divisor = num(y);

            if (divisor == 0.0) {
                return error("floor-remainder: division by zero");
            }

            // Compute remainder: n - d * floor(n/d)
            double quotient = std::floor(dividend / divisor);
            double remainder = dividend - (divisor * quotient);

            if (numeric::is_integer_value(x) && numeric::is_integer_value(y)) {
                return numeric::make_integer((int)remainder);
            } else {
                return numeric::make_inexact(remainder);
            }
        }

        case TRUNCATEQUOTIENT: {
            // (truncate-quotient n d) - returns quotient of truncate division
            if (!x || x->is_nil()) {
                return error("truncate-quotient: requires two arguments");
            }
            if (!y || y->is_nil()) {
                return error("truncate-quotient: requires two arguments");
            }

            double dividend = num(x);
            double divisor = num(y);

            if (divisor == 0.0) {
                return error("truncate-quotient: division by zero");
            }

            // Truncate quotient
            if (numeric::is_integer_value(x) && numeric::is_integer_value(y)) {
                int quotient = (int)std::trunc(dividend / divisor);
                return numeric::make_integer(quotient);
            } else {
                double quotient = std::trunc(dividend / divisor);
                return numeric::make_inexact(quotient);
            }
        }

        case TRUNCATEREMAINDER: {
            // (truncate-remainder n d) - returns remainder of truncate division
            // r = n - d * trunc(n/d)
            if (!x || x->is_nil()) {
                return error("truncate-remainder: requires two arguments");
            }
            if (!y || y->is_nil()) {
                return error("truncate-remainder: requires two arguments");
            }

            double dividend = num(x);
            double divisor = num(y);

            if (divisor == 0.0) {
                return error("truncate-remainder: division by zero");
            }

            // Compute remainder: n - d * trunc(n/d)
            double quotient = std::trunc(dividend / divisor);
            double remainder = dividend - (divisor * quotient);

            if (numeric::is_integer_value(x) && numeric::is_integer_value(y)) {
                return numeric::make_integer((int)remainder);
            } else {
                return numeric::make_inexact(remainder);
            }
        }

        default:
            return error("Unknown numeric Primitive");
    }
}

// ============================================================================
// Registration
// ============================================================================

void PrimitiveNumbers::install(Environment* env) {
    const int n = 999999;  // Unlimited arguments marker

    // GC-safe define helper: protects name string before allocating primitive
    #define DEF(name, ...) do { \
        gc_root<a_string> _n(gc_new<a_string>(name)); \
        env->define(_n.get(), gc_new<PrimitiveNumbers>(__VA_ARGS__)); \
    } while(0)

    // Predicates
    DEF("number?",   NUMBERQ, 1, 1);
    DEF("odd?",      ODDQ, 1, 1);
    DEF("even?",     EVENQ, 1, 1);
    DEF("zero?",     ZEROQ, 1, 1);
    DEF("positive?", POSITIVEQ, 1, 1);
    DEF("negative?", NEGATIVEQ, 1, 1);
    DEF("integer?",  INTEGERQ, 1, 1);
    DEF("exact?",    EXACTQ, 1, 1);
    DEF("inexact?",  INEXACTQ, 1, 1);
    DEF("rational?", RATIONALQ, 1, 1);
    DEF("real?",     REALQ, 1, 1);
    DEF("complex?",  COMPLEXQ, 1, 1);

    // Comparisons
    DEF("=",  EQ, 2, n);
    DEF("<",  LT, 2, n);
    DEF(">",  GT, 2, n);
    DEF("<=", LE, 2, n);
    DEF(">=", GE, 2, n);

    // Arithmetic
    DEF("+",   PLUS, 0, n);
    DEF("-",   MINUS, 1, n);
    DEF("*",   TIMES, 0, n);
    DEF("/",   DIVIDE, 1, n);
    DEF("max", MAX, 1, n);
    DEF("min", MIN, 1, n);

    // Division
    DEF("quotient",  QUOTIENT, 2, 2);
    DEF("remainder", REMAINDER, 2, 2);
    DEF("modulo",    MODULO, 2, 2);
    DEF("gcd",       GCD, 0, n);
    DEF("lcm",       LCM, 0, n);

    // Rounding
    DEF("round",    ROUND, 1, 1);
    DEF("floor",    FLOOR, 1, 1);
    DEF("ceiling",  CEILING, 1, 1);
    DEF("truncate", TRUNCATE, 1, 1);

    // Math
    DEF("abs",  ABS, 1, 1);
    DEF("sqrt", SQRT, 1, 1);
    DEF("expt", EXPT, 2, 2);

    // Transcendental
    DEF("exp",  EXP, 1, 1);
    DEF("log",  LOG, 1, 1);
    DEF("sin",  SIN, 1, 1);
    DEF("cos",  COS, 1, 1);
    DEF("tan",  TAN, 1, 1);
    DEF("asin", ASIN, 1, 1);
    DEF("acos", ACOS, 1, 1);
    DEF("atan", ATAN, 1, 1);

    // Conversions
    DEF("exact->inexact", EXACTTOINEXACT, 1, 1);
    DEF("inexact->exact", INEXACTTOEXACT, 1, 1);

    // Rational
    DEF("numerator",   NUMERATOR, 1, 1);
    DEF("denominator", DENOMINATOR, 1, 1);
    DEF("rationalize", RATIONALIZE, 2, 2);

    // Complex
    DEF("real-part",        REALPART, 1, 1);
    DEF("imag-part",        IMAGPART, 1, 1);
    DEF("make-rectangular", MAKERECTANGULAR, 2, 2);
    DEF("make-polar",       MAKEPOLAR, 2, 2);
    DEF("magnitude",        MAGNITUDE, 1, 1);
    DEF("angle",            ANGLE, 1, 1);

    // String conversions
    DEF("number->string", NUMBERTOSTRING, 1, 2);
    DEF("string->number", STRINGTONUMBER, 1, 2);

    // R7RS additions
    DEF("exact",              EXACT, 1, 1);
    DEF("inexact",            INEXACT, 1, 1);
    DEF("square",             SQUARE, 1, 1);
    DEF("exact-integer-sqrt", EXACTINTEGERSQRT, 1, 1);
    DEF("floor/",             FLOORDIV, 2, 2);
    DEF("truncate/",          TRUNCATEDIV, 2, 2);
    DEF("floor-quotient",     FLOORQUOTIENT, 2, 2);
    DEF("floor-remainder",    FLOORREMAINDER, 2, 2);
    DEF("truncate-quotient",  TRUNCATEQUOTIENT, 2, 2);
    DEF("truncate-remainder", TRUNCATEREMAINDER, 2, 2);

    #undef DEF
}
