#include "pch.h"
#include "TestFixture.h"
#include "a_rational.h"
#include "a_complex.h"
#include "values_object.h"

// ============================================================================
// Numeric Tower Tests - Rational Numbers
// ============================================================================

TEST_F(ZombieSchemeTest, RationalBasic) {
    // Basic rational arithmetic
    Object* result = eval("(+ 1/2 1/3)");
    // Should be 5/6
    ASSERT_TRUE(is_rational(result));
    a_rational* r = as_rational(result);
    EXPECT_EQ(r->numerator(), 5);
    EXPECT_EQ(r->denominator(), 6);
}

TEST_F(ZombieSchemeTest, RationalNormalization) {
    // Rationals should normalize automatically
    Object* result = eval("(+ 2/4 2/4)");
    // 2/4 + 2/4 = 1/2 + 1/2 = 1
    ASSERT_TRUE(is_rational(result));
    a_rational* r = as_rational(result);
    EXPECT_EQ(r->numerator(), 1);
    EXPECT_EQ(r->denominator(), 1);  // Should normalize to integer
}

TEST_F(ZombieSchemeTest, RationalSubtraction) {
    Object* result = eval("(- 3/4 1/4)");
    ASSERT_TRUE(is_rational(result));
    a_rational* r = as_rational(result);
    EXPECT_EQ(r->numerator(), 1);
    EXPECT_EQ(r->denominator(), 2);
}

TEST_F(ZombieSchemeTest, RationalMultiplication) {
    Object* result = eval("(* 2/3 3/4)");
    ASSERT_TRUE(is_rational(result));
    a_rational* r = as_rational(result);
    EXPECT_EQ(r->numerator(), 1);
    EXPECT_EQ(r->denominator(), 2);
}

TEST_F(ZombieSchemeTest, RationalDivision) {
    Object* result = eval("(/ 3/4 2/3)");
    ASSERT_TRUE(is_rational(result));
    a_rational* r = as_rational(result);
    EXPECT_EQ(r->numerator(), 9);
    EXPECT_EQ(r->denominator(), 8);
}

TEST_F(ZombieSchemeTest, RationalComparison) {
    // Test rational comparison
    EXPECT_EQ(eval("(< 1/3 1/2)"), truth(true));
    EXPECT_EQ(eval("(> 2/3 1/2)"), truth(true));
    EXPECT_EQ(eval("(= 2/4 1/2)"), truth(true));
}

TEST_F(ZombieSchemeTest, RationalToInteger) {
    // Rational that equals an integer
    Object* result = eval("(+ 1/2 1/2)");
    ASSERT_TRUE(is_rational(result));
    a_rational* r = as_rational(result);
    EXPECT_TRUE(r->is_integer());
    EXPECT_EQ(r->to_int64(), 1);
}

TEST_F(ZombieSchemeTest, RationalNegative) {
    Object* result = eval("(- 0 1/3)");
    ASSERT_TRUE(is_rational(result));
    a_rational* r = as_rational(result);
    EXPECT_EQ(r->numerator(), -1);
    EXPECT_EQ(r->denominator(), 3);
}

// ============================================================================
// Numeric Tower Tests - Complex Numbers
// ============================================================================

TEST_F(ZombieSchemeTest, ComplexMakeRectangular) {
    Object* result = eval("(make-rectangular 3 4)");
    ASSERT_TRUE(is_complex(result));
    a_complex* c = as_complex(result);
    EXPECT_EQ(numeric::to_int64(c->real_part()), 3);
    EXPECT_EQ(numeric::to_int64(c->imag_part()), 4);
}

TEST_F(ZombieSchemeTest, ComplexRealPart) {
    Object* result = eval("(real-part (make-rectangular 5 7))");
    EXPECT_EQ(numeric::to_int64(result), 5);
}

TEST_F(ZombieSchemeTest, ComplexImagPart) {
    Object* result = eval("(imag-part (make-rectangular 5 7))");
    EXPECT_EQ(numeric::to_int64(result), 7);
}

TEST_F(ZombieSchemeTest, ComplexAddition) {
    // (3+4i) + (1+2i) = 4+6i
    eval("(define c1 (make-rectangular 3 4))");
    eval("(define c2 (make-rectangular 1 2))");
    Object* result = eval("(+ c1 c2)");
    ASSERT_TRUE(is_complex(result));
    a_complex* c = as_complex(result);
    EXPECT_EQ(numeric::to_int64(c->real_part()), 4);
    EXPECT_EQ(numeric::to_int64(c->imag_part()), 6);
}

TEST_F(ZombieSchemeTest, ComplexSubtraction) {
    // (5+7i) - (2+3i) = 3+4i
    eval("(define c1 (make-rectangular 5 7))");
    eval("(define c2 (make-rectangular 2 3))");
    Object* result = eval("(- c1 c2)");
    ASSERT_TRUE(is_complex(result));
    a_complex* c = as_complex(result);
    EXPECT_EQ(numeric::to_int64(c->real_part()), 3);
    EXPECT_EQ(numeric::to_int64(c->imag_part()), 4);
}

TEST_F(ZombieSchemeTest, ComplexMagnitude) {
    // Magnitude of 3+4i should be 5
    Object* result = eval("(magnitude (make-rectangular 3 4))");
    EXPECT_TRUE(numeric::is_number(result));
    // Result should be 5.0
    EXPECT_DOUBLE_EQ(numeric::to_double(result), 5.0);
}

TEST_F(ZombieSchemeTest, ComplexZeroImaginaryPart) {
    // Complex with zero imaginary part should simplify to real
    Object* result = eval("(make-rectangular 5 0)");
    // Should just return 5, not a complex number
    EXPECT_FALSE(is_complex(result));
    EXPECT_EQ(numeric::to_int64(result), 5);
}

TEST_F(ZombieSchemeTest, ComplexWithRationals) {
    // Complex numbers with rational parts
    Object* result = eval("(make-rectangular 1/2 1/3)");
    ASSERT_TRUE(is_complex(result));
    a_complex* c = as_complex(result);

    // Real part should be 1/2
    ASSERT_TRUE(is_rational(c->real_part()));
    a_rational* r = as_rational(c->real_part());
    EXPECT_EQ(r->numerator(), 1);
    EXPECT_EQ(r->denominator(), 2);

    // Imag part should be 1/3
    ASSERT_TRUE(is_rational(c->imag_part()));
    a_rational* i = as_rational(c->imag_part());
    EXPECT_EQ(i->numerator(), 1);
    EXPECT_EQ(i->denominator(), 3);
}

// ============================================================================
// Multiple Return Values Tests
// ============================================================================

TEST_F(ZombieSchemeTest, ValuesSingle) {
    // Single value should be returned as-is
    Object* result = eval("(values 42)");
    EXPECT_EQ(numeric::to_int64(result), 42);
}

TEST_F(ZombieSchemeTest, ValuesMultiple) {
    // Multiple values should be wrapped in ValuesObject
    Object* result = eval("(values 1 2 3)");
    ASSERT_NE(result, nullptr);
    ValuesObject* vals = dynamic_cast<ValuesObject*>(result);
    ASSERT_NE(vals, nullptr);

    Object* list = vals->get_values();
    EXPECT_EQ(numeric::to_int64(car(list)), 1);
    EXPECT_EQ(numeric::to_int64(car(cdr(list))), 2);
    EXPECT_EQ(numeric::to_int64(car(cdr(cdr(list)))), 3);
}

TEST_F(ZombieSchemeTest, CallWithValuesBasic) {
    // Basic call-with-values
    Object* result = eval(
        "(call-with-values "
        "  (lambda () (values 1 2 3)) "
        "  (lambda (a b c) (+ a b c)))");
    EXPECT_EQ(numeric::to_int64(result), 6);
}

TEST_F(ZombieSchemeTest, CallWithValuesSingle) {
    // Single value
    Object* result = eval(
        "(call-with-values "
        "  (lambda () 42) "
        "  (lambda (x) (* x 2)))");
    EXPECT_EQ(numeric::to_int64(result), 84);
}

TEST_F(ZombieSchemeTest, CallWithValuesArithmetic) {
    // More complex example with computation
    Object* result = eval(
        "(call-with-values "
        "  (lambda () (values (+ 1 2) (* 3 4))) "
        "  (lambda (sum prod) (- prod sum)))");
    EXPECT_EQ(numeric::to_int64(result), 9);  // 12 - 3 = 9
}

TEST_F(ZombieSchemeTest, ValuesEmpty) {
    // (values) with no arguments
    Object* result = eval("(values)");
    // Should return nil/unspecified
    EXPECT_TRUE(::is_nil(result) || result->is_nil());
}

// ============================================================================
// Mixed Numeric Tower Tests
// ============================================================================

TEST_F(ZombieSchemeTest, MixedIntegerRational) {
    // Integer + Rational
    Object* result = eval("(+ 1 1/2)");
    ASSERT_TRUE(is_rational(result));
    a_rational* r = as_rational(result);
    EXPECT_EQ(r->numerator(), 3);
    EXPECT_EQ(r->denominator(), 2);
}

TEST_F(ZombieSchemeTest, MixedRationalDouble) {
    // Rational + Double should become double
    Object* result = eval("(+ 1/2 1.5)");
    EXPECT_TRUE(is_double(result));
    EXPECT_DOUBLE_EQ(numeric::to_double(result), 2.0);
}

TEST_F(ZombieSchemeTest, MixedIntegerComplex) {
    // Integer + Complex
    eval("(define c (make-rectangular 3 4))");
    Object* result = eval("(+ 2 c)");
    ASSERT_TRUE(is_complex(result));
    a_complex* complex_result = as_complex(result);
    EXPECT_EQ(numeric::to_int64(complex_result->real_part()), 5);
    EXPECT_EQ(numeric::to_int64(complex_result->imag_part()), 4);
}

TEST_F(ZombieSchemeTest, ExactnessPreservation) {
    // Exactness should be preserved in rational arithmetic
    Object* result = eval("(* 1/3 3)");
    ASSERT_TRUE(is_rational(result));
    a_rational* r = as_rational(result);
    EXPECT_TRUE(r->is_integer());
    EXPECT_EQ(r->to_int64(), 1);
}

// ============================================================================
// inexact->exact: proper double -> rational conversion
// ============================================================================

TEST_F(ZombieSchemeTest, InexactToExactHalf) {
    Object* result = eval("(inexact->exact 0.5)");
    ASSERT_TRUE(is_rational(result));
    a_rational* r = as_rational(result);
    EXPECT_EQ(r->numerator(), 1);
    EXPECT_EQ(r->denominator(), 2);
}

TEST_F(ZombieSchemeTest, InexactToExactMixedFraction) {
    Object* result = eval("(inexact->exact 3.25)");
    ASSERT_TRUE(is_rational(result));
    a_rational* r = as_rational(result);
    EXPECT_EQ(r->numerator(), 13);
    EXPECT_EQ(r->denominator(), 4);
}

TEST_F(ZombieSchemeTest, InexactToExactWholeNumber) {
    Object* result = eval("(inexact->exact 7.0)");
    EXPECT_EQ(numeric::to_int64(result), 7);
}

TEST_F(ZombieSchemeTest, InexactToExactNegative) {
    Object* result = eval("(inexact->exact -0.75)");
    ASSERT_TRUE(is_rational(result));
    a_rational* r = as_rational(result);
    EXPECT_EQ(r->numerator(), -3);
    EXPECT_EQ(r->denominator(), 4);
}

// ============================================================================
// abs on complex numbers (magnitude)
// ============================================================================

TEST_F(ZombieSchemeTest, AbsOfComplexIsMagnitude) {
    Object* result = eval("(abs (make-rectangular 3 4))");
    EXPECT_TRUE(is_double(result));
    EXPECT_DOUBLE_EQ(numeric::to_double(result), 5.0);
}
