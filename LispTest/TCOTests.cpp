// TCOTests.cpp - Tail-Call Optimization Tests for BytecodeVM
#include "pch.h"
#include "gtest/gtest.h"
#include "scheme_engine.h"
#include "constants.h"
#include "Base.h"

// Test fixture for TCO tests
class TCOTest : public ::testing::Test {
protected:
    void SetUp() override {
        init_lisp_constants();
    }
};

// Test 1: Tail-recursive factorial
TEST_F(TCOTest, TailRecursiveFactorial) {
    scheme_engine engine(EvaluatorType::BYTECODE);

    std::string code = R"(
        (define (factorial n acc)
          (if (<= n 1)
              acc
              (factorial (- n 1) (* n acc))))

        (factorial 5 1)
    )";

    Object* result = static_cast<Object*>(engine.eval_string(code));
    ASSERT_TRUE(result != nullptr);
    ASSERT_TRUE(result->is_atom());
    EXPECT_DOUBLE_EQ(120.0, num(result));
}

// Test 2: Deep recursion (would stack overflow without TCO)
TEST_F(TCOTest, DeepRecursion10000) {
    scheme_engine engine(EvaluatorType::BYTECODE);

    std::string code = R"(
        (define (count-down n)
          (if (<= n 0)
              0
              (count-down (- n 1))))

        (count-down 10000)
    )";

    // This should complete without stack overflow
    Object* result = static_cast<Object*>(engine.eval_string(code));
    ASSERT_TRUE(result != nullptr);
    EXPECT_DOUBLE_EQ(0.0, num(result));
}

// Test 3: Tail-recursive sum
TEST_F(TCOTest, TailRecursiveSum) {
    scheme_engine engine(EvaluatorType::BYTECODE);

    std::string code = R"(
        (define (sum-to n acc)
          (if (<= n 0)
              acc
              (sum-to (- n 1) (+ acc n))))

        (sum-to 100 0)
    )";

    Object* result = static_cast<Object*>(engine.eval_string(code));
    ASSERT_TRUE(result != nullptr);
    EXPECT_DOUBLE_EQ(5050.0, num(result));
}

// Test 4: Fibonacci with tail recursion
TEST_F(TCOTest, TailRecursiveFibonacci) {
    scheme_engine engine(EvaluatorType::BYTECODE);

    std::string code = R"(
        (define (fib n)
          (letrec ((fib-iter (lambda (a b count)
                               (if (= count 0)
                                   a
                                   (fib-iter b (+ a b) (- count 1))))))
            (fib-iter 0 1 n)))

        (fib 10)
    )";

    Object* result = static_cast<Object*>(engine.eval_string(code));
    ASSERT_TRUE(result != nullptr);
    EXPECT_DOUBLE_EQ(55.0, num(result));
}

// Test 5: Fibonacci with larger N
TEST_F(TCOTest, TailRecursiveFibonacci20) {
    scheme_engine engine(EvaluatorType::BYTECODE);

    std::string code = R"(
        (define (fib n)
          (letrec ((fib-iter (lambda (a b count)
                               (if (= count 0)
                                   a
                                   (fib-iter b (+ a b) (- count 1))))))
            (fib-iter 0 1 n)))

        (fib 20)
    )";

    Object* result = static_cast<Object*>(engine.eval_string(code));
    ASSERT_TRUE(result != nullptr);
    EXPECT_DOUBLE_EQ(6765.0, num(result));
}

// Test 6: Tail call in if branches
TEST_F(TCOTest, TailCallInIfBranches) {
    scheme_engine engine(EvaluatorType::BYTECODE);

    std::string code = R"(
        (define (alternating n)
          (if (= n 0)
              "zero"
              (if (= n 1)
                  "one"
                  (alternating (- n 2)))))

        (alternating 100)
    )";

    Object* result = static_cast<Object*>(engine.eval_string(code));
    ASSERT_TRUE(result != nullptr);
    ASSERT_TRUE(is_string(result));
    EXPECT_EQ("zero", static_cast<a_string*>(result)->value());
}

// Test 7: Tail call in begin
TEST_F(TCOTest, TailCallInBegin) {
    scheme_engine engine(EvaluatorType::BYTECODE);

    std::string code = R"(
        (define (loop n acc)
          (begin
            (if (<= n 0)
                acc
                (loop (- n 1) (+ acc n)))))

        (loop 50 0)
    )";

    Object* result = static_cast<Object*>(engine.eval_string(code));
    ASSERT_TRUE(result != nullptr);
    EXPECT_DOUBLE_EQ(1275.0, num(result));
}

// Test 8: Very deep recursion stress test
TEST_F(TCOTest, VeryDeepRecursion50000) {
    scheme_engine engine(EvaluatorType::BYTECODE);

    std::string code = R"(
        (define (deep-recursion n acc)
          (if (<= n 0)
              acc
              (deep-recursion (- n 1) (+ acc 1))))

        (deep-recursion 50000 0)
    )";

    // This should complete without stack overflow
    Object* result = static_cast<Object*>(engine.eval_string(code));
    ASSERT_TRUE(result != nullptr);
    EXPECT_DOUBLE_EQ(50000.0, num(result));
}

// Test 9: Mutual recursion (if supported)
TEST_F(TCOTest, MutualRecursion) {
    scheme_engine engine(EvaluatorType::BYTECODE);

    std::string code = R"(
        (define (is-even n)
          (if (= n 0)
              #t
              (is-odd (- n 1))))

        (define (is-odd n)
          (if (= n 0)
              #f
              (is-even (- n 1))))

        (is-even 100)
    )";

    Object* result = static_cast<Object*>(engine.eval_string(code));
    ASSERT_TRUE(result != nullptr);
    EXPECT_EQ(t, result);  // Should be #t (true)
}

// Test 10: Non-tail call should still work
TEST_F(TCOTest, NonTailCallStillWorks) {
    scheme_engine engine(EvaluatorType::BYTECODE);

    std::string code = R"(
        (define (factorial-non-tail n)
          (if (<= n 1)
              1
              (* n (factorial-non-tail (- n 1)))))

        (factorial-non-tail 5)
    )";

    Object* result = static_cast<Object*>(engine.eval_string(code));
    ASSERT_TRUE(result != nullptr);
    EXPECT_DOUBLE_EQ(120.0, num(result));
}
