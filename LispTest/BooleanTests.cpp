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

// NOTE: and/or are special forms that need to be implemented in Lisp.cpp
// They require short-circuit evaluation and are not primitives
// TODO: Implement and/or special forms in Lisp.cpp
/*
TEST_F(ZombieSchemeTest, AndWithBooleans) {
    // (and #t #t) returns #t
    Object* result = eval("(and #t #t)");
    EXPECT_EQ(result, t);

    // (and #t #f) returns #f
    result = eval("(and #t #f)");
    EXPECT_EQ(result, nil);

    // (and #f #t) returns #f (and is short-circuiting)
    result = eval("(and #f #t)");
    EXPECT_EQ(result, nil);
}

TEST_F(ZombieSchemeTest, OrWithBooleans) {
    // (or #f #f) returns #f
    Object* result = eval("(or #f #f)");
    EXPECT_EQ(result, nil);

    // (or #f #t) returns #t
    result = eval("(or #f #t)");
    EXPECT_EQ(result, t);

    // (or #t #f) returns #t (or is short-circuiting)
    result = eval("(or #t #f)");
    EXPECT_EQ(result, t);
}
*/

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
