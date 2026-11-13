#include "pch.h"
#include "TestFixture.h"

// ============================================================================
// Do Loop Tests
// ============================================================================

TEST_F(ZombieSchemeTest, DoLoopBasic) {
    // Simple do loop counting down
    Object* result = eval(
        "(do ((i 5 (- i 1))) "
        "    ((= i 0) i))");

    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 0);
}

TEST_F(ZombieSchemeTest, DoLoopSum) {
    // Do loop computing sum
    Object* result = eval(
        "(do ((i 0 (+ i 1)) "
        "     (sum 0 (+ sum i))) "
        "    ((> i 5) sum))");

    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 15);  // 0+1+2+3+4+5 = 15
}

TEST_F(ZombieSchemeTest, DoLoopFactorial) {
    // Do loop computing factorial
    Object* result = eval(
        "(do ((n 5 (- n 1)) "
        "     (fact 1 (* fact n))) "
        "    ((= n 0) fact))");

    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 120);  // 5! = 120
}

TEST_F(ZombieSchemeTest, DoLoopWithBody) {
    // Do loop with body expressions (side effects)
    eval("(define x 0)");
    Object* result = eval(
        "(do ((i 1 (+ i 1))) "
        "    ((> i 3) x) "
        "  (set! x (+ x i)))");

    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 6);  // 1+2+3 = 6
}

TEST_F(ZombieSchemeTest, DoLoopNoStep) {
    // Do loop with variable that doesn't change (no step expression)
    Object* result = eval(
        "(do ((i 5)) "
        "    ((= i 5) i))");

    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 5);
}

TEST_F(ZombieSchemeTest, DoLoopMultipleResults) {
    // Do loop with multiple result expressions
    Object* result = eval(
        "(do ((i 0 (+ i 1))) "
        "    ((= i 5) (+ i 10)))");  // Returns i + 10 = 15

    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 15);
}

TEST_F(ZombieSchemeTest, DoLoopNested) {
    // Nested do loops
    Object* result = eval(
        "(do ((i 1 (+ i 1)) "
        "     (sum 0 (+ sum "
        "               (do ((j 1 (+ j 1)) "
        "                    (inner 0 (+ inner j))) "
        "                   ((> j i) inner))))) "
        "    ((> i 3) sum))");

    // i=1: inner sum = 1
    // i=2: inner sum = 1+2 = 3
    // i=3: inner sum = 1+2+3 = 6
    // Total: 1+3+6 = 10
    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 10);
}

TEST_F(ZombieSchemeTest, DoLoopListProcessing) {
    // Do loop processing a list
    eval("(define lst '(1 2 3 4 5))");
    Object* result = eval(
        "(do ((l lst (cdr l)) "
        "     (sum 0 (+ sum (car l)))) "
        "    ((null? l) sum))");

    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 15);  // 1+2+3+4+5 = 15
}

TEST_F(ZombieSchemeTest, DoLoopReverseList) {
    // Do loop to reverse a list
    eval("(define lst '(1 2 3))");
    eval("(define result (do ((l lst (cdr l)) "
         "                    (rev '() (cons (car l) rev))) "
         "                   ((null? l) rev)))");

    Object* result = eval("result");
    ASSERT_TRUE(result->pair());

    // Should be (3 2 1)
    EXPECT_EQ(numeric::to_int64(car(result)), 3);
    EXPECT_EQ(numeric::to_int64(car(cdr(result))), 2);
    EXPECT_EQ(numeric::to_int64(car(cdr(cdr(result)))), 1);
}

TEST_F(ZombieSchemeTest, DoLoopZeroIterations) {
    // Do loop that exits immediately
    Object* result = eval(
        "(do ((i 0 (+ i 1))) "
        "    (#t 42))");  // Test is always true

    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 42);
}
