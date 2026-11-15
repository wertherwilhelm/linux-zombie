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

TEST_F(CoreIREngineTest, NamedLetLoop) {
    scheme_engine engine(EvaluatorType::COREIR);
    EXPECT_EQ(10.0, eval_num(engine,
        "(let loop ((i 0) (s 0)) (if (< i 5) (loop (+ i 1) (+ s i)) s))"));
}
