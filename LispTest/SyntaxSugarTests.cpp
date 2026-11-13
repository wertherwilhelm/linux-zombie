#include "pch.h"
#include "TestFixture.h"
#include "toy_vm.h"
#include "scheme_reader.h"
#include "numeric_runtime.h"

// Access to global GC instance
extern gc_manager* gc;

// Test fixture for syntax sugar features (let and define shorthand)
class SyntaxSugarTest : public ::testing::Test {
protected:
    Environment* env;
    scheme_reader* reader;
    toy::VM* vm;
    gc_root_guard* guard_env;

    void SetUp() override {
        // Global environment already initialized init_lisp_constants() in main
        // IMPORTANT: Use gc->allocate to ensure GC tracking
        env = gc->allocate<Environment>();

        // Protect from GC immediately
        guard_env = new gc_root_guard((Object*)env);

        zombie::runtime::Primitive::install_primitives(env);
        reader = new scheme_reader();
        vm = new toy::VM(env, false);  // Disable debug mode
    }

    void TearDown() override {
        delete reader;
        delete vm;

        // Unregister GC root
        delete guard_env;
    }

    Object* eval_vm(const std::string& code) {
        Object* expr = reader->read_from_string(code);
        vm->eval(expr, env);
        return vm->accumulator;
    }

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
};

// ============================================================================
// Define Shorthand Tests
// ============================================================================

TEST_F(SyntaxSugarTest, DefineShorthandSimple) {
    // (define (square x) (* x x))
    eval_vm("(define (square x) (* x x))");
    expectInt("(square 5)", 25);
}

TEST_F(SyntaxSugarTest, DefineShorthandMultipleArgs) {
    // (define (add3 x y z) (+ x (+ y z)))
    eval_vm("(define (add3 x y z) (+ x (+ y z)))");
    expectInt("(add3 1 2 3)", 6);
}

TEST_F(SyntaxSugarTest, DefineShorthandNoArgs) {
    // (define (get-42) 42)
    eval_vm("(define (get-42) 42)");
    expectInt("(get-42)", 42);
}

TEST_F(SyntaxSugarTest, DefineShorthandWithBegin) {
    // (define (foo x) (begin (+ x 1) (+ x 2)))
    eval_vm("(define (foo x) (begin (+ x 1) (+ x 2)))");
    expectInt("(foo 10)", 12);  // Returns last expression
}

TEST_F(SyntaxSugarTest, DefineShorthandRecursive) {
    // (define (factorial n) (if (= n 0) 1 (* n (factorial (- n 1)))))
    eval_vm("(define (factorial n) (if (= n 0) 1 (* n (factorial (- n 1)))))");
    expectInt("(factorial 5)", 120);
}

// ============================================================================
// Let Tests
// ============================================================================

TEST_F(SyntaxSugarTest, LetSimple) {
    // (let ((x 10) (y 20)) (+ x y))
    expectInt("(let ((x 10) (y 20)) (+ x y))", 30);
}

TEST_F(SyntaxSugarTest, LetSingleBinding) {
    // (let ((x 42)) x)
    expectInt("(let ((x 42)) x)", 42);
}

TEST_F(SyntaxSugarTest, LetNoBindings) {
    // (let () 99)
    expectInt("(let () 99)", 99);
}

TEST_F(SyntaxSugarTest, LetNestedScope) {
    // Outer variable should not be affected by let
    eval_vm("(define a 100)");
    expectInt("(let ((a 1) (b 2)) (+ a b))", 3);
    expectInt("a", 100);  // a should still be 100
}

TEST_F(SyntaxSugarTest, LetWithExpressions) {
    // (let ((x (+ 1 2)) (y (* 3 4))) (+ x y))
    expectInt("(let ((x (+ 1 2)) (y (* 3 4))) (+ x y))", 15);
}

TEST_F(SyntaxSugarTest, LetMultipleBodyExpressions) {
    // (let ((x 5)) (+ x 1) (+ x 2))
    expectInt("(let ((x 5)) (+ x 1) (+ x 2))", 7);  // Returns last expression
}

TEST_F(SyntaxSugarTest, NestedLet) {
    // (let ((x 1)) (let ((y 2)) (+ x y)))
    expectInt("(let ((x 1)) (let ((y 2)) (+ x y)))", 3);
}

// ============================================================================
// Combined Tests
// ============================================================================

TEST_F(SyntaxSugarTest, DefineAndLetCombined) {
    // (define (foo x) (let ((y 10)) (+ x y)))
    eval_vm("(define (foo x) (let ((y 10)) (+ x y)))");
    expectInt("(foo 5)", 15);
}

TEST_F(SyntaxSugarTest, LetWithDefineShorthand) {
    // Using a function defined with shorthand inside a let
    eval_vm("(define (double x) (* x 2))");
    expectInt("(let ((x 5)) (double x))", 10);
}
