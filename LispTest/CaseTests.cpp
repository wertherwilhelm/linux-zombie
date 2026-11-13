#include "pch.h"
#include "TestFixture.h"

// ============================================================================
// Case Expression Tests
// ============================================================================

TEST_F(ZombieSchemeTest, CaseBasic) {
    // Basic case with numeric key
    eval("(define result (case (* 2 3) "
         "  ((2 3 5 7) 'prime) "
         "  ((1 4 6 8 9) 'composite)))");

    Object* result = eval("result");
    ASSERT_NE(result, nullptr);
    a_symbol* sym = dynamic_cast<a_symbol*>(result);
    ASSERT_NE(sym, nullptr);
    EXPECT_EQ(sym->getName(), "composite");
}

TEST_F(ZombieSchemeTest, CaseWithElse) {
    // Case with else clause
    eval("(define result (case 99 "
         "  ((1 2 3) 'small) "
         "  ((10 20 30) 'medium) "
         "  (else 'large)))");

    Object* result = eval("result");
    a_symbol* sym = dynamic_cast<a_symbol*>(result);
    ASSERT_NE(sym, nullptr);
    EXPECT_EQ(sym->getName(), "large");
}

TEST_F(ZombieSchemeTest, CaseWithVariable) {
    // Case with variable key
    eval("(define x 5)");
    eval("(define result (case x "
         "  ((1 2) 'low) "
         "  ((3 4 5) 'mid) "
         "  ((6 7 8) 'high)))");

    Object* result = eval("result");
    a_symbol* sym = dynamic_cast<a_symbol*>(result);
    ASSERT_NE(sym, nullptr);
    EXPECT_EQ(sym->getName(), "mid");
}

TEST_F(ZombieSchemeTest, CaseWithSymbols) {
    // Case matching symbols
    eval("(define result (case 'red "
         "  ((red blue) 'primary-color) "
         "  ((green purple) 'mixed-color)))");

    Object* result = eval("result");
    a_symbol* sym = dynamic_cast<a_symbol*>(result);
    ASSERT_NE(sym, nullptr);
    EXPECT_EQ(sym->getName(), "primary-color");
}

TEST_F(ZombieSchemeTest, CaseMultipleExpressionsInResult) {
    // Case with multiple expressions in result
    expectInt("(case 2 "
              "  ((1) (+ 1 1)) "
              "  ((2) (+ 2 2)) "
              "  ((3) (+ 3 3)))", 4);
}

TEST_F(ZombieSchemeTest, CaseNoMatch) {
    // Case with no matching clause (undefined behavior in R5RS)
    // Most implementations return unspecified value, we return #f
    eval("(define result (case 100 "
         "  ((1 2) 'a) "
         "  ((3 4) 'b)))");

    Object* result = eval("result");
    // Result is unspecified, just check it doesn't crash
    ASSERT_NE(result, nullptr);
}

TEST_F(ZombieSchemeTest, CaseInsideLetStar) {
    // Nesting case inside let*
    expectInt("(let* ((x 3) "
              "       (y (* x 2))) "
              "  (case y "
              "    ((2 4 6 8) 100) "
              "    ((1 3 5 7) 200)))", 100);
}

TEST_F(ZombieSchemeTest, CaseWithComputation) {
    // Case key can be a complex expression
    expectInt("(case (+ 1 2 3) "
              "  ((1 2 3) 10) "
              "  ((4 5 6) 20) "
              "  ((7 8 9) 30))", 20);
}

TEST_F(ZombieSchemeTest, CaseNestedInCond) {
    // Case can be nested in cond
    expectInt("(cond "
              "  ((< 5 3) 10) "
              "  (else (case 2 "
              "          ((1) 100) "
              "          ((2) 200) "
              "          (else 300))))", 200);
}

TEST_F(ZombieSchemeTest, CaseWithStrings) {
    // R5RS: case uses eqv? which doesn't match string content
    // String literals are not eqv? even if they have the same content
    // This test verifies correct R5RS behavior
    eval("(define result (case \"hello\" "
         "  ((\"world\" \"goodbye\") 'farewell) "
         "  ((\"hello\" \"hi\") 'greeting) "
         "  (else 'no-match)))");

    Object* result = eval("result");
    a_symbol* sym = dynamic_cast<a_symbol*>(result);
    ASSERT_NE(sym, nullptr);
    // Should return 'no-match because strings don't match with eqv?
    EXPECT_EQ(sym->getName(), "no-match");
}
