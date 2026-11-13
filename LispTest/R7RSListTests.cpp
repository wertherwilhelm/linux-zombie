/**
 * @file R7RSListTests.cpp
 * @brief R7RS additional list Primitive tests
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"

class R7RSListTest : public ZombieSchemeTest {};

// ============================================================================
// make-list Tests
// ============================================================================

TEST_F(R7RSListTest, MakeListZero) {
    expectNil("(make-list 0)");
}

TEST_F(R7RSListTest, MakeListBasic) {
    eval("(define lst (make-list 3))");
    expectInt("(length lst)", 3);
    expectNil("(car lst)");
    expectNil("(cadr lst)");
    expectNil("(caddr lst)");
}

TEST_F(R7RSListTest, MakeListWithFill) {
    eval("(define lst (make-list 4 'x))");
    expectInt("(length lst)", 4);
    expectSymbol("(car lst)", "x");
    expectSymbol("(cadr lst)", "x");
    expectSymbol("(caddr lst)", "x");
    expectSymbol("(car (cdddr lst))", "x");
}

TEST_F(R7RSListTest, MakeListWithNumberFill) {
    eval("(define lst (make-list 3 10))");
    expectInt("(car lst)", 10);
    expectInt("(cadr lst)", 10);
    expectInt("(caddr lst)", 10);
}

TEST_F(R7RSListTest, MakeListWithStringFill) {
    eval("(define lst (make-list 2 \"hello\"))");
    expectString("(car lst)", "hello");
    expectString("(cadr lst)", "hello");
}

TEST_F(R7RSListTest, MakeListLarge) {
    eval("(define big (make-list 100 42))");
    expectInt("(length big)", 100);
    expectInt("(car big)", 42);
    expectInt("(list-ref big 50)", 42);
    expectInt("(list-ref big 99)", 42);
}

TEST_F(R7RSListTest, MakeListWithListFill) {
    eval("(define lst (make-list 2 '(a b)))");
    expectSymbol("(car (car lst))", "a");
    expectSymbol("(cadr (car lst))", "b");
}

// ============================================================================
// list-copy Tests
// ============================================================================

TEST_F(R7RSListTest, ListCopyEmpty) {
    expectNil("(list-copy '())");
}

TEST_F(R7RSListTest, ListCopyBasic) {
    eval("(define orig '(1 2 3))");
    eval("(define copy (list-copy orig))");
    expectInt("(car copy)", 1);
    expectInt("(cadr copy)", 2);
    expectInt("(caddr copy)", 3);
}

TEST_F(R7RSListTest, ListCopyIndependent) {
    eval("(define orig '(1 2 3))");
    eval("(define copy (list-copy orig))");
    eval("(set-car! copy 99)");
    expectInt("(car orig)", 1);    // Original unchanged
    expectInt("(car copy)", 99);   // Copy modified
}

TEST_F(R7RSListTest, ListCopyNestedShallow) {
    eval("(define orig '((a b) c))");
    eval("(define copy (list-copy orig))");
    eval("(set-car! (car copy) 'x)");
    // Shallow copy - nested list is shared
    expectSymbol("(car (car orig))", "x");
    expectSymbol("(car (car copy))", "x");
}

TEST_F(R7RSListTest, ListCopyImproperList) {
    eval("(define orig '(1 2 . 3))");
    eval("(define copy (list-copy orig))");
    expectInt("(car copy)", 1);
    expectInt("(cadr copy)", 2);
    expectInt("(cddr copy)", 3);
}

TEST_F(R7RSListTest, ListCopyPreservesTypes) {
    eval("(define orig (list 1 \"hello\" 'x #t))");
    eval("(define copy (list-copy orig))");
    expectInt("(car copy)", 1);
    expectString("(cadr copy)", "hello");
    expectSymbol("(caddr copy)", "x");
    expectBool("(car (cdddr copy))", true);
}

// ============================================================================
// list-set! Tests
// ============================================================================

TEST_F(R7RSListTest, ListSetBasic) {
    eval("(define lst (list 1 2 3))");
    eval("(list-set! lst 0 'a)");
    expectSymbol("(car lst)", "a");
    expectInt("(cadr lst)", 2);
    expectInt("(caddr lst)", 3);
}

TEST_F(R7RSListTest, ListSetMiddle) {
    eval("(define lst (list 1 2 3))");
    eval("(list-set! lst 1 'b)");
    expectInt("(car lst)", 1);
    expectSymbol("(cadr lst)", "b");
    expectInt("(caddr lst)", 3);
}

TEST_F(R7RSListTest, ListSetLast) {
    eval("(define lst (list 1 2 3))");
    eval("(list-set! lst 2 'c)");
    expectInt("(car lst)", 1);
    expectInt("(cadr lst)", 2);
    expectSymbol("(caddr lst)", "c");
}

TEST_F(R7RSListTest, ListSetMultiple) {
    eval("(define lst (list 'a 'b 'c))");
    eval("(list-set! lst 0 1)");
    eval("(list-set! lst 1 2)");
    eval("(list-set! lst 2 3)");
    expectInt("(car lst)", 1);
    expectInt("(cadr lst)", 2);
    expectInt("(caddr lst)", 3);
}

TEST_F(R7RSListTest, ListSetString) {
    eval("(define lst (make-list 3 0))");
    eval("(list-set! lst 0 \"first\")");
    eval("(list-set! lst 1 \"second\")");
    eval("(list-set! lst 2 \"third\")");
    expectString("(car lst)", "first");
    expectString("(cadr lst)", "second");
    expectString("(caddr lst)", "third");
}

TEST_F(R7RSListTest, ListSetList) {
    eval("(define lst (make-list 2 0))");
    eval("(list-set! lst 0 '(a b))");
    eval("(list-set! lst 1 '(c d))");
    expectSymbol("(car (car lst))", "a");
    expectSymbol("(cadr (car lst))", "b");
    expectSymbol("(car (cadr lst))", "c");
}

TEST_F(R7RSListTest, ListSetOutOfBoundsThrows) {
    eval("(define lst (list 1 2 3))");
    EXPECT_THROW(eval("(list-set! lst 5 'x)"), std::runtime_error);
}

TEST_F(R7RSListTest, ListSetNegativeIndexThrows) {
    eval("(define lst (list 1 2 3))");
    EXPECT_THROW(eval("(list-set! lst -1 'x)"), std::runtime_error);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(R7RSListTest, MakeListAndModify) {
    eval("(define lst (make-list 3 0))");
    eval("(list-set! lst 0 10)");
    eval("(list-set! lst 1 20)");
    eval("(list-set! lst 2 30)");
    expectInt("(car lst)", 10);
    expectInt("(cadr lst)", 20);
    expectInt("(caddr lst)", 30);
}

TEST_F(R7RSListTest, CopyAndModify) {
    eval("(define orig (list 1 2 3))");
    eval("(define copy (list-copy orig))");
    eval("(list-set! copy 0 99)");
    eval("(list-set! copy 1 88)");
    expectInt("(car orig)", 1);
    expectInt("(cadr orig)", 2);
    expectInt("(car copy)", 99);
    expectInt("(cadr copy)", 88);
}

TEST_F(R7RSListTest, MakeListCopyAndCompare) {
    eval("(define lst1 (make-list 3 'x))");
    eval("(define lst2 (list-copy lst1))");
    expectBool("(equal? lst1 lst2)", true);
    expectBool("(eq? lst1 lst2)", false);  // Different objects
}

TEST_F(R7RSListTest, ChainedOperations) {
    eval("(define result (list-copy (make-list 4 10)))");
    expectInt("(length result)", 4);
    expectInt("(car result)", 10);
    eval("(list-set! result 2 42)");
    expectInt("(caddr result)", 42);
}

TEST_F(R7RSListTest, MakeListWithComputation) {
    eval("(define lst (make-list (+ 2 1) (* 3 4)))");
    expectInt("(length lst)", 3);
    expectInt("(car lst)", 12);
}

TEST_F(R7RSListTest, ListSetWithComputation) {
    eval("(define lst (list 1 2 3))");
    eval("(list-set! lst (+ 1 1) (* 5 6))");
    expectInt("(caddr lst)", 30);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(R7RSListTest, MakeListSingleElement) {
    eval("(define lst (make-list 1 42))");
    expectInt("(length lst)", 1);
    expectInt("(car lst)", 42);
}

TEST_F(R7RSListTest, ListCopySingleElement) {
    eval("(define orig '(42))");
    eval("(define copy (list-copy orig))");
    expectInt("(car copy)", 42);
    expectNil("(cdr copy)");
}

TEST_F(R7RSListTest, ListSetSingleElement) {
    eval("(define lst (list 1))");
    eval("(list-set! lst 0 99)");
    expectInt("(car lst)", 99);
}

TEST_F(R7RSListTest, MakeListWithNilFill) {
    eval("(define lst (make-list 2 '()))");
    expectNil("(car lst)");
    expectNil("(cadr lst)");
}

TEST_F(R7RSListTest, ListCopyPreservesNil) {
    eval("(define orig '(() () ()))");
    eval("(define copy (list-copy orig))");
    expectNil("(car copy)");
    expectNil("(cadr copy)");
    expectNil("(caddr copy)");
}

// ============================================================================
// Practical Examples
// ============================================================================

TEST_F(R7RSListTest, InitializeArray) {
    eval(R"(
        (define (make-array n init-value)
          (make-list n init-value))
        (define arr (make-array 5 0))
    )");
    expectInt("(length arr)", 5);
    expectInt("(list-ref arr 0)", 0);
    expectInt("(list-ref arr 4)", 0);
}

TEST_F(R7RSListTest, FillArray) {
    eval(R"(
        (define arr (make-list 3 0))
        (list-set! arr 0 10)
        (list-set! arr 1 20)
        (list-set! arr 2 30)
        (define sum (+ (car arr) (cadr arr) (caddr arr)))
    )");
    expectInt("sum", 60);
}

TEST_F(R7RSListTest, CloneAndModify) {
    eval(R"(
        (define original (list 'a 'b 'c))
        (define clone (list-copy original))
        (list-set! clone 1 'x)
    )");
    expectSymbol("(cadr original)", "b");
    expectSymbol("(cadr clone)", "x");
}
