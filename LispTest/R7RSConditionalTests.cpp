/**
 * @file R7RSConditionalTests.cpp
 * @brief R7RS conditional control flow (when/unless) tests
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"

class R7RSConditionalTest : public ZombieSchemeTest {};

// ============================================================================
// when Tests
// ============================================================================

TEST_F(R7RSConditionalTest, WhenBasicTrue) {
    eval("(define x 0)");
    eval("(when #t (set! x 1))");
    expectInt("x", 1);
}

TEST_F(R7RSConditionalTest, WhenBasicFalse) {
    eval("(define x 0)");
    eval("(when #f (set! x 1))");
    expectInt("x", 0);
}

TEST_F(R7RSConditionalTest, WhenMultipleExpressions) {
    eval("(define x 0)");
    eval("(define y 0)");
    eval("(when #t (set! x 1) (set! y 2))");
    expectInt("x", 1);
    expectInt("y", 2);
}

TEST_F(R7RSConditionalTest, WhenMultipleExpressionsFalse) {
    eval("(define x 0)");
    eval("(define y 0)");
    eval("(when #f (set! x 1) (set! y 2))");
    expectInt("x", 0);
    expectInt("y", 0);
}

TEST_F(R7RSConditionalTest, WhenWithExpression) {
    eval("(define x 5)");
    eval("(when (> x 3) (set! x (* x 2)))");
    expectInt("x", 10);
}

TEST_F(R7RSConditionalTest, WhenWithExpressionFalse) {
    eval("(define x 2)");
    eval("(when (> x 3) (set! x (* x 2)))");
    expectInt("x", 2);
}

TEST_F(R7RSConditionalTest, WhenReturnsValue) {
    expectInt("(when #t 42)", 42);
}

TEST_F(R7RSConditionalTest, WhenSequence) {
    expectInt("(when #t 1 2 3)", 3);
}

TEST_F(R7RSConditionalTest, WhenNested) {
    eval("(define x 0)");
    eval(R"(
        (when #t
            (set! x 1)
            (when #t
                (set! x 2)))
    )");
    expectInt("x", 2);
}

TEST_F(R7RSConditionalTest, WhenNestedFalse) {
    eval("(define x 0)");
    eval(R"(
        (when #t
            (set! x 1)
            (when #f
                (set! x 2)))
    )");
    expectInt("x", 1);
}

TEST_F(R7RSConditionalTest, WhenInLambda) {
    eval(R"(
        (define (increment-positive x)
            (when (> x 0)
                (+ x 1)))
    )");
    expectInt("(increment-positive 5)", 6);
}

TEST_F(R7RSConditionalTest, WhenInLambdaFalse) {
    eval(R"(
        (define (increment-positive x)
            (when (> x 0)
                (+ x 1)))
    )");
    eval("(define result (increment-positive -5))");
    // When returns unspecified when test is false
    // We'll check that it doesn't crash
}

TEST_F(R7RSConditionalTest, WhenWithSideEffects) {
    eval("(define count 0)");
    eval(R"(
        (define (inc)
            (set! count (+ count 1)))
    )");
    eval("(when #t (inc) (inc) (inc))");
    expectInt("count", 3);
}

TEST_F(R7RSConditionalTest, WhenZeroTest) {
    eval("(define x 10)");
    eval("(when (= 0 0) (set! x 20))");
    expectInt("x", 20);
}

TEST_F(R7RSConditionalTest, WhenNonEmptyList) {
    eval("(define lst '(1 2 3))");
    eval("(when (not (null? lst)) (set! lst (cdr lst)))");
    expectInt("(car lst)", 2);
}

// ============================================================================
// unless Tests
// ============================================================================

TEST_F(R7RSConditionalTest, UnlessBasicTrue) {
    eval("(define x 0)");
    eval("(unless #t (set! x 1))");
    expectInt("x", 0);
}

TEST_F(R7RSConditionalTest, UnlessBasicFalse) {
    eval("(define x 0)");
    eval("(unless #f (set! x 1))");
    expectInt("x", 1);
}

TEST_F(R7RSConditionalTest, UnlessMultipleExpressions) {
    eval("(define x 0)");
    eval("(define y 0)");
    eval("(unless #f (set! x 1) (set! y 2))");
    expectInt("x", 1);
    expectInt("y", 2);
}

TEST_F(R7RSConditionalTest, UnlessMultipleExpressionsTrue) {
    eval("(define x 0)");
    eval("(define y 0)");
    eval("(unless #t (set! x 1) (set! y 2))");
    expectInt("x", 0);
    expectInt("y", 0);
}

TEST_F(R7RSConditionalTest, UnlessWithExpression) {
    eval("(define x 2)");
    eval("(unless (> x 3) (set! x (* x 2)))");
    expectInt("x", 4);
}

TEST_F(R7RSConditionalTest, UnlessWithExpressionTrue) {
    eval("(define x 5)");
    eval("(unless (> x 3) (set! x (* x 2)))");
    expectInt("x", 5);
}

TEST_F(R7RSConditionalTest, UnlessReturnsValue) {
    expectInt("(unless #f 42)", 42);
}

TEST_F(R7RSConditionalTest, UnlessSequence) {
    expectInt("(unless #f 1 2 3)", 3);
}

TEST_F(R7RSConditionalTest, UnlessNested) {
    eval("(define x 0)");
    eval(R"(
        (unless #f
            (set! x 1)
            (unless #f
                (set! x 2)))
    )");
    expectInt("x", 2);
}

TEST_F(R7RSConditionalTest, UnlessNestedTrue) {
    eval("(define x 0)");
    eval(R"(
        (unless #f
            (set! x 1)
            (unless #t
                (set! x 2)))
    )");
    expectInt("x", 1);
}

TEST_F(R7RSConditionalTest, UnlessInLambda) {
    eval(R"(
        (define (decrement-non-positive x)
            (unless (> x 0)
                (- x 1)))
    )");
    expectInt("(decrement-non-positive -5)", -6);
}

TEST_F(R7RSConditionalTest, UnlessInLambdaTrue) {
    eval(R"(
        (define (decrement-non-positive x)
            (unless (> x 0)
                (- x 1)))
    )");
    eval("(define result (decrement-non-positive 5))");
    // Unless returns unspecified when test is true
    // We'll check that it doesn't crash
}

TEST_F(R7RSConditionalTest, UnlessWithSideEffects) {
    eval("(define count 0)");
    eval(R"(
        (define (inc)
            (set! count (+ count 1)))
    )");
    eval("(unless #f (inc) (inc) (inc))");
    expectInt("count", 3);
}

TEST_F(R7RSConditionalTest, UnlessZeroTest) {
    eval("(define x 10)");
    eval("(unless (= 1 0) (set! x 20))");
    expectInt("x", 20);
}

TEST_F(R7RSConditionalTest, UnlessEmptyList) {
    eval("(define lst '())");
    eval("(unless (null? lst) (set! lst '(1 2 3)))");
    expectBool("(null? lst)", true);
}

// ============================================================================
// when/unless Integration Tests
// ============================================================================

TEST_F(R7RSConditionalTest, WhenUnlessCombined) {
    eval("(define x 0)");
    eval(R"(
        (when #t
            (set! x 1)
            (unless #f
                (set! x 2)))
    )");
    expectInt("x", 2);
}

TEST_F(R7RSConditionalTest, WhenUnlessMixed) {
    eval("(define x 0)");
    eval("(define y 0)");
    eval(R"(
        (when (> 5 3)
            (set! x 1)
            (unless (< 5 3)
                (set! y 2)))
    )");
    expectInt("x", 1);
    expectInt("y", 2);
}

TEST_F(R7RSConditionalTest, GuardAgainstNegative) {
    eval(R"(
        (define (safe-divide x y)
            (unless (= y 0)
                (/ x y)))
    )");
    expectInt("(safe-divide 10 2)", 5);
}

TEST_F(R7RSConditionalTest, ConditionalIncrement) {
    eval(R"(
        (define (conditional-inc x)
            (when (> x 0)
                (+ x 1)))
    )");
    eval(R"(
        (define (conditional-dec x)
            (unless (> x 0)
                (- x 1)))
    )");
    expectInt("(conditional-inc 5)", 6);
    expectInt("(conditional-dec -5)", -6);
}

TEST_F(R7RSConditionalTest, WhenUnlessInCond) {
    eval(R"(
        (define (classify x)
            (cond
                ((> x 0) (when #t 'positive))
                ((< x 0) (unless #f 'negative))
                (else 'zero)))
    )");
    expectSymbol("(classify 5)", "positive");
    expectSymbol("(classify -5)", "negative");
    expectSymbol("(classify 0)", "zero");
}

// ============================================================================
// Practical Examples
// ============================================================================

TEST_F(R7RSConditionalTest, ValidateInput) {
    eval(R"(
        (define (validate-positive x)
            (unless (> x 0)
                (error "Expected positive number")))
    )");
    // This test just verifies the function is created
    // We can't easily test error throwing without try/catch
}

TEST_F(R7RSConditionalTest, ConditionalLogging) {
    eval("(define log '())");
    eval(R"(
        (define (log-if-debug msg debug?)
            (when debug?
                (set! log (cons msg log))))
    )");
    eval("(log-if-debug 'test1 #t)");
    eval("(log-if-debug 'test2 #f)");
    eval("(log-if-debug 'test3 #t)");
    expectInt("(length log)", 2);
    expectSymbol("(car log)", "test3");
}

TEST_F(R7RSConditionalTest, LazyEvaluation) {
    eval("(define x 0)");
    eval(R"(
        (define (expensive-computation)
            (set! x (+ x 1))
            42)
    )");
    eval("(when #f (expensive-computation))");
    expectInt("x", 0);  // Should not call expensive computation
}

TEST_F(R7RSConditionalTest, LazyEvaluationUnless) {
    eval("(define x 0)");
    eval(R"(
        (define (expensive-computation)
            (set! x (+ x 1))
            42)
    )");
    eval("(unless #t (expensive-computation))");
    expectInt("x", 0);  // Should not call expensive computation
}

TEST_F(R7RSConditionalTest, BoundaryCheck) {
    eval(R"(
        (define (in-range? x min max)
            (when (< x min)
                (error "Below minimum"))
            (when (> x max)
                (error "Above maximum"))
            #t)
    )");
    // Test passes if no error for valid input
    expectBool("(in-range? 5 0 10)", true);
}

TEST_F(R7RSConditionalTest, ListProcessing) {
    eval(R"(
        (define (process-non-empty-list lst)
            (when (not (null? lst))
                (car lst)))
    )");
    expectInt("(process-non-empty-list '(1 2 3))", 1);
}

TEST_F(R7RSConditionalTest, ConditionalAccumulation) {
    eval("(define sum 0)");
    eval(R"(
        (define (add-positive x)
            (when (> x 0)
                (set! sum (+ sum x))))
    )");
    eval("(add-positive 5)");
    eval("(add-positive -3)");
    eval("(add-positive 7)");
    eval("(add-positive -2)");
    expectInt("sum", 12);
}

TEST_F(R7RSConditionalTest, SymbolInterning) {
    // Test symbol interning - eq? should work with symbols now
    eval("(define state 'init)");
    expectSymbol("state", "init");  // Verify initial state

    // Test eq? with symbols (should work now with symbol interning)
    expectBool("(eq? state 'init)", true);
    expectBool("(eq? 'init 'init)", true);
    expectBool("(eq? 'running 'init)", false);

    // Also test eqv? still works
    expectBool("(eqv? state 'init)", true);
    expectBool("(eqv? 'init 'init)", true);
    expectBool("(eqv? 'running 'init)", false);
}

TEST_F(R7RSConditionalTest, StateMachineSimple) {
    // Simplified version to debug the issue
    eval("(define state 'init)");
    expectSymbol("state", "init");  // Verify initial state

    // Test 1: Simple when with set! (using eq? now that symbols are interned)
    eval("(when (eq? state 'init) (set! state 'running))");
    expectSymbol("state", "running");
}

TEST_F(R7RSConditionalTest, StateMachineNested) {
    // Test nested when statements
    eval("(define state 'init)");
    eval("(define cmd 'start)");
    expectSymbol("state", "init");

    // Test 2: Nested when (using eq? now that symbols are interned)
    eval("(when (eq? state 'init) (when (eq? cmd 'start) (set! state 'running)))");
    expectSymbol("state", "running");
}

TEST_F(R7RSConditionalTest, StateMachine) {
    eval("(define state 'init)");
    eval(R"(
        (define (transition cmd)
            (when (eq? state 'init)
                (when (eq? cmd 'start)
                    (set! state 'running)))
            (when (eq? state 'running)
                (when (eq? cmd 'stop)
                    (set! state 'stopped))))
    )");
    eval("(transition 'start)");
    expectSymbol("state", "running");
    eval("(transition 'stop)");
    expectSymbol("state", "stopped");
}
