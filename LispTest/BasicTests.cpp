#include "pch.h"
#include "TestFixture.h"

// ============================================================================
// Basic Arithmetic Tests
// ============================================================================

TEST_F(ZombieSchemeTest, Addition) {
    expectInt("(+ 2 2)", 4);
    expectInt("(+ 1 2 3 4 5)", 15);
    expectInt("(+ 10 20 30)", 60);
    expectDouble("(+ 2.5 3.5)", 6.0);
}

TEST_F(ZombieSchemeTest, Subtraction) {
    expectInt("(- 10 5)", 5);
    expectInt("(- 100 50 25)", 25);
    expectDouble("(- 10.5 2.5)", 8.0);
}

TEST_F(ZombieSchemeTest, Multiplication) {
    expectInt("(* 3 4)", 12);
    expectInt("(* 2 3 4)", 24);
    expectDouble("(* 2.5 4)", 10.0);
}

TEST_F(ZombieSchemeTest, Division) {
    expectDouble("(/ 10 2)", 5.0);
    expectDouble("(/ 100 4 5)", 5.0);
}

TEST_F(ZombieSchemeTest, Comparison) {
    expectBool("(= 5 5)", true);
    expectBool("(= 5 6)", false);
    expectBool("(< 3 5)", true);
    expectBool("(< 5 3)", false);
    expectBool("(> 10 5)", true);
    expectBool("(<= 5 5)", true);
    expectBool("(>= 10 10)", true);
}

// ============================================================================
// Define and Variables Tests
// ============================================================================

TEST_F(ZombieSchemeTest, DefineVariable) {
    eval("(define x 42)");
    expectInt("x", 42);
}

TEST_F(ZombieSchemeTest, DefineMultipleVariables) {
    eval("(define a 10)");
    eval("(define b 20)");
    expectInt("(+ a b)", 30);
}

TEST_F(ZombieSchemeTest, DefineFunction) {
    eval("(define (square n) (* n n))");
    expectInt("(square 5)", 25);
    expectInt("(square 10)", 100);
}

TEST_F(ZombieSchemeTest, DefineFunctionWithMultipleArgs) {
    eval("(define (add a b) (+ a b))");
    expectInt("(add 3 4)", 7);
    expectInt("(add 100 200)", 300);
}

// ============================================================================
// Lambda Tests
// ============================================================================

TEST_F(ZombieSchemeTest, SimpleLambda) {
    expectInt("((lambda (x) (* x 2)) 5)", 10);
}

TEST_F(ZombieSchemeTest, LambdaWithMultipleArgs) {
    expectInt("((lambda (x y) (+ x y)) 3 4)", 7);
}

TEST_F(ZombieSchemeTest, LambdaAssignedToVariable) {
    eval("(define double (lambda (x) (* x 2)))");
    expectInt("(double 7)", 14);
}

// ============================================================================
// Conditional Tests
// ============================================================================

TEST_F(ZombieSchemeTest, IfTrue) {
    // In this implementation, only boolean results from comparisons are truthy
    expectInt("(if (= 1 1) 10 20)", 10);
}

TEST_F(ZombieSchemeTest, IfFalse) {
    // R5RS: Only #f is false. 0 is true!
    expectInt("(if 0 10 20)", 10);
}

TEST_F(ZombieSchemeTest, IfWithComparison) {
    expectInt("(if (< 3 5) 100 200)", 100);
    expectInt("(if (> 3 5) 100 200)", 200);
}

// ============================================================================
// List Tests
// ============================================================================

TEST_F(ZombieSchemeTest, Cons) {
    Object* result = eval("(cons 1 2)");
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->is_atom());
}

TEST_F(ZombieSchemeTest, Car) {
    expectInt("(car (cons 1 2))", 1);
}

TEST_F(ZombieSchemeTest, Cdr) {
    expectInt("(cdr (cons 1 2))", 2);
}

TEST_F(ZombieSchemeTest, QuotedList) {
    Object* result = eval("'(1 2 3)");
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->is_atom());
}

TEST_F(ZombieSchemeTest, ListLength) {
    expectInt("(length '(1 2 3 4 5))", 5);
    expectInt("(length '())", 0);
}

TEST_F(ZombieSchemeTest, ListReverse) {
    // Just verify it doesn't crash - output format testing is complex
    Object* result = eval("(reverse '(1 2 3))");
    ASSERT_NE(result, nullptr);
}

// ============================================================================
// Begin Tests
// ============================================================================

TEST_F(ZombieSchemeTest, Begin) {
    expectInt("(begin (define x 10) (define y 20) (+ x y))", 30);
}

TEST_F(ZombieSchemeTest, BeginReturnsLastValue) {
    expectInt("(begin 1 2 3 4 5)", 5);
}

// ============================================================================
// Quote Tests
// ============================================================================

TEST_F(ZombieSchemeTest, QuoteSymbol) {
    Object* result = eval("'hello");
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->is_atom());
}

TEST_F(ZombieSchemeTest, QuoteList) {
    Object* result = eval("'(+ 1 2)");
    ASSERT_NE(result, nullptr);
    EXPECT_FALSE(result->is_atom());
    // The list should NOT be evaluated
}

// ============================================================================
// Recursion Tests
// ============================================================================

TEST_F(ZombieSchemeTest, SimpleRecursion) {
    eval("(define (factorial n) (if (<= n 1) 1 (* n (factorial (- n 1)))))");
    expectInt("(factorial 5)", 120);
    expectDouble("(factorial 10)", 3628800.0);
}

TEST_F(ZombieSchemeTest, TailRecursion) {
    eval("(define (sum n acc) (if (<= n 0) acc (sum (- n 1) (+ acc n))))");
    expectInt("(sum 100 0)", 5050);
}
