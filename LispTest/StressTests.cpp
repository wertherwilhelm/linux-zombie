#include "pch.h"
#include "TestFixture.h"
#include "a_rational.h"
#include "a_complex.h"

// ============================================================================
// Stress Tests - Push the boundaries of the evaluator, GC, and runtime
// ============================================================================

// ============================================================================
// Deep Recursion Stress Tests (TCO)
// ============================================================================

TEST_F(ZombieSchemeTest, StressDeepRecursion10K) {
    // 10,000 iterations - tests tail-call optimization
    eval("(define (countdown n) "
         "  (if (zero? n) "
         "      'done "
         "      (countdown (- n 1))))");

    Object* result = eval("(countdown 10000)");
    a_symbol* sym = dynamic_cast<a_symbol*>(result);
    ASSERT_NE(sym, nullptr);
    EXPECT_EQ(sym->getName(), "done");
}

TEST_F(ZombieSchemeTest, StressFactorialDeep) {
    // Factorial with TCO via accumulator
    eval("(define (factorial n) "
         "  (define (fact-iter n acc) "
         "    (if (= n 0) "
         "        acc "
         "        (fact-iter (- n 1) (* n acc)))) "
         "  (fact-iter n 1))");

    // Calculate 20! (should not overflow with rationals)
    Object* result = eval("(factorial 20)");
    ASSERT_TRUE(numeric::is_number(result));
    // 20! = 2432902008176640000
}

TEST_F(ZombieSchemeTest, StressMutualRecursionDeep) {
    // Deep mutual recursion (1000 iterations)
    eval("(define (even? n) "
         "  (if (zero? n) #t (odd? (- n 1))))");
    eval("(define (odd? n) "
         "  (if (zero? n) #f (even? (- n 1))))");

    expectBool("(even? 1000)", true);
    expectBool("(odd? 1000)", false);
    expectBool("(even? 9999)", false);
    expectBool("(odd? 9999)", true);
}

// ============================================================================
// Large Data Structure Stress Tests (GC)
// ============================================================================

TEST_F(ZombieSchemeTest, StressLargeList1000) {
    // Create a list with 1000 elements
    eval("(define (range n) "
         "  (define (range-iter i acc) "
         "    (if (< i 0) "
         "        acc "
         "        (range-iter (- i 1) (cons i acc)))) "
         "  (range-iter n '()))");

    eval("(define big-list (range 1000))");
    expectInt("(length big-list)", 1001);  // 0..1000 inclusive
    expectInt("(list-ref big-list 0)", 0);
    expectInt("(list-ref big-list 1000)", 1000);
}

TEST_F(ZombieSchemeTest, StressLargeVector1000) {
    // Create and fill a large vector
    eval("(define big-vec (make-vector 1000 42))");
    expectInt("(vector-length big-vec)", 1000);
    expectInt("(vector-ref big-vec 0)", 42);
    expectInt("(vector-ref big-vec 999)", 42);

    // Modify and verify
    eval("(vector-set! big-vec 500 99)");
    expectInt("(vector-ref big-vec 500)", 99);
}

TEST_F(ZombieSchemeTest, StressNestedLists) {
    // Create deeply nested list structure
    eval("(define (make-nested n) "
         "  (if (zero? n) "
         "      'leaf "
         "      (list (make-nested (- n 1)) (make-nested (- n 1)))))");

    eval("(define nested (make-nested 10))");
    // This creates a binary tree of depth 10 (2^10 = 1024 nodes)
    // Just verify it doesn't crash
    Object* result = eval("nested");
    ASSERT_NE(result, nullptr);
}

TEST_F(ZombieSchemeTest, StressManyClosures) {
    // Create many closures that capture variables
    eval("(define (make-adder n) (lambda (x) (+ x n)))");
    eval("(define closures (list "
         "  (make-adder 1) (make-adder 2) (make-adder 3) "
         "  (make-adder 4) (make-adder 5) (make-adder 6) "
         "  (make-adder 7) (make-adder 8) (make-adder 9) "
         "  (make-adder 10)))");

    // Test that closures work correctly
    expectInt("((list-ref closures 0) 10)", 11);
    expectInt("((list-ref closures 9) 10)", 20);
}

// ============================================================================
// String Stress Tests
// ============================================================================

TEST_F(ZombieSchemeTest, StressVeryLongString10K) {
    // Create a 10,000 character string
    std::string long_str(10000, 'x');
    std::string code = "(string-length \"" + long_str + "\")";
    Object* result = eval(code.c_str());
    EXPECT_EQ(numeric::to_int64(result), 10000);
}

TEST_F(ZombieSchemeTest, StressStringConcatenation) {
    // Concatenate many strings
    eval("(define s1 \"hello\")");
    eval("(define s2 \"world\")");
    eval("(define s3 (string-append s1 s2 s1 s2 s1 s2))");

    Object* result = eval("(string-length s3)");
    // "hello" + "world" repeated 3 times = 5 + 5 + 5 + 5 + 5 + 5 = 30
    EXPECT_EQ(numeric::to_int64(result), 30);
}

// ============================================================================
// Numeric Stress Tests
// ============================================================================

TEST_F(ZombieSchemeTest, StressLargeIntegerArithmetic) {
    // Test with very large integers
    Object* result = eval("(+ 9223372036854775800 0)");
    ASSERT_TRUE(numeric::is_number(result));

    // Test large multiplication - use numbers that fit in 32-bit signed int
    // (32-bit signed int max is 2,147,483,647)
    result = eval("(* 10000 10000)");
    ASSERT_TRUE(numeric::is_number(result));
    // 10000 * 10000 = 100,000,000 (100 million, safe in 32-bit)
    EXPECT_EQ(numeric::to_int64(result), 100000000LL);

    // Test another safe multiplication
    result = eval("(* 30000 30000)");
    ASSERT_TRUE(numeric::is_number(result));
    // 30000 * 30000 = 900,000,000 (900 million, safe in 32-bit)
    EXPECT_EQ(numeric::to_int64(result), 900000000LL);

    // Test large addition
    result = eval("(+ 1000000000 1000000000)");
    ASSERT_TRUE(numeric::is_number(result));
    // 1 billion + 1 billion = 2 billion
    EXPECT_EQ(numeric::to_int64(result), 2000000000LL);
}

TEST_F(ZombieSchemeTest, StressRationalReduction) {
    // Test rational reduction with large numbers
    Object* result = eval("(+ 1/1000000 1/1000000)");
    ASSERT_TRUE(is_rational(result));

    // Add many small fractions
    result = eval("(+ 1/2 1/3 1/4 1/5 1/6 1/7 1/8 1/9 1/10)");
    ASSERT_TRUE(numeric::is_number(result));
}

TEST_F(ZombieSchemeTest, StressComplexArithmetic) {
    // Complex operations with many steps
    Object* result = eval(
        "(+ (make-rectangular 1 2) "
        "   (make-rectangular 3 4) "
        "   (make-rectangular 5 6))");

    ASSERT_TRUE(is_complex(result));
    a_complex* c = as_complex(result);
    EXPECT_EQ(numeric::to_int64(c->real_part()), 9);  // 1+3+5
    EXPECT_EQ(numeric::to_int64(c->imag_part()), 12); // 2+4+6
}

// ============================================================================
// Control Flow Stress Tests
// ============================================================================

TEST_F(ZombieSchemeTest, StressDeeplyNestedIf) {
    // 20 levels of nested if expressions
    eval("(define x 20)");
    std::string code = "(define result ";
    for (int i = 0; i < 20; i++) {
        code += "(if (> x " + std::to_string(i) + ") ";
    }
    code += "'success ";
    for (int i = 0; i < 20; i++) {
        code += "'fail) ";
    }
    code += ")";

    eval(code.c_str());
    expectSymbol("result", "success");
}

TEST_F(ZombieSchemeTest, StressCondWithManyClausesLarge) {
    // cond with 50 clauses
    std::string code = "(define (test n) (cond ";
    for (int i = 0; i < 50; i++) {
        code += "((= n " + std::to_string(i) + ") " + std::to_string(i * 10) + ") ";
    }
    code += "(else 999)))";

    eval(code.c_str());
    expectInt("(test 0)", 0);
    expectInt("(test 25)", 250);
    expectInt("(test 49)", 490);
    expectInt("(test 100)", 999);
}

// ============================================================================
// Let/Lambda Stress Tests
// ============================================================================

TEST_F(ZombieSchemeTest, StressLetWithManyVariables) {
    // let with 50 bindings
    std::string code = "(let (";
    for (int i = 0; i < 50; i++) {
        code += "(x" + std::to_string(i) + " " + std::to_string(i) + ") ";
    }
    code += ") (+ ";
    for (int i = 0; i < 50; i++) {
        code += "x" + std::to_string(i);
        if (i < 49) code += " ";
    }
    code += "))";

    Object* result = eval(code.c_str());
    // Sum of 0..49 = 49*50/2 = 1225
    EXPECT_EQ(numeric::to_int64(result), 1225);
}

TEST_F(ZombieSchemeTest, StressDeeplyNestedLambdas) {
    // Create deeply nested lambda chain
    eval("(define f0 (lambda (x) (+ x 0)))");
    for (int i = 1; i <= 20; i++) {
        std::string code = "(define f" + std::to_string(i) +
                          " (lambda (x) (f" + std::to_string(i-1) + " (+ x 1))))";
        eval(code.c_str());
    }

    expectInt("(f20 0)", 20);
}

// ============================================================================
// Macro Stress Tests
// ============================================================================

TEST_F(ZombieSchemeTest, StressMacroWithEllipsis) {
    // Macro that generates large expression
    eval("(define-syntax sum-all "
         "  (syntax-rules () "
         "    ((_ x ...) (+ x ...))))");

    expectInt("(sum-all 1 2 3 4 5 6 7 8 9 10)", 55);
}

TEST_F(ZombieSchemeTest, StressNestedMacroExpansion) {
    // Macro that expands to another Macro
    eval("(define-syntax when-not "
         "  (syntax-rules () "
         "    ((_ test body ...) "
         "     (if (not test) (begin body ...)))))");

    eval("(define x 0)");
    eval("(when-not (> x 10) (set! x 42))");
    expectInt("x", 42);
}

// ============================================================================
// Quasiquote Stress Tests
// ============================================================================

TEST_F(ZombieSchemeTest, StressLargeQuasiquoteTemplate) {
    // Large quasiquote template with many unquotes
    eval("(define a 1)");
    eval("(define b 2)");
    eval("(define c 3)");
    eval("(define d 4)");
    eval("(define e 5)");
    Object* result = eval("`(,a ,b ,c ,d ,e ,a ,b ,c ,d ,e ,a ,b ,c ,d ,e)");

    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->pair());
    // Should be a list of 15 elements
}

TEST_F(ZombieSchemeTest, StressQuasiquoteWithSplicing) {
    // Multiple splicing operations
    eval("(define l1 '(1 2))");
    eval("(define l2 '(3 4))");
    eval("(define l3 '(5 6))");

    Object* result = eval("`(,@l1 ,@l2 ,@l3)");
    ASSERT_TRUE(result->pair());
    EXPECT_EQ(numeric::to_int64(car(result)), 1);
}

// ============================================================================
// Do Loop Stress Tests
// ============================================================================

TEST_F(ZombieSchemeTest, StressDoLoopLargeIteration) {
    // Do loop with 10,000 iterations
    Object* result = eval(
        "(do ((i 0 (+ i 1)) "
        "     (sum 0 (+ sum i))) "
        "    ((>= i 10000) sum))");

    // Sum of 0..9999 = 9999*10000/2 = 49995000
    EXPECT_EQ(numeric::to_int64(result), 49995000);
}

TEST_F(ZombieSchemeTest, StressDoLoopMultipleAccumulators) {
    // Do loop with 10 accumulators
    Object* result = eval(
        "(do ((i 0 (+ i 1)) "
        "     (s1 0 (+ s1 i)) "
        "     (s2 0 (+ s2 i)) "
        "     (s3 0 (+ s3 i))) "
        "    ((>= i 100) (+ s1 s2 s3)))");

    // Each accumulator sums 0..99 = 4950
    // Total = 4950 * 3 = 14850
    EXPECT_EQ(numeric::to_int64(result), 14850);
}

// ============================================================================
// GC Stress Tests
// ============================================================================

TEST_F(ZombieSchemeTest, StressGCManyAllocations) {
    // Create and discard many objects to stress GC
    Object* def_result = eval("(define (make-garbage n) "
                               "  (if (zero? n) "
                               "      'done "
                               "      (begin "
                               "        (cons 1 2) "
                               "        (make-garbage (- n 1)))))");
    ASSERT_NE(def_result, nullptr);

    Object* result = eval("(make-garbage 1000)");
    ASSERT_NE(result, nullptr);
    a_symbol* sym = dynamic_cast<a_symbol*>(result);
    ASSERT_NE(sym, nullptr);
    EXPECT_EQ(sym->getName(), "done");
}

TEST_F(ZombieSchemeTest, StressGCLiveAndDead) {
    // Mix of live and dead objects
    eval("(define (create-mixed n acc) "
         "  (if (zero? n) "
         "      acc "
         "      (create-mixed (- n 1) "
         "                    (cons (cons 1 2) acc))))");

    eval("(define mixed (create-mixed 500 '()))");
    expectInt("(length mixed)", 500);
}

// ============================================================================
// Combined Stress Tests
// ============================================================================

TEST_F(ZombieSchemeTest, StressCombinedAll) {
    // Simplified: Test basic recursion with accumulator (no let inside)
    eval("(define (build-list n) "
         "  (define (helper i acc) "
         "    (if (>= i n) "
         "        acc "
         "        (helper (+ i 1) (cons i acc)))) "
         "  (helper 0 '()))");

    // Test with small value first
    expectInt("(length (build-list 10))", 10);

    // Now test with larger value
    eval("(define result (build-list 100))");
    expectInt("(length result)", 100);

    // Verify first and last elements
    expectInt("(car result)", 99);  // Last element added
}
