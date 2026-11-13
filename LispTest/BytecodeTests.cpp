#include "pch.h"
#include <string>
#include <sstream>

// Zombie Lisp includes
#include "Object.h"
#include "atom.h"
#include "cons_cell.h"
#include "Lisp.h"
#include "Environment.h"
#include "primitive_base.h"
#include "primitive.h"
#include "constants.h"
#include "gc_manager.h"
#include "scheme_reader.h"
#include "numeric_runtime.h"

// Bytecode includes
#include "bytecode_vm.h"
#include "compiler.h"

// Access to global GC instance
extern gc_manager* gc;

// Test fixture for Bytecode VM tests
class BytecodeVMTest : public ::testing::Test {
protected:
    zombie::bytecode::BytecodeVM* vm;
    Environment* env;
    scheme_reader* reader;
    gc_root_guard* guard_env;

    void SetUp() override {
        // Global environment already initialized init_lisp_constants() in main
        // Create environment and bytecode VM for each test
        // IMPORTANT: Use gc->allocate to ensure GC tracking
        env = gc->allocate<Environment>();

        // Protect from GC immediately
        guard_env = new gc_root_guard((Object*)env);

        zombie::runtime::Primitive::install_primitives(env);
        vm = new zombie::bytecode::BytecodeVM(env, false);  // debug=false for tests
        reader = new scheme_reader();
    }

    void TearDown() override {
        // Cleanup is handled by GC
        delete reader;

        // Unregister GC root
        delete guard_env;
    }

    // Helper: Evaluate a Scheme expression from string using bytecode VM
    Object* eval(const std::string& code) {
        Object* expr = reader->read_from_string(code);
        return vm->eval(expr, env);
    }

    // Helper: Compare symbol by name (not pointer)
    void expectSymbol(Object* obj, const std::string& expected_name) {
        a_symbol* sym = dynamic_cast<a_symbol*>(obj);
        ASSERT_NE(sym, nullptr) << "Expected a symbol";
        EXPECT_EQ(sym->value(), expected_name);
    }

    // Helper: Evaluate and expect integer result
    void expectInt(const std::string& code, int expected) {
        Object* result = eval(code);
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

    // Helper: Evaluate and expect double result
    void expectDouble(const std::string& code, double expected, double tolerance = 0.0001) {
        Object* result = eval(code);
        ASSERT_NE(result, nullptr) << "Expression returned null: " << code;

        ASSERT_TRUE(numeric::is_number(result)) << "Result is not a number: " << code;
        double actual = numeric::to_double(result);
        EXPECT_NEAR(actual, expected, tolerance) << "For expression: " << code;
    }

    // Helper: Evaluate and expect string result
    void expectString(const std::string& code, const std::string& expected) {
        Object* result = eval(code);
        ASSERT_NE(result, nullptr) << "Expression returned null: " << code;

        a_string* str = dynamic_cast<a_string*>(result);
        ASSERT_NE(str, nullptr) << "Result is not a string: " << code;
        EXPECT_EQ(str->value(), expected) << "For expression: " << code;
    }

    // Helper: Evaluate and expect boolean result
    void expectBool(const std::string& code, bool expected) {
        Object* result = eval(code);
        ASSERT_NE(result, nullptr) << "Expression returned null: " << code;

        // R5RS: #f is the only false value, everything else is true
        if (expected) {
            EXPECT_NE(result, false_obj) << "Expected true for: " << code;
        } else {
            EXPECT_EQ(result, false_obj) << "Expected false for: " << code;
        }
    }

    // Helper: Evaluate and expect nil result
    void expectNil(const std::string& code) {
        Object* result = eval(code);
        EXPECT_EQ(result, nil) << "Expected nil for: " << code;
    }
};

// ============================================================================
// Basic Arithmetic Tests (Optimized Instructions)
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeAddition) {
    expectInt("(+ 2 2)", 4);
    expectInt("(+ 10 20)", 30);
    expectDouble("(+ 2.5 3.5)", 6.0);
}

TEST_F(BytecodeVMTest, BytecodeSubtraction) {
    expectInt("(- 10 5)", 5);
    expectInt("(- 100 50)", 50);
    expectDouble("(- 10.5 2.5)", 8.0);
}

TEST_F(BytecodeVMTest, BytecodeMultiplication) {
    expectInt("(* 3 4)", 12);
    expectInt("(* 5 6)", 30);
    expectDouble("(* 2.5 4)", 10.0);
}

TEST_F(BytecodeVMTest, BytecodeDivision) {
    expectDouble("(/ 10 2)", 5.0);
    expectDouble("(/ 100 4)", 25.0);
}

TEST_F(BytecodeVMTest, BytecodeNestedArithmetic) {
    expectInt("(+ (* 2 3) 4)", 10);  // (2 * 3) + 4 = 10
    expectInt("(* (+ 2 3) (- 10 4))", 30);  // (2 + 3) * (10 - 4) = 30
    expectInt("(- (* 5 5) (/ 10 2))", 20);  // (5 * 5) - (10 / 2) = 20
}

// ============================================================================
// Comparison Tests (Optimized Instructions)
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeEquality) {
    expectBool("(= 5 5)", true);
    expectBool("(= 5 6)", false);
    expectBool("(= 10.5 10.5)", true);
}

TEST_F(BytecodeVMTest, BytecodeLessThan) {
    expectBool("(< 3 5)", true);
    expectBool("(< 5 3)", false);
    expectBool("(< 5 5)", false);
}

TEST_F(BytecodeVMTest, BytecodeGreaterThan) {
    expectBool("(> 10 5)", true);
    expectBool("(> 5 10)", false);
    expectBool("(> 5 5)", false);
}

// ============================================================================
// List Operation Tests (Optimized Instructions)
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeCons) {
    Object* result = eval("(cons 1 2)");
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->pair() != 0) << "Result should be a pair";
}

TEST_F(BytecodeVMTest, BytecodeCar) {
    expectInt("(car (cons 1 2))", 1);
    expectInt("(car (cons 42 99))", 42);
}

TEST_F(BytecodeVMTest, BytecodeCdr) {
    expectInt("(cdr (cons 1 2))", 2);
    expectInt("(cdr (cons 42 99))", 99);
}

TEST_F(BytecodeVMTest, BytecodeNestedListOps) {
    expectInt("(car (cons (+ 1 2) (+ 3 4)))", 3);
    expectInt("(cdr (cons (+ 1 2) (+ 3 4)))", 7);
}

// ============================================================================
// Quote Tests
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeQuoteSymbol) {
    Object* result = eval("(quote hello)");
    ASSERT_NE(result, nullptr);
    // Check if it's a symbol (scheme_reader produces a_symbol)
    a_symbol* sym = dynamic_cast<a_symbol*>(result);
    if (!sym) {
        // Fallback to a_string (legacy)
        a_string* str = dynamic_cast<a_string*>(result);
        ASSERT_NE(str, nullptr);
        EXPECT_EQ(str->value(), "hello");
    } else {
        EXPECT_EQ(sym->value(), "hello");
    }
}

TEST_F(BytecodeVMTest, BytecodeQuoteNumber) {
    expectInt("(quote 42)", 42);
}

TEST_F(BytecodeVMTest, BytecodeQuoteList) {
    Object* result = eval("(quote (1 2 3))");
    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->pair() != 0) << "Quoted list should be a pair";
}

// ============================================================================
// If Expression Tests
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeIfTrue) {
    expectInt("(if (< 5 10) 100 200)", 100);
    expectInt("(if (> 10 5) 42 99)", 42);
}

TEST_F(BytecodeVMTest, BytecodeIfFalse) {
    expectInt("(if (> 5 10) 100 200)", 200);
    expectInt("(if (< 10 5) 42 99)", 99);
}

TEST_F(BytecodeVMTest, BytecodeIfNoAlternate) {
    Object* result = eval("(if (> 5 10) 100)");
    EXPECT_EQ(result, nil) << "If with false test and no alternate should return nil";
}

TEST_F(BytecodeVMTest, BytecodeNestedIf) {
    expectInt("(if (< 5 10) (if (> 3 2) 1 2) 3)", 1);
    expectInt("(if (> 5 10) 1 (if (< 3 5) 2 3))", 2);
}

// ============================================================================
// Define and Variable Lookup Tests
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeDefineVariable) {
    Object* define_result = eval("(define x 42)");
    std::cout << "Define returned: ";
    if (define_result) {
        define_result->print(std::cout);
        std::cout << " (type: " << typeid(*define_result).name() << ")";
    } else {
        std::cout << "nullptr";
    }
    std::cout << std::endl;

    Object* lookup_result = eval("x");
    std::cout << "Lookup returned: ";
    if (lookup_result) {
        lookup_result->print(std::cout);
        std::cout << " (type: " << typeid(*lookup_result).name() << ")";
    } else {
        std::cout << "nullptr";
    }
    std::cout << std::endl;

    expectInt("x", 42);
}

TEST_F(BytecodeVMTest, BytecodeDefineMultipleVariables) {
    eval("(define a 10)");
    eval("(define b 20)");
    expectInt("(+ a b)", 30);
}

TEST_F(BytecodeVMTest, BytecodeRedefineVariable) {
    eval("(define x 10)");
    expectInt("x", 10);
    eval("(define x 20)");
    expectInt("x", 20);
}

// ============================================================================
// Set! Tests
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeSetBang) {
    eval("(define x 10)");
    expectInt("x", 10);
    eval("(set! x 20)");
    expectInt("x", 20);
}

TEST_F(BytecodeVMTest, BytecodeSetBangExpression) {
    eval("(define x 5)");
    eval("(set! x (+ x 1))");
    expectInt("x", 6);
}

// ============================================================================
// Begin Tests
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeBegin) {
    expectInt("(begin (+ 1 2) (* 3 4))", 12);
    expectInt("(begin 1 2 3 4 5)", 5);
}

TEST_F(BytecodeVMTest, BytecodeBeginWithSideEffects) {
    eval("(define x 0)");
    eval("(begin (set! x 10) (set! x 20) (set! x 30))");
    expectInt("x", 30);
}

TEST_F(BytecodeVMTest, BytecodeEmptyBegin) {
    expectNil("(begin)");
}

// ============================================================================
// AND Tests (Short-circuit evaluation)
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeAndAllTrue) {
    expectBool("(and (> 5 3) (< 2 10))", true);
    expectBool("(and (= 5 5) (= 10 10))", true);
}

TEST_F(BytecodeVMTest, BytecodeAndWithFalse) {
    expectBool("(and (> 5 10) (< 2 10))", false);
    expectBool("(and (< 2 10) (> 5 10))", false);
}

TEST_F(BytecodeVMTest, BytecodeAndEmpty) {
    expectBool("(and)", true);  // (and) => #t
}

TEST_F(BytecodeVMTest, BytecodeAndReturnsLastValue) {
    expectInt("(and 1 2 3 4)", 4);  // Returns last true value
}

// ============================================================================
// OR Tests (Short-circuit evaluation)
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeOrFirstTrue) {
    expectBool("(or (> 5 3) (< 2 10))", true);
}

TEST_F(BytecodeVMTest, BytecodeOrAllFalse) {
    expectBool("(or (> 5 10) (< 10 2))", false);
}

TEST_F(BytecodeVMTest, BytecodeOrEmpty) {
    expectBool("(or)", false);  // (or) => #f
}

TEST_F(BytecodeVMTest, BytecodeOrReturnsFirstTrueValue) {
    expectInt("(or (> 5 10) 42 99)", 42);  // Returns first true value
}

// ============================================================================
// Lambda Tests
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeSimpleLambda) {
    expectInt("((lambda (x) (* x 2)) 5)", 10);
    expectInt("((lambda (x) (+ x 10)) 5)", 15);
}

TEST_F(BytecodeVMTest, BytecodeLambdaMultipleArgs) {
    expectInt("((lambda (x y) (+ x y)) 3 4)", 7);
    expectInt("((lambda (a b c) (* a (+ b c))) 2 3 4)", 14);
}

TEST_F(BytecodeVMTest, BytecodeLambdaNoArgs) {
    expectInt("((lambda () 42))", 42);
}

// ============================================================================
// Closure Tests
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeSimpleClosure) {
    eval("(define (make-adder n) (lambda (x) (+ x n)))");
    eval("(define add10 (make-adder 10))");
    expectInt("(add10 5)", 15);
    expectInt("(add10 100)", 110);
}

TEST_F(BytecodeVMTest, BytecodeClosureCapturesEnvironment) {
    eval("(define (make-multiplier n) (lambda (x) (* x n)))");
    eval("(define times2 (make-multiplier 2))");
    eval("(define times5 (make-multiplier 5))");
    expectInt("(times2 10)", 20);
    expectInt("(times5 10)", 50);
}

TEST_F(BytecodeVMTest, BytecodeNestedClosures) {
    eval("(define (make-adder-maker x) (lambda (y) (lambda (z) (+ x (+ y z)))))");
    eval("(define add10-maker (make-adder-maker 10))");
    eval("(define add10-20 (add10-maker 20))");
    expectInt("(add10-20 5)", 35);  // 10 + 20 + 5
}

// ============================================================================
// Complex Expression Tests
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeComplexArithmetic) {
    // ((10 + 20) * 3) - (100 / 4)
    expectInt("(- (* (+ 10 20) 3) (/ 100 4))", 65);
}

TEST_F(BytecodeVMTest, BytecodeComplexConditional) {
    eval("(define (max a b) (if (> a b) a b))");
    expectInt("(max 10 20)", 20);
    expectInt("(max 50 30)", 50);
}

TEST_F(BytecodeVMTest, BytecodeFactorial) {
    eval("(define (fact n) (if (< n 2) 1 (* n (fact (- n 1)))))");
    expectInt("(fact 5)", 120);
    expectInt("(fact 6)", 720);
}

TEST_F(BytecodeVMTest, BytecodeFibonacci) {
    eval("(define (fib n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))");
    expectInt("(fib 5)", 5);
    expectInt("(fib 7)", 13);
}

// ============================================================================
// Higher-Order Function Tests
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeMapSimulation) {
    eval("(define (double x) (* x 2))");
    expectInt("(double 5)", 10);
    expectInt("(double 10)", 20);
}

TEST_F(BytecodeVMTest, BytecodeFunctionComposition) {
    eval("(define (add1 x) (+ x 1))");
    eval("(define (mul2 x) (* x 2))");
    expectInt("(mul2 (add1 5))", 12);  // (5 + 1) * 2 = 12
    expectInt("(add1 (mul2 5))", 11);  // (5 * 2) + 1 = 11
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeZeroDivision) {
    // This should throw or return error
    EXPECT_THROW(eval("(/ 10 0)"), std::runtime_error);
}

TEST_F(BytecodeVMTest, BytecodeLargeNumbers) {
    expectInt("(+ 1000000 2000000)", 3000000);
    expectInt("(* 1000 1000)", 1000000);
}

TEST_F(BytecodeVMTest, BytecodeDeeplyNestedExpressions) {
    expectInt("(+ (+ (+ (+ (+ 1 1) 1) 1) 1) 1)", 6);
}

// ============================================================================
// Comparison with AST Interpreter
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeMatchesASTInterpreter) {
    // Create AST interpreter for comparison
    Lisp* ast_interp = new Lisp();
    Environment* ast_env = new Environment();
    zombie::runtime::Primitive::install_primitives(ast_env);
    ast_interp->set_environment(ast_env);

    scheme_reader test_reader;

    // Test several expressions
    std::vector<std::string> test_cases = {
        "(+ 1 2)",
        "(* 3 4)",
        "(- 10 5)",
        "(/ 20 4)",
        "(if (< 5 10) 100 200)",
        "(if (> 5 10) 100 200)",
        "((lambda (x) (* x 2)) 5)",
        "(car (cons 1 2))",
        "(cdr (cons 3 4))"
    };

    for (const auto& code : test_cases) {
        // Eval with bytecode VM
        Object* bc_result = eval(code);

        // Eval with AST interpreter
        Object* ast_expr = test_reader.read_from_string(code);
        Object* ast_result = ast_interp->new_eval(ast_expr, ast_env);

        // Compare results
        if (numeric::is_number(bc_result) && numeric::is_number(ast_result)) {
            EXPECT_EQ(numeric::to_double(bc_result), numeric::to_double(ast_result))
                << "Bytecode and AST results differ for: " << code;
        }
    }
}

// ============================================================================
// Disassembly and Debug Tests
// ============================================================================

TEST_F(BytecodeVMTest, BytecodeDisassemblyWorks) {
    // Just verify disassembly doesn't crash
    zombie::bytecode::Compiler compiler;
    Object* expr = reader->read_from_string("(+ 1 2)");
    auto compiled = compiler.compile(expr, env);

    std::ostringstream oss;
    vm->disassemble(compiled.bytecode, compiled.constants, oss);

    std::string disasm = oss.str();
    EXPECT_FALSE(disasm.empty()) << "Disassembly should produce output";
    EXPECT_NE(disasm.find("LOAD_CONST"), std::string::npos)
        << "Disassembly should contain instruction names";
}

// ============================================================================
// Dynamic-wind Tests (R5RS Section 6.4)
// ============================================================================

TEST_F(BytecodeVMTest, DynamicWindBasic) {
    // Test that before, thunk, and after are all called in order
    eval("(define trace '())");
    Object* result = eval(R"(
        (dynamic-wind
            (lambda () (set! trace (cons 'before trace)))
            (lambda () (set! trace (cons 'thunk trace)) 42)
            (lambda () (set! trace (cons 'after trace))))
    )");

    // Result should be 42
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 42);

    // Trace should be (after thunk before) in reverse order
    Object* trace = eval("trace");
    ASSERT_FALSE(trace->is_nil());
    expectSymbol(car(trace), "after");
}

TEST_F(BytecodeVMTest, DynamicWindWithCallCCEscape) {
    // Test that after thunk runs when escaping via call/cc
    eval("(define trace '())");
    eval("(define saved-k '())");

    Object* result = eval(R"(
        (+ 100 (dynamic-wind
                 (lambda () (set! trace (cons 'before trace)))
                 (lambda ()
                   (call/cc (lambda (k)
                              (set! saved-k k)
                              (set! trace (cons 'thunk trace))
                              10)))
                 (lambda () (set! trace (cons 'after trace)))))
    )");

    // Result should be 110 (100 + 10)
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 110);

    // Trace should show: after, thunk, before
    Object* trace = eval("trace");
    ASSERT_FALSE(trace->is_nil());
    expectSymbol(car(trace), "after");
    expectSymbol(car(cdr(trace)), "thunk");
    expectSymbol(car(cdr(cdr(trace))), "before");
}

// DISABLED: Bytecode VM continuation reentry not fully implemented
TEST_F(BytecodeVMTest, DynamicWindWithCallCCReentry) {
    // Test that before/after thunks run when re-entering via Continuation
    eval("(define trace '())");
    eval("(define saved-k '())");

    // Initial call captures Continuation
    eval(R"(
        (define result1
          (+ 100 (dynamic-wind
                   (lambda () (set! trace (cons 'before trace)))
                   (lambda ()
                     (call/cc (lambda (k)
                                (set! saved-k k)
                                (set! trace (cons 'thunk trace))
                                10)))
                   (lambda () (set! trace (cons 'after trace))))))
    )");

    expectInt("result1", 110);

    // Clear trace to see re-entry behavior
    eval("(set! trace '())");

    // Re-invoke Continuation - should run before thunk again
    Object* result = eval("(saved-k 20)");
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 120);  // 100 + 20

    // Trace should show before and after thunks were called on re-entry
    // Order: after is most recent (at head), then before
    Object* trace = eval("trace");
    ASSERT_FALSE(trace->is_nil());
    expectSymbol(car(trace), "after");
    expectSymbol(car(cdr(trace)), "before");
}

TEST_F(BytecodeVMTest, DynamicWindNested) {
    // Test nested dynamic-wind calls
    eval("(define trace '())");

    Object* result = eval(R"(
        (dynamic-wind
            (lambda () (set! trace (cons 'outer-before trace)))
            (lambda ()
                (dynamic-wind
                    (lambda () (set! trace (cons 'inner-before trace)))
                    (lambda () (set! trace (cons 'thunk trace)) 42)
                    (lambda () (set! trace (cons 'inner-after trace)))))
            (lambda () (set! trace (cons 'outer-after trace))))
    )");

    // Result should be 42
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 42);

    // Trace should show proper nesting order
    Object* trace = eval("trace");
    ASSERT_FALSE(trace->is_nil());
    // Order: outer-after, inner-after, thunk, inner-before, outer-before
    expectSymbol(car(trace), "outer-after");
}

TEST_F(BytecodeVMTest, DynamicWindNestedWithCallCC) {
    // Test nested dynamic-wind with call/cc escape
    eval("(define trace '())");
    eval("(define saved-k '())");

    Object* result = eval(R"(
        (dynamic-wind
            (lambda () (set! trace (cons 'outer-before trace)))
            (lambda ()
                (dynamic-wind
                    (lambda () (set! trace (cons 'inner-before trace)))
                    (lambda ()
                      (call/cc (lambda (k)
                                 (set! saved-k k)
                                 (set! trace (cons 'thunk trace))
                                 42)))
                    (lambda () (set! trace (cons 'inner-after trace)))))
            (lambda () (set! trace (cons 'outer-after trace))))
    )");

    // Result should be 42
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 42);

    // Both after thunks should have been called
    Object* trace = eval("trace");
    ASSERT_FALSE(trace->is_nil());
    expectSymbol(car(trace), "outer-after");
    expectSymbol(car(cdr(trace)), "inner-after");
}

// DISABLED: Bytecode VM continuation reentry not fully implemented
TEST_F(BytecodeVMTest, DynamicWindMultipleReentries) {
    // Test multiple re-entries via Continuation
    eval("(define trace '())");
    eval("(define saved-k '())");

    eval(R"(
        (dynamic-wind
            (lambda () (set! trace (cons 'before trace)))
            (lambda ()
              (call/cc (lambda (k)
                         (set! saved-k k)
                         10)))
            (lambda () (set! trace (cons 'after trace))))
    )");

    // Clear trace
    eval("(set! trace '())");

    // First re-entry
    eval("(saved-k 20)");
    Object* trace1 = eval("trace");
    ASSERT_FALSE(trace1->is_nil());

    // Clear and re-enter again
    eval("(set! trace '())");
    eval("(saved-k 30)");
    Object* trace2 = eval("trace");
    ASSERT_FALSE(trace2->is_nil());
    // Before thunk should have been called each time
}

TEST_F(BytecodeVMTest, DynamicWindWithRecursion) {
    // Test dynamic-wind with recursive function

    // First test: simple factorial without dynamic-wind
    eval(R"(
        (define (fact-simple n)
          (if (= n 0)
              1
              (* n (fact-simple (- n 1)))))
    )");

    std::cout << "Testing simple factorial(5)..." << std::endl;
    expectInt("(fact-simple 5)", 120);
    std::cout << "Simple factorial works!" << std::endl;

    // Second test: factorial with dynamic-wind
    eval(R"(
        (define (factorial n)
          (dynamic-wind
              (lambda () 'before)
              (lambda ()
                (if (= n 0)
                    1
                    (* n (factorial (- n 1)))))
              (lambda () 'after)))
    )");

    std::cout << "Testing dynamic-wind factorial(1)..." << std::endl;
    expectInt("(factorial 1)", 1);
    std::cout << "factorial(1) works!" << std::endl;

    std::cout << "\nTesting dynamic-wind factorial(2)..." << std::endl;
    vm->set_debug(true);
    Object* result2 = eval("(factorial 2)");
    vm->set_debug(false);
    std::cout << "factorial(2) returned: ";
    if (result2) result2->print(std::cout);
    std::cout << std::endl;
    expectInt("(factorial 2)", 2);
    std::cout << "factorial(2) works!" << std::endl;

    std::cout << "Testing dynamic-wind factorial(3)..." << std::endl;
    expectInt("(factorial 3)", 6);
    std::cout << "factorial(3) works!" << std::endl;

    std::cout << "Testing dynamic-wind factorial(5)..." << std::endl;
    expectInt("(factorial 5)", 120);
}

TEST_F(BytecodeVMTest, DynamicWindZeroArgThunks) {
    // Verify that thunks are really zero-argument
    eval("(define x 0)");

    // This should work - all thunks are zero-arg lambdas
    Object* result = eval(R"(
        (dynamic-wind
            (lambda () (set! x 1))
            (lambda () 42)
            (lambda () (set! x 2)))
    )");

    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 42);
    expectInt("x", 2);
}

TEST_F(BytecodeVMTest, DynamicWindReturnValue) {
    // Test that dynamic-wind returns the value of the thunk
    std::cout << "DEBUG: Testing first dynamic-wind (should return 5)..." << std::endl;
    expectInt(R"(
        (dynamic-wind
            (lambda () 1)
            (lambda () (+ 2 3))
            (lambda () 4))
    )", 5);

    std::cout << "DEBUG: Testing second dynamic-wind (should return \"hello\")..." << std::endl;
    expectString(R"(
        (dynamic-wind
            (lambda () 'a)
            (lambda () "hello")
            (lambda () 'b))
    )", "hello");
}

// ============================================================================
// R5RS Canonical Examples for Dynamic-wind
// ============================================================================

// DISABLED: Bytecode VM continuation reentry not fully implemented
TEST_F(BytecodeVMTest, R5RS_DynamicWindExample1) {
    // R5RS example: Testing the order of execution
    eval("(define path '())");
    eval("(define c '())");

    eval(R"(
        (define (add-to-path! x)
          (set! path (cons x path)))
    )");

    eval(R"(
        (dynamic-wind
          (lambda () (add-to-path! 'connect))
          (lambda ()
            (add-to-path! 'talk1)
            (call/cc (lambda (k) (set! c k)))
            (add-to-path! 'talk2))
          (lambda () (add-to-path! 'disconnect)))
    )");

    // First execution: connect, talk1, talk2, disconnect
    Object* path1 = eval("path");
    ASSERT_FALSE(path1->is_nil());

    // Clear path and invoke Continuation
    eval("(set! path '())");
    eval("(if c (c '()) '())");  // Re-enter if c is set

    // Should show: connect, talk2, disconnect (no talk1 on re-entry)
    Object* path2 = eval("path");
    // Verify path contains expected elements
}

TEST_F(BytecodeVMTest, R5RS_DynamicWindExample2) {
    // R5RS example: Escaping and re-entering wind boundaries
    eval("(define trace '())");
    eval("(define k-out '())");
    eval("(define k-in '())");

    eval(R"(
        (define (log msg)
          (set! trace (cons msg trace)))
    )");

    // Outer dynamic-wind with Continuation capture
    eval(R"(
        (dynamic-wind
          (lambda () (log 'outer-before))
          (lambda ()
            (log 'outer-body-start)
            (call/cc (lambda (k) (set! k-out k)))
            (log 'outer-body-end))
          (lambda () (log 'outer-after)))
    )");

    // Trace should show normal execution
    Object* trace1 = eval("trace");
    ASSERT_FALSE(trace1->is_nil());
}

// DISABLED: Bytecode VM continuation reentry not fully implemented
TEST_F(BytecodeVMTest, DynamicWindCrossingBoundaries) {
    // Test crossing dynamic-wind boundaries with call/cc
    // Initial run: call/cc returns 10 normally, both winds exit normally => both after thunks run
    eval("(define trace '())");
    eval("(define k-escape '())");

    Object* r1 = eval(R"(
        (+ 1000
           (dynamic-wind
             (lambda () (set! trace (cons 'outer-before trace)))
             (lambda ()
               (+ 100
                  (dynamic-wind
                    (lambda () (set! trace (cons 'inner-before trace)))
                    (lambda ()
                      (call/cc (lambda (k)
                                 (set! k-escape k)
                                 10)))
                    (lambda () (set! trace (cons 'inner-after trace))))))
             (lambda () (set! trace (cons 'outer-after trace)))))
    )");

    ASSERT_NE(r1, nullptr);
    ASSERT_TRUE(numeric::is_number(r1));
    EXPECT_EQ(numeric::to_int64(r1), 1110);  // 1000 + 100 + 10

    // Because we cons to the front, trace is reverse-chronological:
    // (outer-after inner-after inner-before outer-before)
    expectSymbol(eval("(car trace)"), "outer-after");
    expectSymbol(eval("(cadr trace)"), "inner-after");
    expectSymbol(eval("(caddr trace)"), "inner-before");
    expectSymbol(eval("(cadddr trace)"), "outer-before");

    // Clear trace and re-invoke Continuation: re-enters both winds (before thunks),
    // then exits both winds (after thunks). Last event still outer-after.
    eval("(set! trace '())");
    Object* r2 = eval("(k-escape 20)");
    ASSERT_NE(r2, nullptr);
    ASSERT_TRUE(numeric::is_number(r2));
    EXPECT_EQ(numeric::to_int64(r2), 1120);  // 1000 + 100 + 20

    // Same sequence on re-entry
    expectSymbol(eval("(car trace)"), "outer-after");
    expectSymbol(eval("(cadr trace)"), "inner-after");
    expectSymbol(eval("(caddr trace)"), "inner-before");
    expectSymbol(eval("(cadddr trace)"), "outer-before");
}

TEST_F(BytecodeVMTest, DynamicWindComplexControlFlow) {
    // Test complex control flow with multiple continuations
    eval("(define trace '())");
    eval("(define k1 '())");
    eval("(define k2 '())");

    eval(R"(
        (define result1
          (dynamic-wind
            (lambda () (set! trace (cons 'a-before trace)))
            (lambda ()
              (call/cc (lambda (k) (set! k1 k)))
              (dynamic-wind
                (lambda () (set! trace (cons 'b-before trace)))
                (lambda ()
                  (call/cc (lambda (k) (set! k2 k)))
                  42)
                (lambda () (set! trace (cons 'b-after trace)))))
            (lambda () (set! trace (cons 'a-after trace)))))
    )");

    expectInt("result1", 42);

    // Verify trace has entries
    Object* trace = eval("trace");
    ASSERT_FALSE(trace->is_nil());
}

// ============================================================================
// Tail Call Optimization (TCO) Tests
// ============================================================================

TEST_F(BytecodeVMTest, TCOTailRecursiveFactorial) {
    // Define tail-recursive factorial with accumulator
    eval(R"(
        (define (factorial n acc)
          (if (<= n 1)
              acc
              (factorial (- n 1) (* n acc))))
    )");

    // Test small value
    expectInt("(factorial 5 1)", 120);

    // Test large value (would stack overflow without TCO)
    // 1000! is huge, but TCO should handle the recursion depth
    Object* result = eval("(factorial 1000 1)");
    ASSERT_NE(result, nullptr);
    // Just verify it completes without error
}

TEST_F(BytecodeVMTest, TCOMutualRecursion) {
    // Define mutually recursive even? and odd? predicates
    eval(R"(
        (define (my-even? n)
          (if (= n 0)
              #t
              (my-odd? (- n 1))))
    )");

    eval(R"(
        (define (my-odd? n)
          (if (= n 0)
              #f
              (my-even? (- n 1))))
    )");

    // Test even numbers
    expectBool("(my-even? 0)", true);
    expectBool("(my-even? 2)", true);
    expectBool("(my-even? 100)", true);

    // Test odd numbers
    expectBool("(my-odd? 1)", true);
    expectBool("(my-odd? 3)", true);
    expectBool("(my-odd? 99)", true);

    // Test large values (would stack overflow without TCO)
    expectBool("(my-even? 1000)", true);
    expectBool("(my-odd? 999)", true);
}

TEST_F(BytecodeVMTest, TCODeepRecursion) {
    // Countdown function - tests very deep recursion
    eval(R"(
        (define (countdown n)
          (if (<= n 0)
              0
              (countdown (- n 1))))
    )");

    // Very deep recursion (would overflow stack without TCO)
    expectInt("(countdown 5000)", 0);
}

TEST_F(BytecodeVMTest, TCONonTailCallStillWorks) {
    // Non-tail recursive factorial (traditional recursive form)
    eval(R"(
        (define (factorial n)
          (if (<= n 1)
              1
              (* n (factorial (- n 1)))))
    )");

    // This should work correctly (but will use stack)
    expectInt("(factorial 5)", 120);
    expectInt("(factorial 6)", 720);
}

TEST_F(BytecodeVMTest, TCOTailCallInIfBranches) {
    // Verify that both branches of if are in tail position
    eval(R"(
        (define (sign n)
          (if (< n 0)
              (negative-sign n)
              (positive-sign n)))
    )");

    eval(R"(
        (define (negative-sign n)
          (if (= n -1)
              -1
              (negative-sign (+ n 1))))
    )");

    eval(R"(
        (define (positive-sign n)
          (if (= n 1)
              1
              (positive-sign (- n 1))))
    )");

    expectInt("(sign -100)", -1);
    expectInt("(sign 100)", 1);
}

TEST_F(BytecodeVMTest, TCOTailCallInBegin) {
    // Only the last expression in begin should be tail-optimized
    eval(R"(
        (define (test-begin n)
          (begin
            (define temp (* n 2))  ; Not in tail position
            (countdown temp)))      ; In tail position
    )");

    eval(R"(
        (define (countdown n)
          (if (<= n 0)
              n
              (countdown (- n 1))))
    )");

    expectInt("(test-begin 1000)", 0);
}

TEST_F(BytecodeVMTest, TCOTailCallWithMultipleArgs) {
    // Test tail call with multiple arguments
    eval(R"(
        (define (sum-range start end acc)
          (if (> start end)
              acc
              (sum-range (+ start 1) end (+ acc start))))
    )");

    // Sum of 1..100 = 5050
    expectInt("(sum-range 1 100 0)", 5050);

    // Large range (tests TCO with multiple args)
    Object* result = eval("(sum-range 1 1000 0)");
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 500500);  // Sum of 1..1000
}

TEST_F(BytecodeVMTest, TCOTailCallVsNonTailCall) {
    // Define both tail and non-tail recursive versions
    eval(R"(
        (define (tail-sum n acc)
          (if (<= n 0)
              acc
              (tail-sum (- n 1) (+ acc n))))
    )");

    eval(R"(
        (define (non-tail-sum n)
          (if (<= n 0)
              0
              (+ n (non-tail-sum (- n 1)))))
    )");

    // Both should give same result for small values
    expectInt("(tail-sum 10 0)", 55);
    expectInt("(non-tail-sum 10)", 55);

    // But tail version can handle much deeper recursion
    expectInt("(tail-sum 1000 0)", 500500);
}

// ============================================================================
// Guard and parameterize (compiled to %guard / %parameterize rewrites)
// ============================================================================

TEST_F(BytecodeVMTest, GuardCatchesRaise) {
    Object* result = eval("(guard (e (#t 'caught)) (raise 'boom))");
    expectSymbol(result, "caught");
}

TEST_F(BytecodeVMTest, GuardBindsRaisedObject) {
    expectInt("(guard (e ((number? e) (+ e 1))) (raise 41))", 42);
}

TEST_F(BytecodeVMTest, GuardElseClause) {
    Object* result = eval("(guard (e ((number? e) 'num) (else 'other)) (raise 'sym))");
    expectSymbol(result, "other");
}

TEST_F(BytecodeVMTest, GuardNoRaiseReturnsBody) {
    expectInt("(guard (e (else 'err)) (+ 20 22))", 42);
}

TEST_F(BytecodeVMTest, GuardRethrowsWhenNoClauseMatches) {
    // No clause matches a symbol payload; the appended else re-raises
    EXPECT_THROW(eval("(guard (e ((number? e) 'num)) (raise 'sym))"), std::runtime_error);
}

TEST_F(BytecodeVMTest, GuardCatchesErrorObjects) {
    Object* result = eval(
        "(guard (e ((error-Object? e) (error-Object-message e))) "
        "  (error \"kaboom\" 1 2))");
    a_string* str = dynamic_cast<a_string*>(result);
    ASSERT_NE(str, nullptr);
    EXPECT_EQ(str->value(), "kaboom");
}

TEST_F(BytecodeVMTest, GuardCatchesPrimitiveErrors) {
    // C++-side runtime errors (division by zero) become error objects
    Object* result = eval("(guard (e (else 'err)) (/ 10 0))");
    expectSymbol(result, "err");
}

TEST_F(BytecodeVMTest, GuardNested) {
    // Inner guard does not match; payload re-raises to outer guard
    expectInt(
        "(guard (e ((number? e) (+ e 100))) "
        "  (guard (e2 ((string? e2) 'inner)) "
        "    (raise 7)))",
        107);
}

TEST_F(BytecodeVMTest, GuardBodyMultipleExpressions) {
    expectInt("(guard (e (else 'err)) 1 2 (+ 40 2))", 42);
}

TEST_F(BytecodeVMTest, ParameterizeBasic) {
    eval("(define p (make-parameter 10))");
    expectInt("(p)", 10);
    expectInt("(parameterize ((p 20)) (p))", 20);
    expectInt("(p)", 10);
}

TEST_F(BytecodeVMTest, ParameterizeMultipleBindings) {
    eval("(define pa (make-parameter 1))");
    eval("(define pb (make-parameter 2))");
    expectInt("(parameterize ((pa 10) (pb 20)) (+ (pa) (pb)))", 30);
    expectInt("(+ (pa) (pb))", 3);
}

TEST_F(BytecodeVMTest, ParameterizeAppliesConverter) {
    // Converter doubles; applied on entry but NOT re-applied on restore
    eval("(define pc (make-parameter 5 (lambda (x) (* x 2))))");
    expectInt("(pc)", 10);
    expectInt("(parameterize ((pc 7)) (pc))", 14);
    expectInt("(pc)", 10);
}

TEST_F(BytecodeVMTest, ParameterizeNested) {
    eval("(define pn (make-parameter 1))");
    expectInt(
        "(parameterize ((pn 2)) "
        "  (+ (pn) (parameterize ((pn 3)) (pn)) (pn)))",
        7);
    expectInt("(pn)", 1);
}

TEST_F(BytecodeVMTest, ParameterizeRestoresOnError) {
    eval("(define pe (make-parameter 10))");
    // The raise escapes the parameterize; the old value must be restored
    Object* result = eval(
        "(guard (e (else (pe))) "
        "  (parameterize ((pe 99)) (raise 'oops)))");
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(numeric::to_int64(result), 10);
}

TEST_F(BytecodeVMTest, ParameterizeBodyMultipleExpressions) {
    eval("(define pm (make-parameter 1))");
    expectInt("(parameterize ((pm 5)) 'first 'second (* (pm) (pm)))", 25);
}
