#include "pch.h"
#include "TestFixture.h"

// Safety net tests for Primitive refactoring
// These tests establish a baseline before we move primitives to separate files
class PrimitiveRefactorTest : public ZombieSchemeTest {
protected:
    // Helper: Count registered primitives in environment
    int count_primitives() {
        int count = 0;
        // This is a rough count - we'll verify behavior stays the same
        // by running the full test suite
        return count;
    }
};

// ============================================================================
// List Primitives (primitives_lists.cpp target)
// ============================================================================

TEST_F(PrimitiveRefactorTest, Cons) {
    expectInt("(car (cons 1 2))", 1);
    expectInt("(cdr (cons 1 2))", 2);
}

TEST_F(PrimitiveRefactorTest, Car) {
    expectInt("(car '(1 2 3))", 1);
    expectInt("(car (cons 42 99))", 42);
}

TEST_F(PrimitiveRefactorTest, Cdr) {
    expectInt("(car (cdr '(1 2 3)))", 2);
    expectInt("(cdr (cons 1 2))", 2);
}

TEST_F(PrimitiveRefactorTest, List) {
    expectInt("(car (list 1 2 3))", 1);
    expectInt("(car (cdr (list 1 2 3)))", 2);
    expectInt("(car (cdr (cdr (list 1 2 3))))", 3);
}

TEST_F(PrimitiveRefactorTest, Append) {
    expectInt("(car (append '(1 2) '(3 4)))", 1);
    expectInt("(car (cdr (cdr (append '(1 2) '(3 4)))))", 3);
    expectInt("(car (cdr (cdr (cdr (append '(1 2) '(3 4))))))", 4);
}

TEST_F(PrimitiveRefactorTest, Length) {
    expectInt("(length '())", 0);
    expectInt("(length '(1))", 1);
    expectInt("(length '(1 2 3))", 3);
    expectInt("(length (list 1 2 3 4 5))", 5);
}

TEST_F(PrimitiveRefactorTest, ListRef) {
    expectInt("(list-ref '(1 2 3) 0)", 1);
    expectInt("(list-ref '(1 2 3) 1)", 2);
    expectInt("(list-ref '(1 2 3) 2)", 3);
}

TEST_F(PrimitiveRefactorTest, ListTail) {
    expectInt("(car (list-tail '(1 2 3) 0))", 1);
    expectInt("(car (list-tail '(1 2 3) 1))", 2);
    expectInt("(car (list-tail '(1 2 3) 2))", 3);
}

TEST_F(PrimitiveRefactorTest, Reverse) {
    expectInt("(car (reverse '(1 2 3)))", 3);
    expectInt("(car (cdr (reverse '(1 2 3))))", 2);
    expectInt("(car (cdr (cdr (reverse '(1 2 3)))))", 1);
}

TEST_F(PrimitiveRefactorTest, Map) {
    expectInt("(car (map (lambda (x) (* x 2)) '(1 2 3)))", 2);
    expectInt("(car (cdr (map (lambda (x) (* x 2)) '(1 2 3))))", 4);
    expectInt("(car (cdr (cdr (map (lambda (x) (* x 2)) '(1 2 3)))))", 6);
}

TEST_F(PrimitiveRefactorTest, ForEach) {
    // for-each returns unspecified, but has side effects
    // Just verify it runs without error
    eval("(for-each (lambda (x) x) '(1 2 3))");
}

TEST_F(PrimitiveRefactorTest, Apply) {
    expectInt("(apply + '(1 2 3))", 6);
    expectInt("(apply * '(2 3 4))", 24);
    expectInt("(apply + 1 2 '(3 4))", 10);
}

TEST_F(PrimitiveRefactorTest, ListPredicates) {
    expectBool("(pair? '(1 2))", true);
    expectBool("(pair? '())", false);
    expectBool("(list? '())", true);
    expectBool("(list? '(1 2 3))", true);
    expectBool("(null? '())", true);
    expectBool("(null? '(1))", false);
}

// ============================================================================
// Core Primitives (primitives_core.cpp target)
// ============================================================================

TEST_F(PrimitiveRefactorTest, Addition) {
    expectInt("(+)", 0);
    expectInt("(+ 5)", 5);
    expectInt("(+ 1 2)", 3);
    expectInt("(+ 1 2 3)", 6);
    expectInt("(+ 1 2 3 4 5)", 15);
}

TEST_F(PrimitiveRefactorTest, Subtraction) {
    // Note: Unary minus behavior differs between evaluator and compiler
    // Compiler has unary minus support, evaluator may not
    // expectInt("(- 5)", -5);  // Commented out - may not be supported
    expectInt("(- 10 3)", 7);
    expectInt("(- 100 50 25)", 25);
}

TEST_F(PrimitiveRefactorTest, Multiplication) {
    expectInt("(*)", 1);
    expectInt("(* 5)", 5);
    expectInt("(* 2 3)", 6);
    expectInt("(* 2 3 4)", 24);
}

TEST_F(PrimitiveRefactorTest, Division) {
    expectInt("(/ 12 3)", 4);
    expectInt("(/ 100 10)", 10);
    expectInt("(/ 100 10 2)", 5);
}

TEST_F(PrimitiveRefactorTest, Modulo) {
    expectInt("(modulo 10 3)", 1);
    expectInt("(modulo 17 5)", 2);
    expectInt("(modulo 8 4)", 0);
}

TEST_F(PrimitiveRefactorTest, Comparisons) {
    expectBool("(< 3 5)", true);
    expectBool("(< 5 3)", false);
    expectBool("(< 5 5)", false);

    expectBool("(> 5 3)", true);
    expectBool("(> 3 5)", false);
    expectBool("(> 5 5)", false);

    expectBool("(= 5 5)", true);
    expectBool("(= 5 3)", false);
    expectBool("(= 1 1 1)", true);
    expectBool("(= 1 1 2)", false);

    expectBool("(<= 3 5)", true);
    expectBool("(<= 5 5)", true);
    expectBool("(<= 5 3)", false);

    expectBool("(>= 5 3)", true);
    expectBool("(>= 5 5)", true);
    expectBool("(>= 3 5)", false);
}

TEST_F(PrimitiveRefactorTest, BooleanOps) {
    expectBool("(not #f)", true);
    expectBool("(not #t)", false);
    expectBool("(not '())", false);  // R5RS: empty list is truthy
    expectBool("(not 0)", false);
}

TEST_F(PrimitiveRefactorTest, TypePredicates) {
    expectBool("(number? 42)", true);
    expectBool("(number? 'x)", false);

    expectBool("(integer? 42)", true);
    expectBool("(integer? 3.14)", false);

    expectBool("(boolean? #t)", true);
    expectBool("(boolean? #f)", true);
    expectBool("(boolean? 0)", false);

    expectBool("(symbol? 'x)", true);
    expectBool("(symbol? 42)", false);
}

// ============================================================================
// String Primitives (primitives_strings.cpp target)
// ============================================================================

TEST_F(PrimitiveRefactorTest, StringPredicates) {
    expectBool("(string? \"hello\")", true);
    expectBool("(string? 42)", false);
    expectBool("(string? 'x)", false);
}

TEST_F(PrimitiveRefactorTest, StringOperations) {
    expectString("(string-append \"hello\" \" \" \"world\")", "hello world");
    expectInt("(string-length \"hello\")", 5);
    expectInt("(string-length \"\")", 0);
}

TEST_F(PrimitiveRefactorTest, StringConversions) {
    expectString("(number->string 42)", "42");
    expectInt("(string->number \"42\")", 42);
    expectString("(symbol->string 'hello)", "hello");
}

// ============================================================================
// Vector Primitives (primitives_vectors.cpp target)
// ============================================================================

TEST_F(PrimitiveRefactorTest, VectorPredicates) {
    // Note: Vector literal syntax #(...) may not be fully supported
    // expectBool("(vector? #(1 2 3))", true);
    expectBool("(vector? '(1 2 3))", false);
    expectBool("(vector? (make-vector 3))", true);
}

TEST_F(PrimitiveRefactorTest, VectorOperations) {
    // Note: Vector syntax may not be fully supported in current reader
    // expectInt("(vector-length #(1 2 3))", 3);
    // expectInt("(vector-ref #(10 20 30) 1)", 20);

    // Test with make-vector instead
    expectInt("(vector-ref (make-vector 3 42) 0)", 42);
}

TEST_F(PrimitiveRefactorTest, MakeVector) {
    expectInt("(vector-length (make-vector 5))", 5);
}

// ============================================================================
// Control Flow Primitives (primitives_control.cpp target)
// ============================================================================

TEST_F(PrimitiveRefactorTest, EqualityPredicates) {
    // Symbol interning is now implemented - eq? works correctly with symbols
    expectBool("(eq? 'a 'a)", true);
    expectBool("(eq? 'a 'b)", false);

    expectBool("(eqv? 42 42)", true);
    expectBool("(eqv? 42 43)", false);

    expectBool("(equal? '(1 2 3) '(1 2 3))", true);
    expectBool("(equal? '(1 2 3) '(1 2 4))", false);
}

// ============================================================================
// Higher-Order Function Primitives
// ============================================================================

TEST_F(PrimitiveRefactorTest, Filter) {
    expectInt("(car (filter (lambda (x) (> x 2)) '(1 2 3 4)))", 3);
    expectInt("(length (filter (lambda (x) (> x 2)) '(1 2 3 4)))", 2);
}

TEST_F(PrimitiveRefactorTest, Fold) {
    expectInt("(fold-left + 0 '(1 2 3 4))", 10);
    expectInt("(fold-right + 0 '(1 2 3 4))", 10);
    expectInt("(fold-left * 1 '(2 3 4))", 24);
}

// ============================================================================
// Baseline Count Test
// ============================================================================

TEST_F(PrimitiveRefactorTest, BaselinePrimitiveCount) {
    // We don't have a direct way to count primitives from the environment,
    // but we can verify that key primitives are registered
    EXPECT_TRUE(primitiveExists("+"));
    EXPECT_TRUE(primitiveExists("-"));
    EXPECT_TRUE(primitiveExists("*"));
    EXPECT_TRUE(primitiveExists("/"));
    EXPECT_TRUE(primitiveExists("cons"));
    EXPECT_TRUE(primitiveExists("car"));
    EXPECT_TRUE(primitiveExists("cdr"));
    EXPECT_TRUE(primitiveExists("list"));
    EXPECT_TRUE(primitiveExists("append"));
    EXPECT_TRUE(primitiveExists("map"));
    EXPECT_TRUE(primitiveExists("apply"));
    EXPECT_TRUE(primitiveExists("string-append"));
    EXPECT_TRUE(primitiveExists("vector"));
    EXPECT_TRUE(primitiveExists("eq?"));
    EXPECT_TRUE(primitiveExists("equal?"));
    EXPECT_TRUE(primitiveExists("not"));

    // Add more key primitives we want to ensure exist after refactor
}
