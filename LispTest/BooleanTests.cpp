#include "pch.h"
#include "TestFixture.h"

// ============================================================================
// R5RS Boolean Tests
// ============================================================================

// ----------------------------------------------------------------------------
// Boolean Literal Tests
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, BooleanLiteralTrue) {
    // #t should parse and evaluate to true
    Object* result = eval("#t");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result, t);
}

TEST_F(ZombieSchemeTest, BooleanLiteralFalse) {
    // R5RS: #f should parse and evaluate to false_obj
    Object* result = eval("#f");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result, false_obj);
}

// ----------------------------------------------------------------------------
// Boolean Predicate Tests
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, BooleanPredicateOnTrue) {
    // (boolean? #t) should return #t
    Object* result = eval("(boolean? #t)");
    EXPECT_EQ(result, t);
}

TEST_F(ZombieSchemeTest, BooleanPredicateOnFalse) {
    // (boolean? #f) should return #t
    Object* result = eval("(boolean? #f)");
    EXPECT_EQ(result, t);
}

TEST_F(ZombieSchemeTest, BooleanPredicateOnNumber) {
    // (boolean? 5) should return #f
    Object* result = eval("(boolean? 5)");
    EXPECT_EQ(result, false_obj);
}

TEST_F(ZombieSchemeTest, BooleanPredicateOnSymbol) {
    // (boolean? 'foo) should return #f
    Object* result = eval("(boolean? 'foo)");
    EXPECT_EQ(result, false_obj);
}

// ----------------------------------------------------------------------------
// Boolean Operations Tests
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, NotOnTrue) {
    // (not #t) should return #f
    Object* result = eval("(not #t)");
    EXPECT_EQ(result, false_obj);
}

TEST_F(ZombieSchemeTest, NotOnFalse) {
    // (not #f) should return #t
    Object* result = eval("(not #f)");
    EXPECT_EQ(result, t);
}

TEST_F(ZombieSchemeTest, NotOnZero) {
    // R5RS: 0 is true! (not 0) should return #f
    Object* result = eval("(not 0)");
    EXPECT_EQ(result, false_obj);
}

TEST_F(ZombieSchemeTest, NotOnEmptyList) {
    // R5RS: '() is truthy and distinct from #f
    // (not '()) returns #f because empty list is true
    Object* result = eval("(not '())");
    EXPECT_EQ(result, false_obj);
}

// ----------------------------------------------------------------------------
// If with Boolean Tests
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, IfWithTrueLiteral) {
    expectInt("(if #t 1 2)", 1);
}

TEST_F(ZombieSchemeTest, IfWithFalseLiteral) {
    expectInt("(if #f 1 2)", 2);
}

TEST_F(ZombieSchemeTest, IfWithZeroIsTrue) {
    // In R5RS, 0 is true
    expectInt("(if 0 1 2)", 1);
}

TEST_F(ZombieSchemeTest, IfWithEmptyListIsTrue) {
    // R5RS: '() is truthy and distinct from #f
    // (if '() 1 2) returns 1 because empty list is true
    expectInt("(if '() 1 2)", 1);
}

// ----------------------------------------------------------------------------
// And/Or with Booleans
// ----------------------------------------------------------------------------

// and/or are short-circuiting special forms implemented in lisp.cpp
// (eval_list + eval_step_list). Per R5RS 4.2.1 the false value is #f (false_obj).
TEST_F(ZombieSchemeTest, AndWithBooleans) {
    EXPECT_EQ(eval("(and #t #t)"), t);
    EXPECT_EQ(eval("(and #t #f)"), false_obj);
    EXPECT_EQ(eval("(and #f #t)"), false_obj);
}

TEST_F(ZombieSchemeTest, OrWithBooleans) {
    EXPECT_EQ(eval("(or #f #f)"), false_obj);
    EXPECT_EQ(eval("(or #f #t)"), t);
    EXPECT_EQ(eval("(or #t #f)"), t);
}

TEST_F(ZombieSchemeTest, AndOrIdentities) {
    // (and) => #t, (or) => #f  (R5RS 4.2.1)
    EXPECT_EQ(eval("(and)"), t);
    EXPECT_EQ(eval("(or)"), false_obj);
}

TEST_F(ZombieSchemeTest, AndReturnsLastValue) {
    // (and e1 ... en) returns the value of en when all are true
    expectInt("(and 1 2 3)", 3);
}

TEST_F(ZombieSchemeTest, OrReturnsFirstTrueValue) {
    // (or e1 ... en) returns the first true value
    expectInt("(or #f #f 7)", 7);
}

TEST_F(ZombieSchemeTest, AndShortCircuits) {
    // The error-raising tail must never be evaluated once a false is seen.
    EXPECT_EQ(eval("(and #f (error \"should not run\"))"), false_obj);
}

TEST_F(ZombieSchemeTest, OrShortCircuits) {
    // The error-raising tail must never be evaluated once a true is seen.
    expectInt("(or 5 (error \"should not run\"))", 5);
}

// ----------------------------------------------------------------------------
// Comparison with Booleans
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, EqOnBooleans) {
    // (eq? #t #t) should return #t
    Object* result = eval("(eq? #t #t)");
    EXPECT_EQ(result, t);

    // (eq? #f #f) should return #t
    result = eval("(eq? #f #f)");
    EXPECT_EQ(result, t);

    // (eq? #t #f) should return #f
    result = eval("(eq? #t #f)");
    EXPECT_EQ(result, false_obj);
}

// ----------------------------------------------------------------------------
// Macros with Booleans
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, MacroWithBooleanLiterals) {
    eval(R"(
        (define-syntax bool-test
          (syntax-rules ()
            ((bool-test x)
             (if x #t #f))))
    )");

    Object* result = eval("(bool-test #t)");
    EXPECT_EQ(result, t);

    result = eval("(bool-test #f)");
    EXPECT_EQ(result, false_obj);
}

TEST_F(ZombieSchemeTest, RecursiveMacroWithBooleans) {
    // R5RS: We can use actual #t and #f in recursive macros!
    eval(R"(
        (define-syntax my-and
          (syntax-rules ()
            ((my-and) #t)
            ((my-and test) test)
            ((my-and test1 test2 ...)
             (if test1 (my-and test2 ...) #f))))
    )");

    Object* result = eval("(my-and #t #t #t)");
    EXPECT_EQ(result, t);

    result = eval("(my-and #t #f #t)");
    EXPECT_EQ(result, false_obj);

    result = eval("(my-and)");
    EXPECT_EQ(result, t);
}

TEST_F(ZombieSchemeTest, RecursiveMacroOr) {
    eval(R"(
        (define-syntax my-or
          (syntax-rules ()
            ((my-or) #f)
            ((my-or test) test)
            ((my-or test1 test2 ...)
             (if test1 #t (my-or test2 ...)))))
    )");

    Object* result = eval("(my-or #f #f #f)");
    EXPECT_EQ(result, false_obj);

    result = eval("(my-or #f #t #f)");
    EXPECT_EQ(result, t);

    result = eval("(my-or)");
    EXPECT_EQ(result, false_obj);
}
