#include "pch.h"
#include "TestFixture.h"
#include "a_rational.h"
#include "a_complex.h"

// ============================================================================
// Edge Case Tests - Error Handling and Boundary Conditions
// ============================================================================

// ============================================================================
// Arithmetic Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, DivisionByZero) {
    // Division by zero throws exception in Zombie Scheme
    EXPECT_THROW({
        eval("(/ 1 0)");
    }, std::exception);
}

TEST_F(ZombieSchemeTest, ZeroDividedByZero) {
    // 0/0 throws exception in Zombie Scheme
    EXPECT_THROW({
        eval("(/ 0 0)");
    }, std::exception);
}

TEST_F(ZombieSchemeTest, NegativeZero) {
    // Test negative zero handling
    Object* result = eval("(- 0.0)");
    EXPECT_TRUE(numeric::is_number(result));
}

TEST_F(ZombieSchemeTest, VeryLargeInteger) {
    // Test with large integer
    Object* result = eval("(+ 9223372036854775800 7)");
    EXPECT_TRUE(numeric::is_number(result));
}

TEST_F(ZombieSchemeTest, VerySmallDouble) {
    // Test with very small double
    Object* result = eval("(* 0.0000000001 0.0000000001)");
    EXPECT_TRUE(numeric::is_number(result));
}

// ============================================================================
// List Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, EmptyListOperations) {
    // Operations on empty lists
    expectBool("(null? '())", true);
    expectInt("(length '())", 0);

    // append empty lists
    Object* result = eval("(append '() '() '())");
    EXPECT_TRUE(::is_nil(result) || result->is_nil());
}

TEST_F(ZombieSchemeTest, DeepNestedList) {
    // Very deeply nested list
    eval("(define deep '((((((1)))))))");
    Object* result = eval("(car (car (car (car (car (car deep))))))");
    EXPECT_EQ(numeric::to_int64(result), 1);
}

TEST_F(ZombieSchemeTest, LongList) {
    // Test with a long list (100 elements)
    eval("(define long-list (list 1 2 3 4 5 6 7 8 9 10 "
         "11 12 13 14 15 16 17 18 19 20 "
         "21 22 23 24 25 26 27 28 29 30 "
         "31 32 33 34 35 36 37 38 39 40 "
         "41 42 43 44 45 46 47 48 49 50))");

    Object* result = eval("(length long-list)");
    EXPECT_EQ(numeric::to_int64(result), 50);

    result = eval("(list-ref long-list 49)");
    EXPECT_EQ(numeric::to_int64(result), 50);
}

TEST_F(ZombieSchemeTest, CircularListDetection) {
    // Test that we can handle circular references
    eval("(define x (cons 1 2))");
    eval("(set-cdr! x x)");

    // Accessing car should still work
    Object* result = eval("(car x)");
    EXPECT_EQ(numeric::to_int64(result), 1);
}

// ============================================================================
// String Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, EmptyString) {
    // Empty string operations
    expectString("\"\"", "");
    expectInt("(string-length \"\")", 0);
}

TEST_F(ZombieSchemeTest, StringWithEscapes) {
    // String with escape characters
    expectString("\"hello\\nworld\"", "hello\nworld");
}

TEST_F(ZombieSchemeTest, StringEdgeCaseVeryLong) {
    // Test with a long string (1000 characters)
    std::string long_str(1000, 'x');
    std::string code = "(string-length \"" + long_str + "\")";
    Object* result = eval(code.c_str());
    EXPECT_EQ(numeric::to_int64(result), 1000);
}

// ============================================================================
// Vector Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, EmptyVector) {
    // Empty vector operations
    eval("(define v (vector))");
    expectInt("(vector-length v)", 0);
}

TEST_F(ZombieSchemeTest, LargeVector) {
    // Vector with many elements
    eval("(define big-vec (make-vector 100 42))");
    expectInt("(vector-length big-vec)", 100);
    expectInt("(vector-ref big-vec 50)", 42);
}

TEST_F(ZombieSchemeTest, VectorFillEdgeCase) {
    // Fill vector and verify all elements
    eval("(define v (make-vector 10 0))");
    eval("(vector-fill! v 99)");
    expectInt("(vector-ref v 0)", 99);
    expectInt("(vector-ref v 9)", 99);
}

// ============================================================================
// Lambda and Closure Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, ZeroArgumentLambda) {
    // Lambda with no arguments
    eval("(define f (lambda () 42))");
    expectInt("(f)", 42);
}

TEST_F(ZombieSchemeTest, ManyArgumentLambda) {
    // Lambda with many arguments
    eval("(define f (lambda (a b c d e f g h i j) (+ a b c d e f g h i j)))");
    expectInt("(f 1 2 3 4 5 6 7 8 9 10)", 55);
}

TEST_F(ZombieSchemeTest, ClosureEdgeCaseDeeplyNested) {
    // Deeply nested closures
    eval("(define f (lambda (x) (lambda (y) (lambda (z) (+ x y z)))))");
    expectInt("(((f 1) 2) 3)", 6);
}

TEST_F(ZombieSchemeTest, ClosureCapturesMultipleVariables) {
    // Closure capturing many variables
    eval("(define (make-counter a b c d e)"
         "  (lambda () (+ a b c d e)))");
    eval("(define cnt (make-counter 1 2 3 4 5))");
    expectInt("(cnt)", 15);
}

// ============================================================================
// Recursion Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, DeepRecursion) {
    // Test deep recursion (TCO should prevent stack overflow)
    eval("(define (countdown n) "
         "  (if (zero? n) "
         "      'done "
         "      (countdown (- n 1))))");

    Object* result = eval("(countdown 1000)");
    a_symbol* sym = dynamic_cast<a_symbol*>(result);
    ASSERT_NE(sym, nullptr);
    EXPECT_EQ(sym->getName(), "done");
}

TEST_F(ZombieSchemeTest, MutualRecursion) {
    // Test mutual recursion
    eval("(define (even? n) "
         "  (if (zero? n) #t (odd? (- n 1))))");
    eval("(define (odd? n) "
         "  (if (zero? n) #f (even? (- n 1))))");

    expectBool("(even? 100)", true);
    expectBool("(odd? 100)", false);
    expectBool("(even? 99)", false);
    expectBool("(odd? 99)", true);
}

// ============================================================================
// Conditional Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, NestedIf) {
    // Deeply nested if expressions
    eval("(define x 5)");
    Object* result = eval(
        "(if (> x 0) "
        "  (if (> x 1) "
        "    (if (> x 2) "
        "      (if (> x 3) "
        "        (if (> x 4) 'deep #f) "
        "        #f) "
        "      #f) "
        "    #f) "
        "  #f)");

    a_symbol* sym = dynamic_cast<a_symbol*>(result);
    ASSERT_NE(sym, nullptr);
    EXPECT_EQ(sym->getName(), "deep");
}

TEST_F(ZombieSchemeTest, CondWithManyClausesEdgeCase) {
    // cond with many clauses
    eval("(define x 25)");
    Object* result = eval(
        "(cond "
        "  ((< x 10) 'small) "
        "  ((< x 20) 'medium) "
        "  ((< x 30) 'large) "
        "  ((< x 40) 'xlarge) "
        "  (else 'huge))");

    a_symbol* sym = dynamic_cast<a_symbol*>(result);
    ASSERT_NE(sym, nullptr);
    EXPECT_EQ(sym->getName(), "large");
}

// ============================================================================
// Let Forms Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, LetWithNoBindings) {
    // let with no bindings
    expectInt("(let () 42)", 42);
}

TEST_F(ZombieSchemeTest, LetWithManyBindings) {
    // let with many bindings
    Object* result = eval(
        "(let ((a 1) (b 2) (c 3) (d 4) (e 5) "
        "      (f 6) (g 7) (h 8) (i 9) (j 10)) "
        "  (+ a b c d e f g h i j))");
    EXPECT_EQ(numeric::to_int64(result), 55);
}

TEST_F(ZombieSchemeTest, DeeplyNestedLet) {
    // Deeply nested let
    Object* result = eval(
        "(let ((a 1)) "
        "  (let ((b 2)) "
        "    (let ((c 3)) "
        "      (let ((d 4)) "
        "        (let ((e 5)) "
        "          (+ a b c d e))))))");
    EXPECT_EQ(numeric::to_int64(result), 15);
}

// ============================================================================
// Macro Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, MacroWithNoArgs) {
    // Macro with no pattern arguments
    eval("(define-syntax always-42 "
         "  (syntax-rules () "
         "    ((_) 42)))");

    expectInt("(always-42)", 42);
}

TEST_F(ZombieSchemeTest, MacroWithEllipsis) {
    // Macro using ellipsis (...)
    eval("(define-syntax sum "
         "  (syntax-rules () "
         "    ((_ x ...) (+ x ...))))");

    expectInt("(sum 1 2 3 4 5)", 15);
}

// ============================================================================
// Numeric Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, RationalEdgeCases) {
    // Rational edge cases
    expectBool("(= 1/1 1)", true);
    expectBool("(= 0/1 0)", true);

    // Very large numerator/denominator
    Object* result = eval("(+ 1/1000000 1/1000000)");
    EXPECT_TRUE(is_rational(result));
}

TEST_F(ZombieSchemeTest, ComplexEdgeCases) {
    // Complex with very small imaginary part
    eval("(define c (make-rectangular 5 0.0000001))");
    EXPECT_TRUE(is_complex(eval("c")));
}

// ============================================================================
// Symbol and Identifier Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, SymbolWithSpecialChars) {
    // Symbols with special characters
    eval("(define +special-symbol+ 42)");
    expectInt("+special-symbol+", 42);
}

TEST_F(ZombieSchemeTest, SymbolsWithDashes) {
    // Symbols with dashes
    eval("(define my-var-name 99)");
    expectInt("my-var-name", 99);
}

// ============================================================================
// Quasiquote Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, QuasiquoteDeepNesting) {
    // Deeply nested quasiquote
    eval("(define x 5)");
    eval("(define y 10)");
    Object* result = eval("`(a (b (c ,x (d ,y))))");

    EXPECT_TRUE(result->pair());
}

TEST_F(ZombieSchemeTest, QuasiquoteWithMultipleSplicing) {
    // Multiple unquote-splicing in one expression
    eval("(define a '(1 2))");
    eval("(define b '(3 4))");
    eval("(define c '(5 6))");
    Object* result = eval("`(,@a ,@b ,@c)");

    // Should be (1 2 3 4 5 6)
    EXPECT_TRUE(result->pair());
    EXPECT_EQ(numeric::to_int64(car(result)), 1);
}

// ============================================================================
// Do Loop Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, DoLoopNoIterations) {
    // Do loop that exits immediately
    Object* result = eval(
        "(do ((i 0 (+ i 1))) "
        "    ((>= i 0) 'done))");

    a_symbol* sym = dynamic_cast<a_symbol*>(result);
    ASSERT_NE(sym, nullptr);
    EXPECT_EQ(sym->getName(), "done");
}

TEST_F(ZombieSchemeTest, DoLoopManyIterations) {
    // Do loop with many iterations (tests TCO)
    Object* result = eval(
        "(do ((i 0 (+ i 1)) "
        "     (sum 0 (+ sum 1))) "
        "    ((>= i 1000) sum))");

    EXPECT_EQ(numeric::to_int64(result), 1000);
}

// ============================================================================
// Continuation Edge Cases
// ============================================================================

// NOTE: The regular Lisp evaluator supports escape-only (upward, one-shot)
// continuations: invoking k unwinds back to its call/cc frame. Re-entrant
// continuations are tested in ContinuationTests.cpp on the toy VM.

TEST_F(ZombieSchemeTest, CallCCReturningImmediately) {
    // call/cc that returns immediately without using the Continuation
    Object* result = eval("(call/cc (lambda (k) 42))");
    EXPECT_EQ(numeric::to_int64(result), 42);
}

TEST_F(ZombieSchemeTest, CallCCNestedContinuations) {
    // Inner escape targets the OUTER continuation: the inner call/cc frame
    // must rethrow past itself
    Object* result = eval(
        "(call/cc (lambda (k1) "
        "  (+ 1 (call/cc (lambda (k2) "
        "    (k1 10))))))");

    EXPECT_EQ(numeric::to_int64(result), 10);
}

TEST_F(ZombieSchemeTest, CallCCEscapeSkipsRestOfBody) {
    // Everything after (k 10) is discarded; surrounding (+ 1 ...) still runs
    Object* result = eval("(+ 1 (call/cc (lambda (k) (k 10) 999)))");
    EXPECT_EQ(numeric::to_int64(result), 11);
}

TEST_F(ZombieSchemeTest, CallCCEscapeFromDeepRecursion) {
    // Early exit from a recursive walk — the canonical escape use case
    Object* result = eval(
        "(call/cc (lambda (return) "
        "  (letrec ((walk (lambda (lst) "
        "                   (cond ((null? lst) 'not-found) "
        "                         ((eq? (car lst) 'stop) (return 'found)) "
        "                         (else (walk (cdr lst))))))) "
        "    (walk '(a b stop c d)))))");

    a_symbol* sym = dynamic_cast<a_symbol*>(result);
    ASSERT_NE(sym, nullptr);
    EXPECT_EQ(sym->getName(), "found");
}

TEST_F(ZombieSchemeTest, CallCCEscapeRunsDynamicWindAfterThunk) {
    // Escaping through a dynamic-wind must run the after thunk
    eval("(define after-ran #f)");
    Object* result = eval(
        "(call/cc (lambda (k) "
        "  (dynamic-wind "
        "    (lambda () #f) "
        "    (lambda () (k 7)) "
        "    (lambda () (set! after-ran #t)))))");

    EXPECT_EQ(numeric::to_int64(result), 7);
    EXPECT_EQ(eval("after-ran"), t);
}

TEST_F(ZombieSchemeTest, CallCCInvokeOutsideExtentIsError) {
    // The recursive evaluator's continuations are one-shot escapes; using
    // one after its call/cc frame returned raises an error instead of
    // unwinding into a dead stack frame
    eval("(define saved-k #f)");
    eval("(call/cc (lambda (k) (set! saved-k k) 1))");
    EXPECT_THROW(eval("(saved-k 5)"), std::runtime_error);
}
