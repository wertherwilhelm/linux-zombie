/**
 * @file R7RSCharacterTests.cpp
 * @brief R7RS character Primitive tests
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"

class R7RSCharacterTest : public ZombieSchemeTest {};

// ============================================================================
// char-foldcase Tests
// ============================================================================

TEST_F(R7RSCharacterTest, CharFoldcaseUppercase) {
    expectBool("(char=? (char-foldcase #\\A) #\\a)", true);
    expectBool("(char=? (char-foldcase #\\Z) #\\z)", true);
}

TEST_F(R7RSCharacterTest, CharFoldcaseLowercase) {
    expectBool("(char=? (char-foldcase #\\a) #\\a)", true);
    expectBool("(char=? (char-foldcase #\\z) #\\z)", true);
}

TEST_F(R7RSCharacterTest, CharFoldcaseDigit) {
    expectBool("(char=? (char-foldcase #\\0) #\\0)", true);
    expectBool("(char=? (char-foldcase #\\9) #\\9)", true);
}

TEST_F(R7RSCharacterTest, CharFoldcaseSpecialChars) {
    expectBool("(char=? (char-foldcase #\\space) #\\space)", true);
    expectBool("(char=? (char-foldcase #\\!) #\\!)", true);
    expectBool("(char=? (char-foldcase #\\@) #\\@)", true);
}

TEST_F(R7RSCharacterTest, CharFoldcaseAllUppercase) {
    // Test a few representative uppercase letters
    expectBool("(char=? (char-foldcase #\\B) #\\b)", true);
    expectBool("(char=? (char-foldcase #\\M) #\\m)", true);
    expectBool("(char=? (char-foldcase #\\X) #\\x)", true);
}

TEST_F(R7RSCharacterTest, CharFoldcaseAllLowercase) {
    // Test a few representative lowercase letters
    expectBool("(char=? (char-foldcase #\\c) #\\c)", true);
    expectBool("(char=? (char-foldcase #\\n) #\\n)", true);
    expectBool("(char=? (char-foldcase #\\y) #\\y)", true);
}

TEST_F(R7RSCharacterTest, CharFoldcaseForCaseInsensitiveComparison) {
    // R7RS use case: case-insensitive character comparison
    eval("(define c1 #\\H)");
    eval("(define c2 #\\h)");
    expectBool("(char=? (char-foldcase c1) (char-foldcase c2))", true);
}

TEST_F(R7RSCharacterTest, CharFoldcaseIdempotent) {
    // Folding twice should give same result as folding once
    eval("(define c #\\A)");
    eval("(define folded-once (char-foldcase c))");
    eval("(define folded-twice (char-foldcase folded-once))");
    expectBool("(char=? folded-once folded-twice)", true);
}

TEST_F(R7RSCharacterTest, CharFoldcaseConsistentWithStringFoldcase) {
    // char-foldcase should be consistent with string-foldcase
    eval("(define c #\\X)");
    eval("(define char-result (char-foldcase c))");
    eval("(define str (string c))");
    eval("(define str-result (string-foldcase str))");
    eval("(define str-first-char (string-ref str-result 0))");
    expectBool("(char=? char-result str-first-char)", true);
}

TEST_F(R7RSCharacterTest, CharFoldcaseMixedCases) {
    // Test multiple characters to ensure consistency
    eval(R"(
        (define (test-pair upper lower)
            (char=? (char-foldcase upper) lower))
    )");
    expectBool("(test-pair #\\A #\\a)", true);
    expectBool("(test-pair #\\E #\\e)", true);
    expectBool("(test-pair #\\I #\\i)", true);
    expectBool("(test-pair #\\O #\\o)", true);
    expectBool("(test-pair #\\U #\\u)", true);
}

TEST_F(R7RSCharacterTest, CharFoldcaseNotModifyingOriginal) {
    // Ensure original character is not modified
    eval("(define orig #\\A)");
    eval("(define folded (char-foldcase orig))");
    expectBool("(char=? orig #\\A)", true);  // Original unchanged
    expectBool("(char=? folded #\\a)", true);  // Result is lowercase
}

TEST_F(R7RSCharacterTest, CharFoldcaseWithCharUpcase) {
    // Foldcase undoes upcase for uppercase letters
    eval("(define lower #\\a)");
    eval("(define upper (char-upcase lower))");
    eval("(define back-to-lower (char-foldcase upper))");
    expectBool("(char=? lower back-to-lower)", true);
}

TEST_F(R7RSCharacterTest, CharFoldcaseWithCharDowncase) {
    // Foldcase should give same result as downcase for ASCII
    eval("(define c #\\X)");
    eval("(define folded (char-foldcase c))");
    eval("(define downcased (char-downcase c))");
    expectBool("(char=? folded downcased)", true);
}

// ============================================================================
// Integration Tests with Other Character Operations
// ============================================================================

TEST_F(R7RSCharacterTest, CharFoldcaseWithPredicates) {
    // Folded uppercase letters are still alphabetic
    eval("(define folded (char-foldcase #\\A))");
    expectBool("(char-alphabetic? folded)", true);
    expectBool("(char-lower-case? folded)", true);
    expectBool("(char-upper-case? folded)", false);
}

TEST_F(R7RSCharacterTest, CharFoldcaseWithConversions) {
    // Verify integer conversion works correctly
    eval("(define upper #\\A)");
    eval("(define folded (char-foldcase upper))");
    eval("(define upper-code (char->integer upper))");
    eval("(define folded-code (char->integer folded))");
    // ASCII 'A' is 65, 'a' is 97, difference is 32
    expectInt("(- folded-code upper-code)", 32);
}

TEST_F(R7RSCharacterTest, CharFoldcaseInList) {
    // Use char-foldcase in a list processing context
    eval(R"(
        (define chars '(#\H #\e #\L #\L #\o))
        (define folded-chars (map char-foldcase chars))
        (define result-string (list->string folded-chars))
    )");
    expectString("result-string", "hello");
}

// ============================================================================
// Practical Examples
// ============================================================================

TEST_F(R7RSCharacterTest, CaseInsensitiveCharMatch) {
    eval(R"(
        (define (char-match-ci? c1 c2)
            (char=? (char-foldcase c1) (char-foldcase c2)))
    )");
    expectBool("(char-match-ci? #\\a #\\A)", true);
    expectBool("(char-match-ci? #\\Z #\\z)", true);
    expectBool("(char-match-ci? #\\a #\\b)", false);
}

TEST_F(R7RSCharacterTest, NormalizeCharList) {
    eval(R"(
        (define (normalize-chars chars)
            (map char-foldcase chars))
        (define result (normalize-chars '(#\H #\E #\L #\L #\O)))
    )");
    expectInt("(length result)", 5);
    expectBool("(char=? (car result) #\\h)", true);
    expectBool("(char=? (car (cdr (cdr (cdr (cdr result))))) #\\o)", true);
}

TEST_F(R7RSCharacterTest, CountCaseInsensitive) {
    eval(R"(
        (define (count-char-ci target chars)
            (define folded-target (char-foldcase target))
            (define (matches? c)
                (char=? (char-foldcase c) folded-target))
            (length (filter matches? chars)))
        (define test-chars '(#\a #\A #\b #\A #\c #\a))
        (define count-a (count-char-ci #\a test-chars))
        (define count-b (count-char-ci #\B test-chars))
    )");
    expectInt("count-a", 4);  // 3 A's + 1 a = 4
    expectInt("count-b", 1);  // 1 b
}
