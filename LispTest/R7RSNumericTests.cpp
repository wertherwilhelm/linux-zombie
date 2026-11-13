/**
 * @file R7RSNumericTests.cpp
 * @brief R7RS additional numeric Primitive tests
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"

class R7RSNumericTest : public ZombieSchemeTest {};

// ============================================================================
// exact and inexact Tests
// ============================================================================

TEST_F(R7RSNumericTest, ExactBasic) {
    eval("(define x (exact 3.0))");
    expectInt("x", 3);
}

TEST_F(R7RSNumericTest, ExactInteger) {
    eval("(define x (exact 5))");
    expectInt("x", 5);
}

TEST_F(R7RSNumericTest, InexactBasic) {
    eval("(define x (inexact 5))");
    expectDouble("x", 5.0);
}

TEST_F(R7RSNumericTest, InexactDouble) {
    eval("(define x (inexact 3.14))");
    expectDouble("x", 3.14);
}

TEST_F(R7RSNumericTest, ExactInexactRoundTrip) {
    eval("(define x 42)");
    eval("(define y (inexact x))");
    eval("(define z (exact y))");
    expectInt("z", 42);
}

// ============================================================================
// square Tests
// ============================================================================

TEST_F(R7RSNumericTest, SquarePositive) {
    eval("(define x (square 5))");
    expectInt("x", 25);
}

TEST_F(R7RSNumericTest, SquareNegative) {
    eval("(define x (square -3))");
    expectInt("x", 9);
}

TEST_F(R7RSNumericTest, SquareZero) {
    eval("(define x (square 0))");
    expectInt("x", 0);
}

TEST_F(R7RSNumericTest, SquareDouble) {
    eval("(define x (square 2.5))");
    expectDouble("x", 6.25);
}

TEST_F(R7RSNumericTest, SquareLarge) {
    eval("(define x (square 100))");
    expectInt("x", 10000);
}

TEST_F(R7RSNumericTest, SquareFraction) {
    eval("(define x (square 0.5))");
    expectDouble("x", 0.25);
}

// ============================================================================
// exact-integer-sqrt Tests
// ============================================================================

TEST_F(R7RSNumericTest, ExactIntegerSqrtPerfectSquare) {
    eval("(define result (exact-integer-sqrt 16))");
    expectInt("(car result)", 4);
    expectInt("(cadr result)", 0);
}

TEST_F(R7RSNumericTest, ExactIntegerSqrtNonPerfect) {
    eval("(define result (exact-integer-sqrt 10))");
    expectInt("(car result)", 3);  // sqrt(10) = 3.16... => 3
    expectInt("(cadr result)", 1);  // 10 - 3^2 = 1
}

TEST_F(R7RSNumericTest, ExactIntegerSqrtZero) {
    eval("(define result (exact-integer-sqrt 0))");
    expectInt("(car result)", 0);
    expectInt("(cadr result)", 0);
}

TEST_F(R7RSNumericTest, ExactIntegerSqrtOne) {
    eval("(define result (exact-integer-sqrt 1))");
    expectInt("(car result)", 1);
    expectInt("(cadr result)", 0);
}

TEST_F(R7RSNumericTest, ExactIntegerSqrtLarge) {
    eval("(define result (exact-integer-sqrt 100))");
    expectInt("(car result)", 10);
    expectInt("(cadr result)", 0);
}

TEST_F(R7RSNumericTest, ExactIntegerSqrtVerifyFormula) {
    // Verify k = s^2 + r
    eval("(define k 50)");
    eval("(define result (exact-integer-sqrt k))");
    eval("(define s (car result))");
    eval("(define r (cadr result))");
    eval("(define reconstructed (+ (* s s) r))");
    expectInt("reconstructed", 50);
}

TEST_F(R7RSNumericTest, ExactIntegerSqrtRemainderBound) {
    // Verify r < 2*s + 1 (remainder is bounded)
    eval("(define result (exact-integer-sqrt 50))");
    eval("(define s (car result))");
    eval("(define r (cadr result))");
    eval("(define bound (+ (* 2 s) 1))");
    expectBool("(< r bound)", true);
}

// ============================================================================
// floor/ Tests
// ============================================================================

TEST_F(R7RSNumericTest, FloorDivPositivePositive) {
    eval("(define q (floor/ 7 3))");
    expectInt("q", 2);
}

TEST_F(R7RSNumericTest, FloorDivPositiveNegative) {
    eval("(define q (floor/ 7 -3))");
    expectInt("q", -3);  // floor(7/-3) = floor(-2.33) = -3
}

TEST_F(R7RSNumericTest, FloorDivNegativePositive) {
    eval("(define q (floor/ -7 3))");
    expectInt("q", -3);  // floor(-7/3) = floor(-2.33) = -3
}

TEST_F(R7RSNumericTest, FloorDivNegativeNegative) {
    eval("(define q (floor/ -7 -3))");
    expectInt("q", 2);  // floor(-7/-3) = floor(2.33) = 2
}

TEST_F(R7RSNumericTest, FloorDivExactDivision) {
    eval("(define q (floor/ 10 5))");
    expectInt("q", 2);
}

TEST_F(R7RSNumericTest, FloorDivOne) {
    eval("(define q (floor/ 7 1))");
    expectInt("q", 7);
}

TEST_F(R7RSNumericTest, FloorDivSelf) {
    eval("(define q (floor/ 5 5))");
    expectInt("q", 1);
}

// ============================================================================
// truncate/ Tests
// ============================================================================

TEST_F(R7RSNumericTest, TruncateDivPositivePositive) {
    eval("(define q (truncate/ 7 3))");
    expectInt("q", 2);
}

TEST_F(R7RSNumericTest, TruncateDivPositiveNegative) {
    eval("(define q (truncate/ 7 -3))");
    expectInt("q", -2);  // trunc(7/-3) = trunc(-2.33) = -2
}

TEST_F(R7RSNumericTest, TruncateDivNegativePositive) {
    eval("(define q (truncate/ -7 3))");
    expectInt("q", -2);  // trunc(-7/3) = trunc(-2.33) = -2
}

TEST_F(R7RSNumericTest, TruncateDivNegativeNegative) {
    eval("(define q (truncate/ -7 -3))");
    expectInt("q", 2);  // trunc(-7/-3) = trunc(2.33) = 2
}

TEST_F(R7RSNumericTest, TruncateDivExactDivision) {
    eval("(define q (truncate/ 10 5))");
    expectInt("q", 2);
}

TEST_F(R7RSNumericTest, TruncateDivOne) {
    eval("(define q (truncate/ 7 1))");
    expectInt("q", 7);
}

TEST_F(R7RSNumericTest, TruncateDivSelf) {
    eval("(define q (truncate/ 5 5))");
    expectInt("q", 1);
}

// ============================================================================
// Comparison: floor/ vs truncate/
// ============================================================================

TEST_F(R7RSNumericTest, FloorVsTruncatePositive) {
    eval("(define f (floor/ 7 3))");
    eval("(define t (truncate/ 7 3))");
    expectInt("f", 2);
    expectInt("t", 2);  // Same for positive/positive
}

TEST_F(R7RSNumericTest, FloorVsTruncateNegative) {
    eval("(define f (floor/ -7 3))");
    eval("(define t (truncate/ -7 3))");
    expectInt("f", -3);  // floor rounds toward negative infinity
    expectInt("t", -2);  // truncate rounds toward zero
}

TEST_F(R7RSNumericTest, FloorVsTruncateMixed) {
    eval("(define f (floor/ 7 -3))");
    eval("(define t (truncate/ 7 -3))");
    expectInt("f", -3);  // floor rounds toward negative infinity
    expectInt("t", -2);  // truncate rounds toward zero
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(R7RSNumericTest, SquareAndSqrt) {
    eval("(define x 7)");
    eval("(define squared (square x))");
    eval("(define root (exact-integer-sqrt squared))");
    expectInt("(car root)", 7);
    expectInt("(cadr root)", 0);
}

TEST_F(R7RSNumericTest, ExactInexactArithmetic) {
    eval("(define a (exact 6.0))");
    eval("(define b (inexact 10))");
    eval("(define c (+ a b))");
    expectDouble("c", 16.0);
}

TEST_F(R7RSNumericTest, SquareOfInexact) {
    eval("(define x (inexact 3))");
    eval("(define y (square x))");
    expectDouble("y", 9.0);
}

TEST_F(R7RSNumericTest, ChainedConversions) {
    eval("(define a 42)");
    eval("(define b (inexact a))");
    eval("(define c (exact b))");
    eval("(define d (inexact c))");
    eval("(define e (exact d))");
    expectInt("e", 42);
}

// ============================================================================
// Practical Examples
// ============================================================================

TEST_F(R7RSNumericTest, PythagoreanTriple) {
    eval(R"(
        (define (is-pythagorean? a b c)
            (= (+ (square a) (square b)) (square c)))
        (define test1 (is-pythagorean? 3 4 5))
        (define test2 (is-pythagorean? 5 12 13))
        (define test3 (is-pythagorean? 1 2 3))
    )");
    expectBool("test1", true);
    expectBool("test2", true);
    expectBool("test3", false);
}

TEST_F(R7RSNumericTest, DistanceFormula) {
    eval(R"(
        (define (distance x1 y1 x2 y2)
            (define dx (- x2 x1))
            (define dy (- y2 y1))
            (sqrt (+ (square dx) (square dy))))
        (define d (distance 0 0 3 4))
    )");
    expectDouble("d", 5.0);
}

TEST_F(R7RSNumericTest, IntegerDivision) {
    eval(R"(
        (define (div-mod n d)
            (define q (floor/ n d))
            (define r (- n (* q d)))
            (cons q r))
        (define result (div-mod 17 5))
    )");
    expectInt("(car result)", 3);
    expectInt("(cdr result)", 2);
}

TEST_F(R7RSNumericTest, IsPerfectSquare) {
    eval(R"(
        (define (is-perfect-square? n)
            (define result (exact-integer-sqrt n))
            (define r (cadr result))
            (= r 0))
        (define test1 (is-perfect-square? 16))
        (define test2 (is-perfect-square? 17))
        (define test3 (is-perfect-square? 100))
    )");
    expectBool("test1", true);
    expectBool("test2", false);
    expectBool("test3", true);
}

TEST_F(R7RSNumericTest, Hypotenuse) {
    eval(R"(
        (define (hypotenuse a b)
            (sqrt (+ (square a) (square b))))
        (define h (hypotenuse 3 4))
    )");
    expectDouble("h", 5.0);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(R7RSNumericTest, SquareOfOne) {
    eval("(define x (square 1))");
    expectInt("x", 1);
}

TEST_F(R7RSNumericTest, FloorDivByLarger) {
    eval("(define q (floor/ 3 7))");
    expectInt("q", 0);
}

TEST_F(R7RSNumericTest, TruncateDivByLarger) {
    eval("(define q (truncate/ 3 7))");
    expectInt("q", 0);
}

TEST_F(R7RSNumericTest, SquareNearZero) {
    eval("(define x (square 0.1))");
    expectDouble("x", 0.01);
}

// ============================================================================
// floor-quotient and floor-remainder Tests
// ============================================================================

TEST_F(R7RSNumericTest, FloorQuotientPositive) {
    eval("(define q (floor-quotient 17 5))");
    expectInt("q", 3);
}

TEST_F(R7RSNumericTest, FloorRemainderPositive) {
    eval("(define r (floor-remainder 17 5))");
    expectInt("r", 2);
}

TEST_F(R7RSNumericTest, FloorQuotientNegative) {
    eval("(define q (floor-quotient -17 5))");
    expectInt("q", -4);  // floor(-17/5) = floor(-3.4) = -4
}

TEST_F(R7RSNumericTest, FloorRemainderNegative) {
    eval("(define r (floor-remainder -17 5))");
    expectInt("r", 3);  // -17 - 5*(-4) = -17 + 20 = 3
}

TEST_F(R7RSNumericTest, FloorQuotientRemainderRelation) {
    // Verify: n = d*q + r
    eval("(define n 17)");
    eval("(define d 5)");
    eval("(define q (floor-quotient n d))");
    eval("(define r (floor-remainder n d))");
    eval("(define reconstructed (+ (* d q) r))");
    expectInt("reconstructed", 17);
}

TEST_F(R7RSNumericTest, FloorRemainderSign) {
    // floor-remainder always has same sign as divisor
    eval("(define r1 (floor-remainder 17 5))");
    eval("(define r2 (floor-remainder -17 5))");
    eval("(define r3 (floor-remainder 17 -5))");
    eval("(define r4 (floor-remainder -17 -5))");
    expectBool("(>= r1 0)", true);  // divisor positive
    expectBool("(>= r2 0)", true);  // divisor positive
    expectBool("(<= r3 0)", true);  // divisor negative
    expectBool("(<= r4 0)", true);  // divisor negative
}

// ============================================================================
// truncate-quotient and truncate-remainder Tests
// ============================================================================

TEST_F(R7RSNumericTest, TruncateQuotientPositive) {
    eval("(define q (truncate-quotient 17 5))");
    expectInt("q", 3);
}

TEST_F(R7RSNumericTest, TruncateRemainderPositive) {
    eval("(define r (truncate-remainder 17 5))");
    expectInt("r", 2);
}

TEST_F(R7RSNumericTest, TruncateQuotientNegative) {
    eval("(define q (truncate-quotient -17 5))");
    expectInt("q", -3);  // trunc(-17/5) = trunc(-3.4) = -3
}

TEST_F(R7RSNumericTest, TruncateRemainderNegative) {
    eval("(define r (truncate-remainder -17 5))");
    expectInt("r", -2);  // -17 - 5*(-3) = -17 + 15 = -2
}

TEST_F(R7RSNumericTest, TruncateQuotientRemainderRelation) {
    // Verify: n = d*q + r
    eval("(define n -17)");
    eval("(define d 5)");
    eval("(define q (truncate-quotient n d))");
    eval("(define r (truncate-remainder n d))");
    eval("(define reconstructed (+ (* d q) r))");
    expectInt("reconstructed", -17);
}

TEST_F(R7RSNumericTest, TruncateRemainderSign) {
    // truncate-remainder always has same sign as dividend
    eval("(define r1 (truncate-remainder 17 5))");
    eval("(define r2 (truncate-remainder -17 5))");
    eval("(define r3 (truncate-remainder 17 -5))");
    eval("(define r4 (truncate-remainder -17 -5))");
    expectBool("(>= r1 0)", true);  // dividend positive
    expectBool("(<= r2 0)", true);  // dividend negative
    expectBool("(>= r3 0)", true);  // dividend positive
    expectBool("(<= r4 0)", true);  // dividend negative
}

// ============================================================================
// Comparison: Different Division Methods
// ============================================================================

TEST_F(R7RSNumericTest, CompareFloorVsTruncateQuotient) {
    eval("(define fq (floor-quotient -17 5))");
    eval("(define tq (truncate-quotient -17 5))");
    expectInt("fq", -4);
    expectInt("tq", -3);
}

TEST_F(R7RSNumericTest, CompareFloorVsTruncateRemainder) {
    eval("(define fr (floor-remainder -17 5))");
    eval("(define tr (truncate-remainder -17 5))");
    expectInt("fr", 3);
    expectInt("tr", -2);
}

TEST_F(R7RSNumericTest, DivisionConsistency) {
    // Compare floor/, floor-quotient
    eval("(define div1 (floor/ 17 5))");
    eval("(define div2 (floor-quotient 17 5))");
    expectInt("div1", 3);
    expectInt("div2", 3);
}

TEST_F(R7RSNumericTest, TruncateDivisionConsistency) {
    // Compare truncate/, truncate-quotient
    eval("(define div1 (truncate/ 17 5))");
    eval("(define div2 (truncate-quotient 17 5))");
    expectInt("div1", 3);
    expectInt("div2", 3);
}

// ============================================================================
// Practical Examples with Quotient/Remainder
// ============================================================================

TEST_F(R7RSNumericTest, ModuloArithmeticWithFloor) {
    eval(R"(
        (define (mod-add a b m)
            (floor-remainder (+ a b) m))
        (define result (mod-add 7 8 10))
    )");
    expectInt("result", 5);  // (7+8) mod 10 = 15 mod 10 = 5
}

TEST_F(R7RSNumericTest, DivideWithBothResults) {
    eval(R"(
        (define (divmod n d)
            (cons (floor-quotient n d) (floor-remainder n d)))
        (define result (divmod 23 5))
    )");
    expectInt("(car result)", 4);
    expectInt("(cdr result)", 3);
}

TEST_F(R7RSNumericTest, ClockArithmetic) {
    eval(R"(
        (define (clock-add hours-to-add current-hour)
            (floor-remainder (+ current-hour hours-to-add) 12))
        (define time1 (clock-add 5 10))
        (define time2 (clock-add 8 7))
    )");
    expectInt("time1", 3);   // 10 + 5 = 15, 15 mod 12 = 3
    expectInt("time2", 3);   // 7 + 8 = 15, 15 mod 12 = 3
}
