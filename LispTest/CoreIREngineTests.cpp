// CoreIREngineTests.cpp - verifies the CoreIR evaluator is wired through
// scheme_engine (EvaluatorType::COREIR), i.e. the user-facing `--coreir` path,
// not just the lowering pipeline in isolation (see CoreIRTests.cpp for that).
#include "gtest/gtest.h"
#include "zombie_runtime_types.h"
#include "scheme_engine.h"
#include "constants.h"
#include "Base.h"

class CoreIREngineTest : public ::testing::Test {
protected:
    void SetUp() override { init_lisp_constants(); }
    static double eval_num(scheme_engine& e, const std::string& code) {
        Object* r = static_cast<Object*>(e.eval_string(code));
        EXPECT_TRUE(r != nullptr);
        return num(r);
    }
};

TEST_F(CoreIREngineTest, EvaluatorTypeSelects) {
    scheme_engine engine(EvaluatorType::COREIR);
    EXPECT_EQ(3.0, eval_num(engine, "(+ 1 2)"));
}

TEST_F(CoreIREngineTest, TopLevelDefinePersistsAcrossEvals) {
    // Separate eval_string calls must share the environment (REPL semantics).
    scheme_engine engine(EvaluatorType::COREIR);
    engine.eval_string("(define (sq x) (* x x))");
    EXPECT_EQ(49.0, eval_num(engine, "(sq 7)"));
}

TEST_F(CoreIREngineTest, RecursionAndClosures) {
    scheme_engine engine(EvaluatorType::COREIR);
    engine.eval_string(
        "(define (fact n) (if (< n 2) 1 (* n (fact (- n 1)))))");
    EXPECT_EQ(120.0, eval_num(engine, "(fact 5)"));
    EXPECT_EQ(7.0, eval_num(engine, "(((lambda (x) (lambda (y) (+ x y))) 3) 4)"));
}

TEST_F(CoreIREngineTest, DefineReturnsUnspecified) {
    // R7RS: the value of a define is unspecified (nil here), consistent with the
    // tree-walk and bytecode evaluators (the inconsistency that prompted this).
    scheme_engine engine(EvaluatorType::COREIR);
    EXPECT_TRUE(static_cast<Object*>(engine.eval_string("(define x 5)"))->is_nil());
    EXPECT_TRUE(static_cast<Object*>(engine.eval_string("(define (f y) (* y y))"))->is_nil());
    // Bindings still take effect:
    EXPECT_EQ(5.0, eval_num(engine, "x"));
    EXPECT_EQ(36.0, eval_num(engine, "(f 6)"));
}

TEST_F(CoreIREngineTest, NamedLetLoop) {
    scheme_engine engine(EvaluatorType::COREIR);
    EXPECT_EQ(10.0, eval_num(engine,
        "(let loop ((i 0) (s 0)) (if (< i 5) (loop (+ i 1) (+ s i)) s))"));
}

// `if` in operand position: the lowering threads the call's continuation as a
// builder closure spliced into both branches of a tail `if`, so an `if` nested
// inside a call/primitive argument lowers correctly (previously unsupported).
TEST_F(CoreIREngineTest, IfInOperandPosition) {
    scheme_engine engine(EvaluatorType::COREIR);
    EXPECT_EQ(21.0, eval_num(engine, "(+ 1 (if #f 10 20))"));
    EXPECT_EQ(107.0, eval_num(engine,
        "(+ (if (> 3 1) 100 0) (if (< 3 1) 5 7))"));          // two ifs as operands
    EXPECT_EQ(15.0, eval_num(engine, "(* (if #t 3 0) (if #t 5 9))"));
    // if as an argument to a general (non-primitive) call:
    engine.eval_string("(define (dbl x) (* 2 x))");
    EXPECT_EQ(14.0, eval_num(engine, "(dbl (if #f 1 7))"));
    // list-valued, to confirm the spliced value is the branch result, not a temp:
    Object* r = static_cast<Object*>(engine.eval_string("(list (if #t 1 2) (if #f 3 4))"));
    EXPECT_EQ("(1 4)", engine.print_value(r));
}
