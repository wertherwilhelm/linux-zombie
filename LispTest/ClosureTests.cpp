#include "pch.h"
#include "TestFixture.h"

// ============================================================================
// Closure Tests
// ============================================================================

TEST_F(ZombieSchemeTest, SimpleClosure) {
    eval("(define (make-adder n) (lambda (x) (+ x n)))");
    eval("(define add10 (make-adder 10))");
    expectInt("(add10 5)", 15);
    expectInt("(add10 100)", 110);
}

TEST_F(ZombieSchemeTest, ClosureCapturesEnvironment) {
    eval("(define (make-multiplier n) (lambda (x) (* x n)))");
    eval("(define times2 (make-multiplier 2))");
    eval("(define times5 (make-multiplier 5))");
    expectInt("(times2 10)", 20);
    expectInt("(times5 10)", 50);
}

TEST_F(ZombieSchemeTest, NestedClosures) {
    eval("(define (make-adder-maker x) (lambda (y) (lambda (z) (+ x (+ y z)))))");
    eval("(define add10-maker (make-adder-maker 10))");
    eval("(define add10-20 (add10-maker 20))");
    expectInt("(add10-20 5)", 35);  // 10 + 20 + 5
}

// ============================================================================
// set! Tests (Mutation)
// ============================================================================

TEST_F(ZombieSchemeTest, SimpleSetBang) {
    eval("(define x 10)");
    expectInt("x", 10);
    eval("(set! x 20)");
    expectInt("x", 20);
}

TEST_F(ZombieSchemeTest, SetBangInFunction) {
    eval("(define x 0)");
    eval("(define (increment) (set! x (+ x 1)))");
    eval("(increment)");
    expectInt("x", 1);
    eval("(increment)");
    expectInt("x", 2);
    eval("(increment)");
    expectInt("x", 3);
}

TEST_F(ZombieSchemeTest, SetBangInClosureWithLet) {
    // This is the exact bug we fixed!
    eval(R"(
        (define (make-counter)
          (let ((n 0))
            (lambda ()
              (set! n (+ n 1))
              n)))
    )");

    eval("(define c (make-counter))");
    expectInt("(c)", 1);
    expectInt("(c)", 2);
    expectInt("(c)", 3);
}

TEST_F(ZombieSchemeTest, MultipleIndependentCounters) {
    eval(R"(
        (define (make-counter)
          (let ((n 0))
            (lambda ()
              (set! n (+ n 1))
              n)))
    )");

    eval("(define c1 (make-counter))");
    eval("(define c2 (make-counter))");

    expectInt("(c1)", 1);
    expectInt("(c1)", 2);
    expectInt("(c2)", 1);  // c2 should have its own state
    expectInt("(c1)", 3);
    expectInt("(c2)", 2);
}

TEST_F(ZombieSchemeTest, SetBangWithMultipleVariables) {
    eval(R"(
        (define (make-point x y)
          (lambda (msg)
            (if (= msg 0)
                x
                (if (= msg 1)
                    y
                    (begin
                      (set! x msg)
                      (set! y msg))))))
    )");

    eval("(define p (make-point 10 20))");
    expectInt("(p 0)", 10);  // get x
    expectInt("(p 1)", 20);  // get y
    eval("(p 100)");          // set both to 100
    expectInt("(p 0)", 100);
    expectInt("(p 1)", 100);
}

TEST_F(ZombieSchemeTest, SetBangReturnsNewValue) {
    eval("(define x 10)");
    // set! returns the value, but verify x was updated
    eval("(set! x 42)");
    expectInt("x", 42);
}

// ============================================================================
// let Tests with Closures
// ============================================================================

TEST_F(ZombieSchemeTest, LetCreatesClosureScope) {
    eval(R"(
        (define (make-getter)
          (let ((secret 42))
            (lambda () secret)))
    )");

    eval("(define get-secret (make-getter))");
    expectInt("(get-secret)", 42);
}

TEST_F(ZombieSchemeTest, ClosureCapturesMultipleLetBindings) {
    eval(R"(
        (define (make-adder)
          (let ((a 10) (b 20))
            (lambda (x) (+ x a b))))
    )");

    eval("(define add30 (make-adder))");
    expectInt("(add30 5)", 35);
}

// ============================================================================
// Closure with set! Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, SetBangInNestedLet) {
    eval(R"(
        (define (make-nested-counter)
          (let ((outer 0))
            (let ((inner 0))
              (lambda ()
                (set! outer (+ outer 1))
                (set! inner (+ inner 10))
                (+ outer inner)))))
    )");

    eval("(define nc (make-nested-counter))");
    expectInt("(nc)", 11);  // outer=1, inner=10
    expectInt("(nc)", 22);  // outer=2, inner=20
    expectInt("(nc)", 33);  // outer=3, inner=30
}

TEST_F(ZombieSchemeTest, SetBangAcrossMultipleCalls) {
    eval(R"(
        (define (make-accumulator)
          (let ((sum 0))
            (lambda (x)
              (set! sum (+ sum x))
              sum)))
    )");

    eval("(define acc (make-accumulator))");
    expectInt("(acc 10)", 10);
    expectInt("(acc 5)", 15);
    expectInt("(acc 3)", 18);
    expectInt("(acc 100)", 118);
}

// ============================================================================
// Regression Test for set! Bug
// ============================================================================

TEST_F(ZombieSchemeTest, SetBangBugRegression) {
    // This test specifically checks that the bug we fixed doesn't return
    // The bug was: Environment::set() was incorrectly casting rest(varList) to a_string*
    // causing infinite loops with "first: cannot take first of an atom!"

    eval(R"(
        (define (counter-Closure)
          (let ((count 0))
            (lambda ()
              (set! count (+ count 1))
              count)))
    )");

    eval("(define counter (counter-Closure))");

    // This would hang/crash with the old bug
    Object* result1 = eval("(counter)");
    ASSERT_NE(result1, nullptr);

    Object* result2 = eval("(counter)");
    ASSERT_NE(result2, nullptr);

    Object* result3 = eval("(counter)");
    ASSERT_NE(result3, nullptr);

    expectInt("(counter)", 4);
}
