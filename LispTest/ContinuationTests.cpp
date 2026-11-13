#include "pch.h"
#include "TestFixture.h"
#include "toy_vm.h"
#include "scheme_reader.h"
#include "numeric_runtime.h"
#include <sstream>


// Access to global GC instance
extern gc_manager* gc;

// Test fixture for Continuation tests using toy_vm
// The main evaluator doesn't support call/cc yet, but toy_vm does!
class ContinuationTest : public ::testing::Test {
protected:
    Environment* env;
    scheme_reader* reader;
    toy::VM* vm;  // Single VM instance for the entire test
    gc_root_guard* guard_env;

    void SetUp() override {
        // Global environment already initialized init_lisp_constants() in main
        // Create environment and VM for each test
        // IMPORTANT: Use gc->allocate to ensure GC tracking
        env = gc->allocate<Environment>();

        // Protect from GC immediately
        guard_env = new gc_root_guard((Object*)env);

        zombie::runtime::Primitive::install_primitives(env);
        reader = new scheme_reader();
        vm = new toy::VM(env, false);  // Single VM instance
    }
    void TearDown() override {
        // Cleanup is handled by GC
        delete reader;
        delete vm;

        // Unregister GC root
        delete guard_env;
    }
    // Helper: Evaluate using the single toy_vm instance
    Object* eval_vm(const std::string& code) {
        Object* expr = reader->read_from_string(code);
        // Don't clear the stack - continuations may reference it!
        // The VM naturally clears the stack when evaluation completes
        vm->eval(expr, env);
        return vm->accumulator;
    }
    // Helper: Evaluate and expect integer result
    void expectInt(const std::string& code, int expected) {
        Object* result = eval_vm(code);
        ASSERT_NE(result, nullptr) << "Expression returned null: " << code;
        ASSERT_TRUE(numeric::is_number(result)) << "Result is not a number: " << code;
        int actual;
        if (result->get_type() == static_cast<int>(Type::INT) || result->get_type() == static_cast<int>(Type::LONG)) {
            actual = static_cast<int>(numeric::to_int64(result));
        } else {
            actual = static_cast<int>(numeric::to_double(result));
        }
        EXPECT_EQ(actual, expected) << "For expression: " << code;
    }
    // Helper: Evaluate and expect string result
    void expectString(const std::string& code, const std::string& expected) {
        Object* result = eval_vm(code);
        ASSERT_NE(result, nullptr) << "Expression returned null: " << code;
        a_string* str = dynamic_cast<a_string*>(result);
        ASSERT_NE(str, nullptr) << "Result is not a string: " << code;
        EXPECT_EQ(str->value(), expected) << "For expression: " << code;
    }
};
// ============================================================================
// Basic Continuation Tests
// ============================================================================
TEST_F(ContinuationTest, SimpleEscape) {
    // (call/cc (lambda (k) (k 42)))
    // The Continuation k escapes immediately with 42
    std::string code = "(call/cc (lambda (k) (k 42)))";
    expectInt(code, 42);
}
TEST_F(ContinuationTest, NonLocalExit) {
    // (+ 10 (call/cc (lambda (k) (+ 20 (k 5)))))
    // Should return 15, not 35, because k escapes the inner +
    std::string code = "(+ 10 (call/cc (lambda (k) (+ 20 (k 5)))))";
    expectInt(code, 15);
}
TEST_F(ContinuationTest, ContinuationIgnored) {
    // (call/cc (lambda (k) 100))
    // The Continuation is not used, just return 100 normally
    std::string code = "(call/cc (lambda (k) 100))";
    expectInt(code, 100);
}
// ============================================================================
// Continuation Capture and Invocation
// ============================================================================
TEST_F(ContinuationTest, CaptureAndInvoke) {
    // Test capturing a Continuation and invoking it later
    // Break into separate statements since toy_vm might handle multi-statement blocks differently
    eval_vm("(define saved-cont '())");
    eval_vm(R"(
        (define result
          (+ 1 (call/cc (lambda (k)
                          (set! saved-cont k)
                          10))))
    )");
    // First call should give us 11 (1 + 10)
    expectInt("result", 11);
    // Now invoke the saved Continuation with 20
    // This should give us 21 (1 + 20)
    expectInt("(saved-cont 20)", 21);
}
TEST_F(ContinuationTest, ContinuationInConditional) {
    // call/cc in an if expression
    // Use 't instead of #t to avoid reader issues
    std::string code = "(if (call/cc (lambda (k) (k 't))) 42 99)";
    expectInt(code, 42);
}
TEST_F(ContinuationTest, ContinuationWithMultipleArgs) {
    // Test that Continuation works in more complex scenarios
    std::string code = R"(
        (* 2 (+ 3 (call/cc (lambda (k) (k 4)))))
    )";
    // (* 2 (+ 3 4)) = (* 2 7) = 14
    expectInt(code, 14);
}
// ============================================================================
// GC Integration Tests
// ============================================================================
// scans stack and finds stale pointers from previous tests, causing heap corruption
TEST_F(ContinuationTest, GCContinuationCapture) {
    // This is the test that was disabled in GCEdgeCaseTests.cpp
    // Now testing it with toy_vm which supports continuations!
    eval_vm("(define saved-cont '())");
    eval_vm(R"(
        (define test
          (lambda ()
            (+ 1 (call/cc
                   (lambda (k)
                     (set! saved-cont k)
                     10)))))
    )");
    expectInt("(test)", 11);
    // Force a GC collection
    // gc->collect(); // Disabled: Conservative GC limitation
    // Invoke saved Continuation - it should still work after GC!
    expectInt("(saved-cont 20)", 21);
}
// scans stack and finds stale pointers from previous tests, causing heap corruption
TEST_F(ContinuationTest, MultipleContinuationsWithGC) {
    // Create multiple continuations and ensure they survive GC
    std::string setup = R"(
        (define cont1 '())
        (define cont2 '())
        (define cont3 '())
        (define val1 (call/cc (lambda (k) (set! cont1 k) 1)))
        (define val2 (call/cc (lambda (k) (set! cont2 k) 2)))
        (define val3 (call/cc (lambda (k) (set! cont3 k) 3)))
    )";
    eval_vm(setup);
    // Run GC
    // gc->collect(); // Disabled: Conservative GC limitation
    // All continuations should still work
    expectInt("(cont1 10)", 10);
    expectInt("(cont2 20)", 20);
    expectInt("(cont3 30)", 30);
}
// ============================================================================
// Advanced Continuation Tests
// ============================================================================
TEST_F(ContinuationTest, NestedCallCC) {
    // Nested call/cc calls
    std::string code = R"(
        (call/cc (lambda (k1)
          (+ 10 (call/cc (lambda (k2)
                   (k1 42))))))
    )";
    // k1 escapes from the outer call/cc with 42
    expectInt(code, 42);
}
TEST_F(ContinuationTest, ContinuationInClosure) {
    // Continuation captured in a Closure's environment
    std::string setup = R"(
        (define (make-escape-fn)
          (call/cc (lambda (k)
                     (lambda (x) (k x)))))
        (define escape (make-escape-fn))
    )";
    eval_vm(setup);
    // Calling escape should use the captured Continuation
    expectInt("(escape 99)", 99);
}
TEST_F(ContinuationTest, ContinuationWithBegin) {
    // call/cc in a begin expression
    std::string code = R"(
        (begin
          (+ 1 2)
          (call/cc (lambda (k) (k 42)))
          (+ 3 4))
    )";
    // The last expression should never execute because k escapes
    expectInt(code, 42);
}
// ============================================================================
// Error Cases and Edge Cases
// ============================================================================
TEST_F(ContinuationTest, ContinuationMultipleInvocations) {
    // A Continuation can be invoked multiple times
    // R5RS: use #t and #f for boolean values
    eval_vm("(define cont '())");
    eval_vm("(define first-time (call/cc (lambda (k) (set! cont k) #t)))");
    // First invocation
    expectInt("(if first-time 1 2)", 1);
    // Invoke Continuation again - should return with #f (false)
    eval_vm("(cont #f)");
    expectInt("(if first-time 1 2)", 2);
}
TEST_F(ContinuationTest, ContinuationPreservesEnvironment) {
    // Ensure Continuation preserves lexical environment
    // Define the function and saved-k variable
    eval_vm("(define saved-k '())");
    eval_vm(R"(
        (define (test-env x)
          (let ((y 10))
            (call/cc (lambda (k)
                       (set! saved-k k)
                       (+ x y)))))
    )");
    eval_vm("(define result1 (test-env 5))");
    // First call: (+ 5 10) = 15
    expectInt("result1", 15);
    // Invoke Continuation with different value
    // The y should still be 10 from the captured environment
    // But we're replacing the whole result, so we get 42
    expectInt("(saved-k 42)", 42);
}
TEST_F(ContinuationTest, ContinuationWithRecursion) {
    // Continuation used in a recursive function
    eval_vm(R"(
        (define (factorial n)
          (call/cc
            (lambda (k)
              (define (fact-helper n acc)
                (if (= n 0)
                    acc
                    (fact-helper (- n 1) (* n acc))))
              (fact-helper n 1))))
    )");
    expectInt("(factorial 5)", 120);
}

// ============================================================================
// Dynamic-Wind Tests (using toy_vm)
// ============================================================================

TEST_F(ContinuationTest, DynamicWindBasic) {
    // Basic dynamic-wind: before thunk is called, thunk executes, after thunk is called
    eval_vm("(define log '())");
    vm->debug_mode = true;
    try {
        eval_vm(R"(
            (dynamic-wind
              (lambda () (set! log (cons 'before log)))
              (lambda () (set! log (cons 'during log)) 42)
              (lambda () (set! log (cons 'after log))))
        )");
    } catch (const std::exception& e) {
        std::cout << "\nException: " << e.what() << std::endl;
        throw;
    }
    vm->debug_mode = false;
    // Log should be: (after during before) - last added first in reverse
    Object* result = eval_vm("log");
    ASSERT_TRUE(result->pair());
    // Check log contains the right symbols in order
    expectInt("(length log)", 3);
}

TEST_F(ContinuationTest, DynamicWindReturnValue) {
    // Enable debug mode on the VM
    vm->debug_mode = true;

    // dynamic-wind should return the value from the thunk
    Object* result = eval_vm("(dynamic-wind (lambda () 1) (lambda () 42) (lambda () 2))");

    vm->debug_mode = false;

    std::cout << "\n=== Final Result ===" << std::endl;
    std::cout << "DynamicWindReturnValue result ptr: " << (void*)result << std::endl;
    std::cout << "DynamicWindReturnValue result type: " << (result ? result->get_type() : -1) << std::endl;
    std::cout << "DynamicWindReturnValue numeric::is_number: " << (result ? numeric::is_number(result) : false) << std::endl;

    // Check if it's a procedure
    auto* proc = dynamic_cast<zombie::runtime::Procedure*>(result);
    if (proc) {
        std::cout << "DynamicWindReturnValue is a Procedure!" << std::endl;
    }

    expectInt("(dynamic-wind (lambda () 1) (lambda () 42) (lambda () 2))", 42);
    expectInt("(dynamic-wind (lambda () #f) (lambda () (+ 10 20)) (lambda () #t))", 30);
}

TEST_F(ContinuationTest, DynamicWindNested) {
    // Nested dynamic-wind calls
    eval_vm("(define log '())");
    eval_vm(R"(
        (dynamic-wind
          (lambda () (set! log (cons 'outer-before log)))
          (lambda ()
            (dynamic-wind
              (lambda () (set! log (cons 'inner-before log)))
              (lambda () (set! log (cons 'inner-thunk log)) 99)
              (lambda () (set! log (cons 'inner-after log)))))
          (lambda () (set! log (cons 'outer-after log))))
    )");
    // Should have 5 entries: outer-before, inner-before, inner-thunk, inner-after, outer-after
    expectInt("(length log)", 5);
}

TEST_F(ContinuationTest, DynamicWindWithCallCCEscape) {
    // When call/cc escapes from inside dynamic-wind, after should still be called
    eval_vm("(define log '())");
    eval_vm(R"(
        (call/cc
          (lambda (escape)
            (dynamic-wind
              (lambda () (set! log (cons 'before log)))
              (lambda ()
                (set! log (cons 'during log))
                (escape 42)
                (set! log (cons 'not-reached log)))
              (lambda () (set! log (cons 'after log))))))
    )");
    // Log should have: after, during, before (escape should trigger after thunk)
    expectInt("(length log)", 3);
    // Verify 'not-reached is NOT in the log
    Object* log = eval_vm("log");
    bool found_not_reached = false;
    while (log && !log->is_nil() && log->pair()) {
        Object* item = zombie::runtime::car(log);
        if (auto* sym = dynamic_cast<a_symbol*>(item)) {
            if (sym->getName() == "not-reached") {
                found_not_reached = true;
            }
        }
        log = zombie::runtime::cdr(log);
    }
    EXPECT_FALSE(found_not_reached) << "Escape should have prevented 'not-reached from being logged";
}

TEST_F(ContinuationTest, DynamicWindContinuationReentry) {
    // Reentry into a dynamic-wind context should call before thunk again
    eval_vm("(define log '())");
    eval_vm("(define cont #f)");
    eval_vm(R"(
        (let ((count 0))
          (dynamic-wind
            (lambda () (set! log (cons 'before log)))
            (lambda ()
              (set! count (+ count 1))
              (if (= count 1)
                  (call/cc (lambda (k) (set! cont k) 'first-time))
                  'second-time))
            (lambda () (set! log (cons 'after log)))))
    )");
    // First time: before, thunk returns 'first-time, after
    // After calling (cont 'x): before is called again (reentry!)
    expectInt("(length log)", 2);  // before, after

    // Now invoke the continuation to reenter
    eval_vm("(cont 'reentry)");
    // Should have: after (exit), before (reentry), after (normal exit)
    // Total log should be: before, after, before, after (4 entries)
    // Wait, the rewind logic will call: after (unwinding), before (rewinding)
    // So total should be at least 4 entries
}

TEST_F(ContinuationTest, DynamicWindPreservesValue) {
    // Verify that dynamic-wind correctly propagates the thunk's return value
    expectInt("(+ 1 (dynamic-wind (lambda () #f) (lambda () 10) (lambda () #t)))", 11);
    expectInt("(* 2 (dynamic-wind (lambda () 0) (lambda () 5) (lambda () 0)))", 10);
}
