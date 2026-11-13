/**
 * @file R7RSVectorTests.cpp
 * @brief R7RS additional vector Primitive tests
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"

class R7RSVectorTest : public ZombieSchemeTest {};

// ============================================================================
// vector-copy Tests
// ============================================================================

TEST_F(R7RSVectorTest, VectorCopyBasic) {
    eval("(define orig (vector 1 2 3))");
    eval("(define copy (vector-copy orig))");
    expectInt("(vector-ref copy 0)", 1);
    expectInt("(vector-ref copy 1)", 2);
    expectInt("(vector-ref copy 2)", 3);
}

TEST_F(R7RSVectorTest, VectorCopyIndependent) {
    eval("(define orig (vector 1 2 3))");
    eval("(define copy (vector-copy orig))");
    eval("(vector-set! copy 0 99)");
    expectInt("(vector-ref orig 0)", 1);   // Original unchanged
    expectInt("(vector-ref copy 0)", 99);  // Copy modified
}

TEST_F(R7RSVectorTest, VectorCopyWithStart) {
    eval("(define orig (vector 1 2 3 4 5))");
    eval("(define copy (vector-copy orig 2))");
    expectInt("(vector-length copy)", 3);
    expectInt("(vector-ref copy 0)", 3);
    expectInt("(vector-ref copy 1)", 4);
    expectInt("(vector-ref copy 2)", 5);
}

TEST_F(R7RSVectorTest, VectorCopyWithStartAndEnd) {
    eval("(define orig (vector 1 2 3 4 5))");
    eval("(define copy (vector-copy orig 1 4))");
    expectInt("(vector-length copy)", 3);
    expectInt("(vector-ref copy 0)", 2);
    expectInt("(vector-ref copy 1)", 3);
    expectInt("(vector-ref copy 2)", 4);
}

TEST_F(R7RSVectorTest, VectorCopyEmpty) {
    eval("(define empty (vector))");
    eval("(define copy (vector-copy empty))");
    expectInt("(vector-length copy)", 0);
}

TEST_F(R7RSVectorTest, VectorCopyEmptyRange) {
    eval("(define orig (vector 1 2 3))");
    eval("(define copy (vector-copy orig 1 1))");
    expectInt("(vector-length copy)", 0);
}

TEST_F(R7RSVectorTest, VectorCopyPreservesTypes) {
    eval("(define orig (vector 1 \"hello\" 'x #t))");
    eval("(define copy (vector-copy orig))");
    expectInt("(vector-ref copy 0)", 1);
    expectString("(vector-ref copy 1)", "hello");
    expectSymbol("(vector-ref copy 2)", "x");
    expectBool("(vector-ref copy 3)", true);
}

// ============================================================================
// vector-append Tests
// ============================================================================

TEST_F(R7RSVectorTest, VectorAppendBasic) {
    eval("(define v (vector-append (vector 1 2) (vector 3 4)))");
    expectInt("(vector-length v)", 4);
    expectInt("(vector-ref v 0)", 1);
    expectInt("(vector-ref v 1)", 2);
    expectInt("(vector-ref v 2)", 3);
    expectInt("(vector-ref v 3)", 4);
}

TEST_F(R7RSVectorTest, VectorAppendEmpty) {
    eval("(define v (vector-append))");
    expectInt("(vector-length v)", 0);
}

TEST_F(R7RSVectorTest, VectorAppendSingle) {
    eval("(define v (vector-append (vector 1 2 3)))");
    expectInt("(vector-length v)", 3);
    expectInt("(vector-ref v 0)", 1);
}

TEST_F(R7RSVectorTest, VectorAppendMultiple) {
    eval("(define v (vector-append (vector 1) (vector 2) (vector 3) (vector 4)))");
    expectInt("(vector-length v)", 4);
    expectInt("(vector-ref v 0)", 1);
    expectInt("(vector-ref v 3)", 4);
}

TEST_F(R7RSVectorTest, VectorAppendWithEmpty) {
    eval("(define v (vector-append (vector 1 2) (vector) (vector 3)))");
    expectInt("(vector-length v)", 3);
    expectInt("(vector-ref v 0)", 1);
    expectInt("(vector-ref v 1)", 2);
    expectInt("(vector-ref v 2)", 3);
}

TEST_F(R7RSVectorTest, VectorAppendMixedTypes) {
    eval("(define v (vector-append (vector 1 2) (vector \"a\" \"b\")))");
    expectInt("(vector-ref v 0)", 1);
    expectInt("(vector-ref v 1)", 2);
    expectString("(vector-ref v 2)", "a");
    expectString("(vector-ref v 3)", "b");
}

// ============================================================================
// vector-map Tests
// ============================================================================

TEST_F(R7RSVectorTest, VectorMapSingleVector) {
    eval("(define v (vector-map (lambda (x) (* x 2)) (vector 1 2 3)))");
    expectInt("(vector-ref v 0)", 2);
    expectInt("(vector-ref v 1)", 4);
    expectInt("(vector-ref v 2)", 6);
}

TEST_F(R7RSVectorTest, VectorMapTwoVectors) {
    eval("(define v (vector-map + (vector 1 2 3) (vector 10 20 30)))");
    expectInt("(vector-ref v 0)", 11);
    expectInt("(vector-ref v 1)", 22);
    expectInt("(vector-ref v 2)", 33);
}

TEST_F(R7RSVectorTest, VectorMapThreeVectors) {
    eval(R"(
        (define v (vector-map
                    (lambda (x y z) (+ x y z))
                    (vector 1 2 3)
                    (vector 10 20 30)
                    (vector 100 200 300)))
    )");
    expectInt("(vector-ref v 0)", 111);
    expectInt("(vector-ref v 1)", 222);
    expectInt("(vector-ref v 2)", 333);
}

TEST_F(R7RSVectorTest, VectorMapEmpty) {
    eval("(define v (vector-map (lambda (x) (* x 2)) (vector)))");
    expectInt("(vector-length v)", 0);
}

TEST_F(R7RSVectorTest, VectorMapWithBuiltin) {
    eval("(define v (vector-map abs (vector -1 -2 -3)))");
    expectInt("(vector-ref v 0)", 1);
    expectInt("(vector-ref v 1)", 2);
    expectInt("(vector-ref v 2)", 3);
}

TEST_F(R7RSVectorTest, VectorMapNonNumeric) {
    eval(R"(
        (define v (vector-map
                    (lambda (s) (string-append s "!"))
                    (vector "hello" "world")))
    )");
    expectString("(vector-ref v 0)", "hello!");
    expectString("(vector-ref v 1)", "world!");
}

TEST_F(R7RSVectorTest, VectorMapPreservesLength) {
    eval("(define orig (vector 1 2 3 4 5))");
    eval("(define mapped (vector-map (lambda (x) (* x x)) orig))");
    expectInt("(vector-length mapped)", 5);
    expectInt("(vector-ref mapped 0)", 1);
    expectInt("(vector-ref mapped 4)", 25);
}

// ============================================================================
// vector-for-each Tests
// ============================================================================

TEST_F(R7RSVectorTest, VectorForEachSideEffect) {
    eval("(define sum 0)");
    eval("(vector-for-each (lambda (x) (set! sum (+ sum x))) (vector 1 2 3))");
    expectInt("sum", 6);
}

TEST_F(R7RSVectorTest, VectorForEachTwoVectors) {
    eval("(define product 1)");
    eval(R"(
        (vector-for-each
            (lambda (x y) (set! product (* product x y)))
            (vector 2 3)
            (vector 4 5))
    )");
    expectInt("product", 120);  // 1 * (2*4) * (3*5) = 1 * 8 * 15 = 120
}

TEST_F(R7RSVectorTest, VectorForEachEmpty) {
    eval("(define count 0)");
    eval("(vector-for-each (lambda (x) (set! count (+ count 1))) (vector))");
    expectInt("count", 0);
}

TEST_F(R7RSVectorTest, VectorForEachCountElements) {
    eval("(define count 0)");
    eval("(vector-for-each (lambda (x) (set! count (+ count 1))) (vector 'a 'b 'c 'd))");
    expectInt("count", 4);
}

TEST_F(R7RSVectorTest, VectorForEachBuildList) {
    eval("(define result '())");
    eval("(vector-for-each (lambda (x) (set! result (cons x result))) (vector 1 2 3))");
    expectInt("(car result)", 3);
    expectInt("(cadr result)", 2);
    expectInt("(caddr result)", 1);
}

TEST_F(R7RSVectorTest, VectorForEachWithIndex) {
    eval(R"(
        (define results (make-vector 3 0))
        (vector-for-each
            (lambda (val idx) (vector-set! results idx val))
            (vector 10 20 30)
            (vector 0 1 2))
    )");
    expectInt("(vector-ref results 0)", 10);
    expectInt("(vector-ref results 1)", 20);
    expectInt("(vector-ref results 2)", 30);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(R7RSVectorTest, CopyAndMap) {
    eval("(define orig (vector 1 2 3))");
    eval("(define copy (vector-copy orig))");
    eval("(define doubled (vector-map (lambda (x) (* x 2)) copy))");
    expectInt("(vector-ref doubled 0)", 2);
    expectInt("(vector-ref doubled 1)", 4);
    expectInt("(vector-ref doubled 2)", 6);
    expectInt("(vector-ref orig 0)", 1);  // Original unchanged
}

TEST_F(R7RSVectorTest, AppendAndMap) {
    eval(R"(
        (define v1 (vector 1 2))
        (define v2 (vector 3 4))
        (define combined (vector-append v1 v2))
        (define squared (vector-map (lambda (x) (* x x)) combined))
    )");
    expectInt("(vector-ref squared 0)", 1);
    expectInt("(vector-ref squared 1)", 4);
    expectInt("(vector-ref squared 2)", 9);
    expectInt("(vector-ref squared 3)", 16);
}

TEST_F(R7RSVectorTest, MapAndForEach) {
    eval("(define sum 0)");
    eval("(define v (vector-map (lambda (x) (* x 2)) (vector 1 2 3)))");
    eval("(vector-for-each (lambda (x) (set! sum (+ sum x))) v)");
    expectInt("sum", 12);  // (1*2) + (2*2) + (3*2) = 2+4+6 = 12
}

TEST_F(R7RSVectorTest, CopySubrange) {
    eval("(define orig (vector 0 1 2 3 4 5 6 7 8 9))");
    eval("(define middle (vector-copy orig 3 7))");
    expectInt("(vector-length middle)", 4);
    expectInt("(vector-ref middle 0)", 3);
    expectInt("(vector-ref middle 3)", 6);
}

TEST_F(R7RSVectorTest, ChainedOperations) {
    eval(R"(
        (define result
            (vector-map
                (lambda (x) (+ x 10))
                (vector-append
                    (vector-copy (vector 1 2 3) 0 2)
                    (vector 4 5))))
    )");
    expectInt("(vector-length result)", 4);
    expectInt("(vector-ref result 0)", 11);  // 1 + 10
    expectInt("(vector-ref result 1)", 12);  // 2 + 10
    expectInt("(vector-ref result 2)", 14);  // 4 + 10
    expectInt("(vector-ref result 3)", 15);  // 5 + 10
}

// ============================================================================
// Practical Examples
// ============================================================================

TEST_F(R7RSVectorTest, DotProduct) {
    eval(R"(
        (define (dot-product v1 v2)
            (define products (vector-map * v1 v2))
            (define sum 0)
            (vector-for-each (lambda (x) (set! sum (+ sum x))) products)
            sum)
        (define result (dot-product (vector 1 2 3) (vector 4 5 6)))
    )");
    expectInt("result", 32);  // 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32
}

TEST_F(R7RSVectorTest, VectorAddition) {
    eval(R"(
        (define (vector-add v1 v2)
            (vector-map + v1 v2))
        (define result (vector-add (vector 1 2 3) (vector 10 20 30)))
    )");
    expectInt("(vector-ref result 0)", 11);
    expectInt("(vector-ref result 1)", 22);
    expectInt("(vector-ref result 2)", 33);
}

TEST_F(R7RSVectorTest, FilterVector) {
    eval(R"(
        (define (vector-filter pred vec)
            (define result '())
            (vector-for-each
                (lambda (x)
                    (if (pred x)
                        (set! result (cons x result))))
                vec)
            (list->vector (reverse result)))
        (define evens (vector-filter even? (vector 1 2 3 4 5 6)))
    )");
    expectInt("(vector-length evens)", 3);
    expectInt("(vector-ref evens 0)", 2);
    expectInt("(vector-ref evens 1)", 4);
    expectInt("(vector-ref evens 2)", 6);
}

TEST_F(R7RSVectorTest, VectorSum) {
    eval(R"(
        (define (vector-sum vec)
            (define sum 0)
            (vector-for-each (lambda (x) (set! sum (+ sum x))) vec)
            sum)
        (define total (vector-sum (vector 1 2 3 4 5)))
    )");
    expectInt("total", 15);
}

TEST_F(R7RSVectorTest, VectorReverse) {
    eval(R"(
        (define (vector-reverse vec)
            (define result (make-vector (vector-length vec)))
            (define len (vector-length vec))
            (vector-for-each
                (lambda (val idx)
                    (vector-set! result (- (- len 1) idx) val))
                vec
                (list->vector (list 0 1 2 3 4)))
            result)
        (define rev (vector-reverse (vector 1 2 3 4 5)))
    )");
    expectInt("(vector-ref rev 0)", 5);
    expectInt("(vector-ref rev 1)", 4);
    expectInt("(vector-ref rev 2)", 3);
    expectInt("(vector-ref rev 3)", 2);
    expectInt("(vector-ref rev 4)", 1);
}
