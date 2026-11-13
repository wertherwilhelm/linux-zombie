#include "pch.h"
#include "TestFixture.h"
#include "toy_vm.h"

// ============================================================================
// R5RS Hygienic Macro Tests (define-syntax / syntax-rules)
// ============================================================================

// ----------------------------------------------------------------------------
// Basic Macro Tests
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, R5RSMacroSimpleWhen) {
    eval(R"(
        (define-syntax when
          (syntax-rules ()
            ((when test body)
             (if test body))))
    )");

    expectInt("(when (> 5 3) 42)", 42);
    // Note: when with false condition returns unspecified value, can't test reliably
}

TEST_F(ZombieSchemeTest, R5RSMacroSimpleUnless) {
    eval(R"(
        (define-syntax unless
          (syntax-rules ()
            ((unless test body)
             (if (not test) body))))
    )");

    expectInt("(unless (< 5 3) 42)", 42);
    // Note: unless with false condition returns unspecified value, can't test reliably
}

// ----------------------------------------------------------------------------
// Ellipsis Tests - Simple Patterns
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, R5RSMacroEllipsisSimple) {
    // Test simple ellipsis: (x ...)
    // Template (list (quote x) ...) should expand to (list (quote a) (quote b) (quote c))
    eval(R"(
        (define-syntax my-list
          (syntax-rules ()
            ((my-list x ...)
             (list (quote x) ...))))
    )");

    expectSymbol("(car (my-list a b c))", "a");
    expectSymbol("(cadr (my-list a b c))", "b");
    expectSymbol("(caddr (my-list a b c))", "c");
}

TEST_F(ZombieSchemeTest, R5RSMacroEllipsisMultipleExpressions) {
    // Test ellipsis with multiple body expressions
    eval(R"(
        (define-syntax when-multi
          (syntax-rules ()
            ((when-multi test body ...)
             (if test (begin body ...)))))
    )");

    eval("(define result 0)");
    eval("(when-multi (> 5 3) (set! result 10) (set! result (+ result 5)))");
    expectInt("result", 15);
}

// ----------------------------------------------------------------------------
// Ellipsis Tests - Nested Patterns
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, R5RSMacroEllipsisNestedBindings) {
    // Test nested ellipsis pattern: ((var val) ...)
    // This is the classic let Macro pattern
    eval(R"(
        (define-syntax my-let
          (syntax-rules ()
            ((my-let ((var val) ...) body ...)
             ((lambda (var ...) body ...) val ...))))
    )");

    expectInt("(my-let ((x 1) (y 2)) (+ x y))", 3);
    expectInt("(my-let ((a 10) (b 20) (c 30)) (+ a b c))", 60);
}

TEST_F(ZombieSchemeTest, R5RSMacroEllipsisNestedEmpty) {
    // Test nested ellipsis with empty list
    eval(R"(
        (define-syntax my-let
          (syntax-rules ()
            ((my-let ((var val) ...) body ...)
             ((lambda (var ...) body ...) val ...))))
    )");

    expectInt("(my-let () 42)", 42);
}

// ----------------------------------------------------------------------------
// Recursive Macro Tests
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, R5RSMacroRecursiveAnd) {
    // Test recursive Macro expansion with proper #t and #f
    eval(R"(
        (define-syntax my-and
          (syntax-rules ()
            ((my-and) #t)
            ((my-and test) test)
            ((my-and test1 test2 ...)
             (if test1 (my-and test2 ...) #f))))
    )");

    expectInt("(if (my-and (> 5 3) (> 7 4) (> 9 8)) 1 0)", 1);
    expectInt("(if (my-and (> 5 3) (< 7 4) (> 9 8)) 1 0)", 0);
    expectInt("(if (my-and) 1 0)", 1);
}

TEST_F(ZombieSchemeTest, R5RSMacroRecursiveOr) {
    // Test recursive Macro expansion with proper #t and #f
    eval(R"(
        (define-syntax my-or
          (syntax-rules ()
            ((my-or) #f)
            ((my-or test) test)
            ((my-or test1 test2 ...)
             (if test1 #t (my-or test2 ...)))))
    )");

    expectInt("(if (my-or (< 5 3) (< 7 4) (> 9 8)) 1 0)", 1);
    expectInt("(if (my-or (< 5 3) (< 7 4) (< 9 8)) 1 0)", 0);
}

// ----------------------------------------------------------------------------
// Literal Identifier Tests (else, =>)
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, R5RSMacroLiteralsElse) {
    // Test literal identifiers with 'else'
    eval(R"(
        (define-syntax my-cond
          (syntax-rules (else)
            ((my-cond (else body ...))
             (begin body ...))
            ((my-cond (test body ...))
             (if test (begin body ...)))
            ((my-cond (test body ...) clause ...)
             (if test (begin body ...) (my-cond clause ...)))))
    )");

    expectInt("(my-cond ((< 1 2) 10) (else 20))", 10);
    expectInt("(my-cond ((> 1 2) 10) (else 20))", 20);
}

// ----------------------------------------------------------------------------
// Hygiene Tests
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, R5RSMacroHygieneSimple) {
    // Test that Macro-introduced variables don't capture user variables
    eval(R"(
        (define-syntax swap
          (syntax-rules ()
            ((swap a b)
             (let ((temp a))
               (set! a b)
               (set! b temp)))))
    )");

    eval("(define x 1)");
    eval("(define y 2)");
    eval("(define temp 999)");  // This should NOT be captured by Macro

    eval("(swap x y)");

    expectInt("x", 2);
    expectInt("y", 1);
    expectInt("temp", 999);  // Should remain unchanged (hygiene)
}

TEST_F(ZombieSchemeTest, R5RSMacroHygieneNested) {
    // Test hygiene with nested Macro calls
    eval(R"(
        (define-syntax my-inc
          (syntax-rules ()
            ((my-inc x)
             (let ((temp x))
               (+ temp 1)))))
    )");

    expectInt("(let ((temp 5)) (my-inc temp))", 6);
}

// ----------------------------------------------------------------------------
// Complex Pattern Matching Tests
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, R5RSMacroMultiplePatterns) {
    // Test Macro with multiple pattern rules
    eval(R"(
        (define-syntax my-if
          (syntax-rules ()
            ((my-if test then)
             (if test then))
            ((my-if test then else)
             (if test then else))))
    )");

    expectInt("(my-if (> 5 3) 42)", 42);
    expectInt("(my-if (< 5 3) 42 99)", 99);
}

TEST_F(ZombieSchemeTest, R5RSMacroPatternMatching) {
    // Test pattern matching with different structures
    eval(R"(
        (define-syntax my-case
          (syntax-rules ()
            ((my-case value)
             '())
            ((my-case value (pattern result))
             result)))
    )");

    expectInt("(my-case 5 (x 42))", 42);
}

// ----------------------------------------------------------------------------
// Real-world R5RS Macro Examples
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, R5RSMacroLetStar) {
    // Implement let* using macros
    eval(R"(
        (define-syntax my-let*
          (syntax-rules ()
            ((my-let* () body ...)
             (begin body ...))
            ((my-let* ((var val) rest ...) body ...)
             (let ((var val))
               (my-let* (rest ...) body ...)))))
    )");

    expectInt("(my-let* ((x 1) (y (+ x 1)) (z (+ y 1))) z)", 3);
}

TEST_F(ZombieSchemeTest, R5RSMacroBeginSequence) {
    // Test begin-like Macro with ellipsis
    eval(R"(
        (define-syntax my-begin
          (syntax-rules ()
            ((my-begin expr ...)
             (begin expr ...))))
    )");

    eval("(define counter 0)");
    eval("(my-begin (set! counter 1) (set! counter (+ counter 1)) (set! counter (+ counter 1)))");
    expectInt("counter", 3);
}

// ----------------------------------------------------------------------------
// Error Handling Tests
// ----------------------------------------------------------------------------

TEST_F(ZombieSchemeTest, R5RSMacroNoMatchingPattern) {
    // Test behavior when no pattern matches
    eval(R"(
        (define-syntax strict-pair
          (syntax-rules ()
            ((strict-pair a b)
             (cons a b))))
    )");

    // This should work
    expectInt("(car (strict-pair 1 2))", 1);

    // TODO: Test what happens with wrong number of arguments
    // This should ideally give an error, but for now we just test success cases
}

// ============================================================================
// toy_vm Tests - Verify stack-based VM supports new features
// ============================================================================

TEST_F(ZombieSchemeTest, ToyVMLetSyntaxBasic) {
    // Test let-syntax in toy_vm (stack-based evaluator with call/cc support)
    Environment* test_env = gc->allocate<Environment>();
    zombie::runtime::Primitive::install_primitives(test_env);
    scheme_reader test_reader;

    // Helper to eval with toy_vm
    auto eval_vm = [&](const std::string& code) -> Object* {
        Object* expr = test_reader.read_from_string(code);
        toy::VM vm(test_env, false);
        vm.eval(expr, test_env);
        return vm.accumulator;
    };

    Object* result = eval_vm(R"(
        (let-syntax ((when (syntax-rules ()
                             ((when test stmt1 stmt2 ...)
                              (if test
                                  (begin stmt1 stmt2 ...))))))
          (let ((x 10))
            (when (> x 5)
              x)))
    )");

    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 10);
}

TEST_F(ZombieSchemeTest, ToyVMLetrecSyntaxBasic) {
    // Test letrec-syntax in toy_vm
    Environment* test_env = gc->allocate<Environment>();
    zombie::runtime::Primitive::install_primitives(test_env);
    scheme_reader test_reader;

    auto eval_vm = [&](const std::string& code) -> Object* {
        Object* expr = test_reader.read_from_string(code);
        toy::VM vm(test_env, false);
        vm.eval(expr, test_env);
        return vm.accumulator;
    };

    Object* result = eval_vm(R"(
        (letrec-syntax
          ((my-or (syntax-rules ()
                    ((my-or) #f)
                    ((my-or e) e)
                    ((my-or e1 e2 ...)
                     (let ((temp e1))
                       (if temp temp (my-or e2 ...)))))))
          (my-or #f #f #t))
    )");

    EXPECT_EQ(result, t);
}
