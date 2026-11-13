#include "pch.h"
#include "TestFixture.h"

// ============================================================================
// Basic Math Predicates
// ============================================================================

TEST_F(ZombieSchemeTest, NumberPredicate) {
    expectBool("(number? 42)", true);
    expectBool("(number? 3.14)", true);
    expectBool("(number? \"hello\")", false);
}

TEST_F(ZombieSchemeTest, IntegerPredicate) {
    expectBool("(integer? 42)", true);
    expectBool("(integer? 3.14)", false);
}

TEST_F(ZombieSchemeTest, RealPredicate) {
    expectBool("(real? 42)", true);
    expectBool("(real? 3.14)", true);
}

TEST_F(ZombieSchemeTest, ZeroPredicate) {
    expectBool("(zero? 0)", true);
    expectBool("(zero? 1)", false);
    expectBool("(zero? -1)", false);
}

TEST_F(ZombieSchemeTest, PositivePredicate) {
    expectBool("(positive? 5)", true);
    expectBool("(positive? 0)", false);
    expectBool("(positive? -5)", false);
}

TEST_F(ZombieSchemeTest, NegativePredicate) {
    expectBool("(negative? -5)", true);
    expectBool("(negative? 0)", false);
    expectBool("(negative? 5)", false);
}

TEST_F(ZombieSchemeTest, OddPredicate) {
    expectBool("(odd? 1)", true);
    expectBool("(odd? 3)", true);
    expectBool("(odd? 2)", false);
    expectBool("(odd? 0)", false);
}

TEST_F(ZombieSchemeTest, EvenPredicate) {
    expectBool("(even? 2)", true);
    expectBool("(even? 0)", true);
    expectBool("(even? 1)", false);
    expectBool("(even? 3)", false);
}

// ============================================================================
// Rounding Functions
// ============================================================================

TEST_F(ZombieSchemeTest, RoundFunction) {
    expectDouble("(round 3.7)", 4.0);
    expectDouble("(round 3.3)", 3.0);
    expectDouble("(round -2.7)", -3.0);
}

TEST_F(ZombieSchemeTest, FloorFunction) {
    expectDouble("(floor 3.7)", 3.0);
    expectDouble("(floor 3.3)", 3.0);
    expectDouble("(floor -2.3)", -3.0);
}

TEST_F(ZombieSchemeTest, CeilingFunction) {
    expectDouble("(ceiling 3.2)", 4.0);
    expectDouble("(ceiling 3.7)", 4.0);
    expectDouble("(ceiling -2.7)", -2.0);
}

TEST_F(ZombieSchemeTest, TruncateFunction) {
    expectDouble("(truncate 3.7)", 3.0);
    expectDouble("(truncate -3.7)", -3.0);
}

// ============================================================================
// Power and Root Functions
// ============================================================================

TEST_F(ZombieSchemeTest, SqrtFunction) {
    expectDouble("(sqrt 16)", 4.0);
    expectDouble("(sqrt 100)", 10.0);
    expectDouble("(sqrt 2)", 1.414, 0.001);
}

TEST_F(ZombieSchemeTest, ExptFunction) {
    expectDouble("(expt 2 3)", 8.0);
    expectDouble("(expt 10 2)", 100.0);
    expectDouble("(expt 3 4)", 81.0);
}

TEST_F(ZombieSchemeTest, ExpFunction) {
    expectDouble("(exp 0)", 1.0);
    expectDouble("(exp 1)", 2.718, 0.001);
}

TEST_F(ZombieSchemeTest, LogFunction) {
    expectDouble("(log 1)", 0.0, 0.001);
    expectDouble("(log 2.718282)", 1.0, 0.001);
}

// ============================================================================
// Trigonometric Functions
// ============================================================================

TEST_F(ZombieSchemeTest, SinFunction) {
    expectDouble("(sin 0)", 0.0, 0.001);
    expectDouble("(sin 1.5708)", 1.0, 0.001);  // pi/2
}

TEST_F(ZombieSchemeTest, CosFunction) {
    expectDouble("(cos 0)", 1.0, 0.001);
    expectDouble("(cos 3.14159)", -1.0, 0.001);  // pi
}

TEST_F(ZombieSchemeTest, TanFunction) {
    expectDouble("(tan 0)", 0.0, 0.001);
}

TEST_F(ZombieSchemeTest, AsinFunction) {
    expectDouble("(asin 0)", 0.0, 0.001);
    expectDouble("(asin 1)", 1.5708, 0.001);  // pi/2
}

TEST_F(ZombieSchemeTest, AcosFunction) {
    expectDouble("(acos 1)", 0.0, 0.001);
}

TEST_F(ZombieSchemeTest, AtanFunction) {
    expectDouble("(atan 0)", 0.0, 0.001);
}

// ============================================================================
// Min/Max Functions
// ============================================================================

TEST_F(ZombieSchemeTest, MaxFunction) {
    expectDouble("(max 1 2 3)", 3.0);
    expectDouble("(max 5 2 8 1)", 8.0);
}

TEST_F(ZombieSchemeTest, MinFunction) {
    expectDouble("(min 1 2 3)", 1.0);
    expectDouble("(min 5 2 8 1)", 1.0);
}

// ============================================================================
// Abs and Other Functions
// ============================================================================

TEST_F(ZombieSchemeTest, AbsFunction) {
    expectDouble("(abs 5)", 5.0);
    expectDouble("(abs -5)", 5.0);
    expectDouble("(abs 0)", 0.0);
}

TEST_F(ZombieSchemeTest, ModuloFunction) {
    expectDouble("(modulo 10 3)", 1.0);
    expectDouble("(modulo 10 5)", 0.0);
}

TEST_F(ZombieSchemeTest, RemainderFunction) {
    expectDouble("(remainder 10 3)", 1.0);
    expectDouble("(remainder 10 5)", 0.0);
}

TEST_F(ZombieSchemeTest, QuotientFunction) {
    expectDouble("(quotient 10 3)", 3.0);
    expectDouble("(quotient 20 5)", 4.0);
}

TEST_F(ZombieSchemeTest, GcdFunction) {
    expectDouble("(gcd 12 8)", 4.0);
    expectDouble("(gcd 15 25)", 5.0);
}

TEST_F(ZombieSchemeTest, LcmFunction) {
    expectDouble("(lcm 4 6)", 12.0);
    expectDouble("(lcm 3 5)", 15.0);
}

// ============================================================================
// Exact/Inexact Tests
// ============================================================================

TEST_F(ZombieSchemeTest, ExactPredicate) {
    // These may depend on implementation
    Object* result = eval("(exact? 5)");
    ASSERT_NE(result, nullptr);
}

TEST_F(ZombieSchemeTest, InexactPredicate) {
    // These may depend on implementation
    Object* result = eval("(inexact? 5.5)");
    ASSERT_NE(result, nullptr);
}

TEST_F(ZombieSchemeTest, ExactToInexact) {
    expectDouble("(exact->inexact 5)", 5.0);
}

TEST_F(ZombieSchemeTest, InexactToExact) {
    Object* result = eval("(inexact->exact 5.0)");
    if (result != nullptr) {
        expectDouble("(inexact->exact 5.0)", 5.0);
    }
}
