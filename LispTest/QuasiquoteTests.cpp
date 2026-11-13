#include "pch.h"
#include "TestFixture.h"

// ============================================================================
// Quasiquote Tests
// ============================================================================

TEST_F(ZombieSchemeTest, QuasiquoteAtom) {
    // Quasiquote of atom is like quote
    Object* result = eval("`5");
    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 5);
}

TEST_F(ZombieSchemeTest, QuasiquoteSymbol) {
    // Quasiquote of symbol
    Object* result = eval("`hello");
    ASSERT_NE(result, nullptr);
    a_symbol* sym = dynamic_cast<a_symbol*>(result);
    ASSERT_NE(sym, nullptr);
    EXPECT_EQ(sym->getName(), "hello");
}

TEST_F(ZombieSchemeTest, QuasiquoteSimpleList) {
    // Quasiquote of simple list
    eval("(define result (quasiquote (1 2 3)))");
    Object* result = eval("result");

    ASSERT_TRUE(result->pair());
    EXPECT_EQ(numeric::to_int64(car(result)), 1);
    EXPECT_EQ(numeric::to_int64(car(cdr(result))), 2);
    EXPECT_EQ(numeric::to_int64(car(cdr(cdr(result)))), 3);
}

TEST_F(ZombieSchemeTest, QuasiquoteWithUnquote) {
    // Quasiquote with unquote
    eval("(define x 5)");
    eval("(define result (quasiquote (1 (unquote x) 3)))");
    Object* result = eval("result");

    ASSERT_TRUE(result->pair());
    EXPECT_EQ(numeric::to_int64(car(result)), 1);
    EXPECT_EQ(numeric::to_int64(car(cdr(result))), 5);  // x evaluated to 5
    EXPECT_EQ(numeric::to_int64(car(cdr(cdr(result)))), 3);
}

TEST_F(ZombieSchemeTest, QuasiquoteWithExpression) {
    // Quasiquote with computed expression
    eval("(define result (quasiquote (1 (unquote (+ 2 3)) 4)))");
    Object* result = eval("result");

    ASSERT_TRUE(result->pair());
    EXPECT_EQ(numeric::to_int64(car(result)), 1);
    EXPECT_EQ(numeric::to_int64(car(cdr(result))), 5);  // (+ 2 3) = 5
    EXPECT_EQ(numeric::to_int64(car(cdr(cdr(result)))), 4);
}

TEST_F(ZombieSchemeTest, QuasiquoteNested) {
    // Nested quasiquote
    eval("(define x 10)");
    eval("(define result (quasiquote (a (b (unquote x) c))))");
    Object* result = eval("result");

    ASSERT_TRUE(result->pair());
    Object* inner = car(cdr(result));  // (b 10 c)
    ASSERT_TRUE(inner->pair());

    a_symbol* b = dynamic_cast<a_symbol*>(car(inner));
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->getName(), "b");

    EXPECT_EQ(numeric::to_int64(car(cdr(inner))), 10);
}

TEST_F(ZombieSchemeTest, QuasiquoteWithUnquoteSplicing) {
    // Quasiquote with unquote-splicing
    eval("(define lst (quote (2 3 4)))");
    eval("(define result (quasiquote (1 (unquote-splicing lst) 5)))");
    Object* result = eval("result");

    ASSERT_TRUE(result->pair());
    EXPECT_EQ(numeric::to_int64(car(result)), 1);
    EXPECT_EQ(numeric::to_int64(car(cdr(result))), 2);
    EXPECT_EQ(numeric::to_int64(car(cdr(cdr(result)))), 3);
    EXPECT_EQ(numeric::to_int64(car(cdr(cdr(cdr(result))))), 4);
    EXPECT_EQ(numeric::to_int64(car(cdr(cdr(cdr(cdr(result)))))), 5);
}

TEST_F(ZombieSchemeTest, QuasiquoteMultipleUnquotes) {
    // Multiple unquotes in one expression
    eval("(define a 1)");
    eval("(define b 2)");
    eval("(define result (quasiquote ((unquote a) + (unquote b) = (unquote (+ a b)))))");
    Object* result = eval("result");

    ASSERT_TRUE(result->pair());
    EXPECT_EQ(numeric::to_int64(car(result)), 1);  // a
    EXPECT_EQ(numeric::to_int64(car(cdr(cdr(result)))), 2);  // b
    EXPECT_EQ(numeric::to_int64(car(cdr(cdr(cdr(cdr(result)))))), 3);  // (+ a b)
}

TEST_F(ZombieSchemeTest, QuasiquoteEmptyList) {
    // Quasiquote of empty list
    eval("(define result (quasiquote ()))");
    Object* result = eval("result");
    EXPECT_TRUE(::is_nil(result) || result->is_nil());
}
