/**
 * @file CaseLambdaTests.cpp
 * @brief R7RS case-lambda tests (Section 4.2.9)
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"

class CaseLambdaTest : public ZombieSchemeTest {};

// ============================================================================
// Basic Case-Lambda Creation
// ============================================================================

TEST_F(CaseLambdaTest, EmptyArityClause) {
    eval("(define zero (case-lambda (() 0)))");
    expectInt("(zero)", 0);
}

TEST_F(CaseLambdaTest, SingleArityClause) {
    eval("(define identity (case-lambda ((x) x)))");
    expectInt("(identity 42)", 42);
}

TEST_F(CaseLambdaTest, TwoArityClauses) {
    eval(R"(
        (define plus
          (case-lambda
            ((x) x)
            ((x y) (+ x y))))
    )");
    expectInt("(plus 5)", 5);
    expectInt("(plus 3 4)", 7);
}

TEST_F(CaseLambdaTest, ThreeArityClauses) {
    eval(R"(
        (define add
          (case-lambda
            (() 0)
            ((x) x)
            ((x y) (+ x y))
            ((x y z) (+ x y z))))
    )");
    expectInt("(add)", 0);
    expectInt("(add 5)", 5);
    expectInt("(add 2 3)", 5);
    expectInt("(add 1 2 3)", 6);
}

// ============================================================================
// Rest Parameters
// ============================================================================

TEST_F(CaseLambdaTest, RestParameterOnly) {
    eval(R"(
        (define collect
          (case-lambda
            (args args)))
    )");
    expectNil("(collect)");
    expectInt("(car (collect 1))", 1);
    expectInt("(car (collect 1 2 3))", 1);
}

TEST_F(CaseLambdaTest, MixedRestParameter) {
    eval(R"(
        (define flex
          (case-lambda
            (() 'zero)
            ((x) x)
            ((x . rest) (cons x rest))))
    )");
    expectSymbol("(flex)", "zero");
    expectInt("(flex 5)", 5);
    expectInt("(car (flex 1 2 3))", 1);
    expectInt("(car (cdr (flex 1 2 3)))", 2);
}

TEST_F(CaseLambdaTest, RestParameterWithMinimum) {
    eval(R"(
        (define at-least-two
          (case-lambda
            ((x y . rest) (+ x y))))
    )");
    expectInt("(at-least-two 3 4)", 7);
    expectInt("(at-least-two 1 2 3 4 5)", 3);
}

// ============================================================================
// Different Return Types
// ============================================================================

TEST_F(CaseLambdaTest, DifferentReturnTypes) {
    eval(R"(
        (define typed
          (case-lambda
            (() "zero")
            ((x) 'one)
            ((x y) 2)))
    )");
    expectString("(typed)", "zero");
    expectSymbol("(typed 'a)", "one");
    expectInt("(typed 'a 'b)", 2);
}

TEST_F(CaseLambdaTest, ReturningLists) {
    eval(R"(
        (define make-list
          (case-lambda
            (() '())
            ((x) (list x))
            ((x y) (list x y))))
    )");
    expectNil("(make-list)");
    expectInt("(car (make-list 1))", 1);
    expectInt("(car (make-list 1 2))", 1);
    expectInt("(car (cdr (make-list 1 2)))", 2);
}

// ============================================================================
// Computation in Different Arities
// ============================================================================

TEST_F(CaseLambdaTest, ComputationsInClauses) {
    eval(R"(
        (define math
          (case-lambda
            ((x) (* x x))
            ((x y) (* x y))
            ((x y z) (+ (* x y) z))))
    )");
    expectInt("(math 5)", 25);      // 5 * 5
    expectInt("(math 3 4)", 12);    // 3 * 4
    expectInt("(math 2 3 4)", 10);  // (2 * 3) + 4
}

TEST_F(CaseLambdaTest, RecursiveCallFromCaseLambda) {
    eval(R"(
        (define factorial
          (case-lambda
            (() 1)
            ((n) (if (<= n 1) 1 (* n (factorial (- n 1)))))))
    )");
    expectInt("(factorial)", 1);
    expectInt("(factorial 5)", 120);
}

// ============================================================================
// Closures and Lexical Scoping
// ============================================================================

TEST_F(CaseLambdaTest, CapturesLexicalEnvironment) {
    eval(R"(
        (define (make-adder n)
          (case-lambda
            (() n)
            ((x) (+ x n))
            ((x y) (+ x y n))))
    )");
    eval("(define add10 (make-adder 10))");
    expectInt("(add10)", 10);
    expectInt("(add10 5)", 15);
    expectInt("(add10 2 3)", 15);
}

TEST_F(CaseLambdaTest, NestedCaseLambda) {
    eval(R"(
        (define outer
          (case-lambda
            ((x)
             (case-lambda
               (() x)
               ((y) (+ x y))))))
    )");
    eval("(define inner (outer 10))");
    expectInt("(inner)", 10);
    expectInt("(inner 5)", 15);
}

// ============================================================================
// Error Handling
// ============================================================================

TEST_F(CaseLambdaTest, NoMatchingClauseThrows) {
    eval(R"(
        (define strict
          (case-lambda
            ((x) x)
            ((x y) (+ x y))))
    )");
    EXPECT_THROW(eval("(strict)"), std::runtime_error);          // No 0-arg clause
    EXPECT_THROW(eval("(strict 1 2 3)"), std::runtime_error);    // No 3-arg clause
}

TEST_F(CaseLambdaTest, EmptyCaseLambdaThrows) {
    EXPECT_THROW(eval("(case-lambda)"), std::runtime_error);
}

// ============================================================================
// Integration with define
// ============================================================================

TEST_F(CaseLambdaTest, DefineWithCaseLambda) {
    eval(R"(
        (define range
          (case-lambda
            ((n) (range 0 n))
            ((start end)
             (if (>= start end)
                 '()
                 (cons start (range (+ start 1) end))))))
    )");
    expectInt("(car (range 5))", 0);
    expectInt("(car (range 2 5))", 2);
}

// ============================================================================
// Higher-Order Functions
// ============================================================================

TEST_F(CaseLambdaTest, CaseLambdaAsArgument) {
    eval(R"(
        (define apply-with-default
          (lambda (f x)
            (f x 10)))
        (define add
          (case-lambda
            ((x) x)
            ((x y) (+ x y))))
    )");
    expectInt("(apply-with-default add 5)", 15);
}

TEST_F(CaseLambdaTest, ReturningCaseLambda) {
    eval(R"(
        (define make-multiplier
          (lambda (n)
            (case-lambda
              ((x) (* n x))
              ((x y) (* n x y)))))
    )");
    eval("(define triple (make-multiplier 3))");
    expectInt("(triple 5)", 15);
    expectInt("(triple 2 4)", 24);
}

// ============================================================================
// Variadic Behavior
// ============================================================================

TEST_F(CaseLambdaTest, VariadicSum) {
    eval(R"(
        (define sum
          (case-lambda
            (() 0)
            ((x) x)
            ((x y) (+ x y))
            ((x y . rest)
             (+ x y (apply sum rest)))))
    )");
    expectInt("(sum)", 0);
    expectInt("(sum 5)", 5);
    expectInt("(sum 1 2)", 3);
    expectInt("(sum 1 2 3)", 6);
    expectInt("(sum 1 2 3 4 5)", 15);
}

// ============================================================================
// Practical Examples
// ============================================================================

TEST_F(CaseLambdaTest, OptionalArguments) {
    eval(R"(
        (define greet
          (case-lambda
            ((name) (string-append "Hello, " name "!"))
            ((greeting name)
             (string-append greeting ", " name "!"))))
    )");
    expectString(R"((greet "Alice"))", "Hello, Alice!");
    expectString(R"((greet "Hi" "Bob"))", "Hi, Bob!");
}

TEST_F(CaseLambdaTest, DefaultValues) {
    eval(R"(
        (define power
          (case-lambda
            ((x) (* x x))
            ((x n)
             (if (= n 0)
                 1
                 (* x (power x (- n 1)))))))
    )");
    expectInt("(power 5)", 25);      // Default: square
    expectInt("(power 2 3)", 8);     // 2^3
    expectInt("(power 3 4)", 81);    // 3^4
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(CaseLambdaTest, SingleClauseWithRestArgs) {
    eval(R"(
        (define all-args
          (case-lambda
            (args (length args))))
    )");
    expectInt("(all-args)", 0);
    expectInt("(all-args 1)", 1);
    expectInt("(all-args 1 2 3)", 3);
}

TEST_F(CaseLambdaTest, ClauseOrderMatters) {
    eval(R"(
        (define first-match
          (case-lambda
            ((x . rest) 'many)
            ((x) 'one)))
    )");
    // First clause matches both cases, so always returns 'many
    expectSymbol("(first-match 1)", "many");
    expectSymbol("(first-match 1 2)", "many");
}

TEST_F(CaseLambdaTest, MultipleBodyExpressions) {
    eval(R"(
        (define with-side-effect
          (case-lambda
            ((x)
             (define temp (* x 2))
             (+ temp temp))))
    )");
    expectInt("(with-side-effect 5)", 20);
}
