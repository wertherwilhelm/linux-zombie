#pragma once

#include "../primitive_base.h"

/**
 * @file primitive_numbers.h
 * @brief Numeric primitives (R5RS Section 6.5 + Numeric Tower)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 *
 * Implements all numeric operations including:
 * - Predicates: number?, odd?, even?, zero?, positive?, negative?, integer?, exact?, inexact?, rational?, real?, complex?
 * - Comparisons: =, <, >, <=, >=
 * - Arithmetic: +, -, *, /, max, min
 * - Division: quotient, remainder, modulo
 * - Math: round, floor, ceiling, truncate, abs, sqrt, expt
 * - Transcendental: exp, log, sin, cos, tan, asin, acos, atan
 * - Numeric tower: exact->inexact, inexact->exact
 * - Rational: numerator, denominator, rationalize, gcd, lcm
 * - Complex: make-rectangular, make-polar, real-part, imag-part, magnitude, angle
 * - Conversions: number->string, string->number
 */

/**
 * @brief Numeric primitives (R5RS Section 6.5)
 *
 * Handles all numeric operations.
 *
 * **Example Usage:**
 * @code
 * (+ 1 2 3)             ; => 6
 * (- 10 3)              ; => 7
 * (* 2 3 4)             ; => 24
 * (/ 15 3)              ; => 5
 * (sqrt 16)             ; => 4.0
 * (expt 2 10)           ; => 1024.0
 * (sin 0)               ; => 0.0
 * (gcd 12 18)           ; => 6
 * (make-rectangular 3 4); => 3+4i
 * @endcode
 */
class PrimitiveNumbers : public PrimitiveBase {
public:
    /**
     * @brief Primitive IDs for numeric operations
     */
    enum ID {
        // Predicates
        NUMBERQ = 0,      ///< number? predicate
        ODDQ = 1,         ///< odd? predicate
        EVENQ = 2,        ///< even? predicate
        ZEROQ = 3,        ///< zero? predicate
        POSITIVEQ = 4,    ///< positive? predicate
        NEGATIVEQ = 5,    ///< negative? predicate
        INTEGERQ = 6,     ///< integer? predicate
        EXACTQ = 7,       ///< exact? predicate
        INEXACTQ = 8,     ///< inexact? predicate
        RATIONALQ = 9,    ///< rational? predicate
        REALQ = 10,       ///< real? predicate
        COMPLEXQ = 11,    ///< complex? predicate

        // Comparisons
        EQ = 12,          ///< = (numeric equality)
        LT = 13,          ///< < (less than)
        GT = 14,          ///< > (greater than)
        LE = 15,          ///< <= (less than or equal)
        GE = 16,          ///< >= (greater than or equal)

        // Arithmetic
        PLUS = 17,        ///< + (addition)
        MINUS = 18,       ///< - (subtraction)
        TIMES = 19,       ///< * (multiplication)
        DIVIDE = 20,      ///< / (division)
        MAX = 21,         ///< max
        MIN = 22,         ///< min

        // Division operations
        QUOTIENT = 23,    ///< quotient
        REMAINDER = 24,   ///< remainder
        MODULO = 25,      ///< modulo
        GCD = 26,         ///< gcd (greatest common divisor)
        LCM = 27,         ///< lcm (least common multiple)

        // Rounding
        ROUND = 28,       ///< round
        FLOOR = 29,       ///< floor
        CEILING = 30,     ///< ceiling
        TRUNCATE = 31,    ///< truncate

        // Math functions
        ABS = 32,         ///< abs (absolute value)
        SQRT = 33,        ///< sqrt (square root)
        EXPT = 34,        ///< expt (exponentiation)

        // Transcendental functions
        EXP = 35,         ///< exp (e^x)
        LOG = 36,         ///< log (natural logarithm)
        SIN = 37,         ///< sin
        COS = 38,         ///< cos
        TAN = 39,         ///< tan
        ASIN = 40,        ///< asin
        ACOS = 41,        ///< acos
        ATAN = 42,        ///< atan

        // Numeric tower conversions
        EXACTTOINEXACT = 43,   ///< exact->inexact
        INEXACTTOEXACT = 44,   ///< inexact->exact

        // Rational operations
        NUMERATOR = 45,        ///< numerator
        DENOMINATOR = 46,      ///< denominator
        RATIONALIZE = 47,      ///< rationalize

        // Complex operations
        REALPART = 48,         ///< real-part
        IMAGPART = 49,         ///< imag-part
        MAKERECTANGULAR = 50,  ///< make-rectangular
        MAKEPOLAR = 51,        ///< make-polar
        MAGNITUDE = 52,        ///< magnitude
        ANGLE = 53,            ///< angle

        // Conversions
        NUMBERTOSTRING = 54,   ///< number->string
        STRINGTONUMBER = 55,   ///< string->number

        // R7RS additions
        EXACT = 56,            ///< exact (alias for inexact->exact)
        INEXACT = 57,          ///< inexact (alias for exact->inexact)
        SQUARE = 58,           ///< square
        EXACTINTEGERSQRT = 59, ///< exact-integer-sqrt
        FLOORDIV = 60,         ///< floor/ (floor division quotient)
        TRUNCATEDIV = 61,      ///< truncate/ (truncate division quotient)
        FLOORQUOTIENT = 62,    ///< floor-quotient
        FLOORREMAINDER = 63,   ///< floor-remainder
        TRUNCATEQUOTIENT = 64, ///< truncate-quotient
        TRUNCATEREMAINDER = 65 ///< truncate-remainder
    };

    /**
     * @brief Construct numeric Primitive
     * @param id Which numeric Primitive
     * @param minArgs Minimum arguments
     * @param maxArgs Maximum arguments
     */
    PrimitiveNumbers(ID id, int minArgs, int maxArgs);

    /**
     * @brief Apply numeric Primitive to arguments
     * @param eval Evaluator
     * @param args Argument list
     * @param env Environment
     * @return Result of numeric operation
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all numeric primitives into environment
     * @param env Environment to install primitives into
     */
    static void install(Environment* env);

private:
    ID m_id;  ///< Which numeric Primitive this instance represents
};
