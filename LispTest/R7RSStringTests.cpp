/**
 * @file R7RSStringTests.cpp
 * @brief R7RS additional string Primitive tests
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"

class R7RSStringTest : public ZombieSchemeTest {};

// ============================================================================
// string-map Tests
// ============================================================================

TEST_F(R7RSStringTest, StringMapSingleString) {
    eval("(define s (string-map (lambda (c) (integer->char (+ (char->integer c) 1))) \"abc\"))");
    expectString("s", "bcd");
}

TEST_F(R7RSStringTest, StringMapTwoStrings) {
    eval(R"(
        (define s (string-map
                    (lambda (c1 c2) c1)
                    "abc"
                    "xyz"))
    )");
    expectString("s", "abc");
}

TEST_F(R7RSStringTest, StringMapThreeStrings) {
    eval(R"(
        (define s (string-map
                    (lambda (c1 c2 c3) c2)
                    "abc"
                    "xyz"
                    "123"))
    )");
    expectString("s", "xyz");
}

TEST_F(R7RSStringTest, StringMapEmpty) {
    eval("(define s (string-map (lambda (c) c) \"\"))");
    expectString("s", "");
}

TEST_F(R7RSStringTest, StringMapWithBuiltin) {
    eval(R"(
        (define (char-upcase c)
            (if (char>=? c #\a)
                (if (char<=? c #\z)
                    (integer->char (- (char->integer c) 32))
                    c)
                c))
        (define s (string-map char-upcase "hello"))
    )");
    expectString("s", "HELLO");
}

TEST_F(R7RSStringTest, StringMapWithBuiltinDowncase) {
    eval(R"(
        (define (char-downcase c)
            (if (char>=? c #\A)
                (if (char<=? c #\Z)
                    (integer->char (+ (char->integer c) 32))
                    c)
                c))
        (define s (string-map char-downcase "WORLD"))
    )");
    expectString("s", "world");
}

TEST_F(R7RSStringTest, StringMapPreservesLength) {
    eval("(define orig \"testing\")");
    eval("(define mapped (string-map (lambda (c) #\\x) orig))");
    expectInt("(string-length mapped)", 7);
    expectString("mapped", "xxxxxxx");
}

TEST_F(R7RSStringTest, StringMapROT13) {
    eval(R"(
        (define (rot13-char c)
            (if (char>=? c #\a)
                (if (char<=? c #\m)
                    (integer->char (+ (char->integer c) 13))
                    (if (char<=? c #\z)
                        (integer->char (- (char->integer c) 13))
                        c))
                (if (char>=? c #\A)
                    (if (char<=? c #\M)
                        (integer->char (+ (char->integer c) 13))
                        (if (char<=? c #\Z)
                            (integer->char (- (char->integer c) 13))
                            c))
                    c)))
        (define encrypted (string-map rot13-char "hello"))
    )");
    expectString("encrypted", "uryyb");
}

TEST_F(R7RSStringTest, StringMapIdentity) {
    eval("(define s (string-map (lambda (c) c) \"identity\"))");
    expectString("s", "identity");
}

TEST_F(R7RSStringTest, StringMapMultipleIdentical) {
    eval(R"(
        (define s (string-map
                    (lambda (c1 c2 c3) c1)
                    "abc"
                    "abc"
                    "abc"))
    )");
    expectString("s", "abc");
}

// ============================================================================
// string-for-each Tests
// ============================================================================

TEST_F(R7RSStringTest, StringForEachSideEffect) {
    eval("(define count 0)");
    eval("(string-for-each (lambda (c) (set! count (+ count 1))) \"abc\")");
    expectInt("count", 3);
}

TEST_F(R7RSStringTest, StringForEachTwoStrings) {
    eval("(define result '())");
    eval(R"(
        (string-for-each
            (lambda (c1 c2)
                (set! result (cons (list c1 c2) result)))
            "ab"
            "xy")
    )");
    // Result should be '((#\b #\y) (#\a #\x)) in reverse order
    expectInt("(length result)", 2);
}

TEST_F(R7RSStringTest, StringForEachEmpty) {
    eval("(define count 0)");
    eval("(string-for-each (lambda (c) (set! count (+ count 1))) \"\")");
    expectInt("count", 0);
}

TEST_F(R7RSStringTest, StringForEachCountCharacters) {
    eval("(define count 0)");
    eval("(string-for-each (lambda (c) (set! count (+ count 1))) \"testing\")");
    expectInt("count", 7);
}

TEST_F(R7RSStringTest, StringForEachBuildList) {
    eval("(define result '())");
    eval("(string-for-each (lambda (c) (set! result (cons c result))) \"abc\")");
    // List should contain characters in reverse order: (#\c #\b #\a)
    expectInt("(length result)", 3);
}

TEST_F(R7RSStringTest, StringForEachWithIndex) {
    eval(R"(
        (define results (make-vector 3 #\space))
        (string-for-each
            (lambda (c idx-char)
                (define idx (- (char->integer idx-char) (char->integer #\0)))
                (vector-set! results idx c))
            "abc"
            "012")
    )");
    // Verify the characters were placed at correct indices
    eval("(define c0 (vector-ref results 0))");
    eval("(define c1 (vector-ref results 1))");
    eval("(define c2 (vector-ref results 2))");
    expectBool("(char=? c0 #\\a)", true);
    expectBool("(char=? c1 #\\b)", true);
    expectBool("(char=? c2 #\\c)", true);
}

TEST_F(R7RSStringTest, StringForEachAccumulate) {
    eval(R"(
        (define sum 0)
        (string-for-each
            (lambda (c) (set! sum (+ sum (char->integer c))))
            "ABC")
    )");
    // Sum of ASCII values: A=65, B=66, C=67
    expectInt("sum", 198);
}

// ============================================================================
// string-foldcase Tests
// ============================================================================

TEST_F(R7RSStringTest, StringFoldcaseBasicUppercase) {
    expectString("(string-foldcase \"HELLO\")", "hello");
}

TEST_F(R7RSStringTest, StringFoldcaseBasicLowercase) {
    expectString("(string-foldcase \"hello\")", "hello");
}

TEST_F(R7RSStringTest, StringFoldcaseMixedCase) {
    expectString("(string-foldcase \"HeLLo WoRLd\")", "hello world");
}

TEST_F(R7RSStringTest, StringFoldcaseEmpty) {
    expectString("(string-foldcase \"\")", "");
}

TEST_F(R7RSStringTest, StringFoldcaseWithNumbers) {
    expectString("(string-foldcase \"Hello123\")", "hello123");
}

TEST_F(R7RSStringTest, StringFoldcaseWithSpecialChars) {
    expectString("(string-foldcase \"Hello, World!\")", "hello, world!");
}

TEST_F(R7RSStringTest, StringFoldcaseAllUppercase) {
    expectString("(string-foldcase \"ABCDEFGHIJKLMNOPQRSTUVWXYZ\")", "abcdefghijklmnopqrstuvwxyz");
}

TEST_F(R7RSStringTest, StringFoldcaseAllLowercase) {
    expectString("(string-foldcase \"abcdefghijklmnopqrstuvwxyz\")", "abcdefghijklmnopqrstuvwxyz");
}

TEST_F(R7RSStringTest, StringFoldcaseForCaseInsensitiveComparison) {
    // R7RS use case: case-insensitive string comparison
    eval("(define s1 \"Hello\")");
    eval("(define s2 \"HELLO\")");
    eval("(define s3 \"hello\")");
    expectBool("(string=? (string-foldcase s1) (string-foldcase s2))", true);
    expectBool("(string=? (string-foldcase s1) (string-foldcase s3))", true);
    expectBool("(string=? (string-foldcase s2) (string-foldcase s3))", true);
}

TEST_F(R7RSStringTest, StringFoldcasePreservesLength) {
    eval("(define orig \"HeLLo\")");
    eval("(define folded (string-foldcase orig))");
    expectInt("(string-length folded)", 5);
}

TEST_F(R7RSStringTest, StringFoldcaseWithSymbols) {
    expectString("(string-foldcase \"Test@123#ABC\")", "test@123#abc");
}

TEST_F(R7RSStringTest, StringFoldcaseSingleChar) {
    expectString("(string-foldcase \"A\")", "a");
    expectString("(string-foldcase \"z\")", "z");
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(R7RSStringTest, MapAndForEach) {
    eval("(define count 0)");
    eval(R"(
        (define s (string-map
                    (lambda (c) (integer->char (+ (char->integer c) 1)))
                    "abc"))
    )");
    eval("(string-for-each (lambda (c) (set! count (+ count 1))) s)");
    expectString("s", "bcd");
    expectInt("count", 3);
}

TEST_F(R7RSStringTest, ChainedOperations) {
    eval(R"(
        (define original "hello")
        (define upper (string-map
                        (lambda (c)
                            (if (char>=? c #\a)
                                (if (char<=? c #\z)
                                    (integer->char (- (char->integer c) 32))
                                    c)
                                c))
                        original))
        (define count 0)
        (string-for-each (lambda (c) (set! count (+ count 1))) upper)
    )");
    expectString("upper", "HELLO");
    expectInt("count", 5);
}

TEST_F(R7RSStringTest, MapWithMultipleStrings) {
    eval(R"(
        (define s (string-map
                    (lambda (c1 c2)
                        (if (char<? c1 c2) c1 c2))
                    "hello"
                    "world"))
    )");
    // Compare character by character: h<w->h, e<o->e, l=l->l, l<l->l, o<d->d
    expectString("s", "helld");
}

// ============================================================================
// Practical Examples
// ============================================================================

TEST_F(R7RSStringTest, CharacterFrequency) {
    eval(R"(
        (define (count-char str target)
            (define count 0)
            (string-for-each
                (lambda (c)
                    (if (char=? c target)
                        (set! count (+ count 1))))
                str)
            count)
        (define freq (count-char "hello world" #\l))
    )");
    expectInt("freq", 3);
}

TEST_F(R7RSStringTest, StringReverse) {
    eval(R"(
        (define (string-reverse str)
            (define chars '())
            (string-for-each
                (lambda (c) (set! chars (cons c chars)))
                str)
            (list->string chars))
        (define reversed (string-reverse "hello"))
    )");
    expectString("reversed", "olleh");
}

TEST_F(R7RSStringTest, CaesarCipher) {
    eval(R"(
        (define (caesar-char c shift)
            (if (char>=? c #\a)
                (if (char<=? c #\z)
                    (begin
                        (define offset (- (char->integer c) (char->integer #\a)))
                        (define new-offset (modulo (+ offset shift) 26))
                        (integer->char (+ (char->integer #\a) new-offset)))
                    (if (char>=? c #\A)
                        (if (char<=? c #\Z)
                            (begin
                                (define offset (- (char->integer c) (char->integer #\A)))
                                (define new-offset (modulo (+ offset shift) 26))
                                (integer->char (+ (char->integer #\A) new-offset)))
                            c)
                        c))
                (if (char>=? c #\A)
                    (if (char<=? c #\Z)
                        (begin
                            (define offset (- (char->integer c) (char->integer #\A)))
                            (define new-offset (modulo (+ offset shift) 26))
                            (integer->char (+ (char->integer #\A) new-offset)))
                        c)
                    c)))
        (define encrypted (string-map
                            (lambda (c) (caesar-char c 3))
                            "hello"))
    )");
    expectString("encrypted", "khoor");
}

TEST_F(R7RSStringTest, AllUppercase) {
    eval(R"(
        (define (all-uppercase? str)
            (define result #t)
            (string-for-each
                (lambda (c)
                    (if (char>=? c #\a)
                        (if (char<=? c #\z)
                            (set! result #f))))
                str)
            result)
        (define test1 (all-uppercase? "HELLO"))
        (define test2 (all-uppercase? "Hello"))
    )");
    expectBool("test1", true);
    expectBool("test2", false);
}

TEST_F(R7RSStringTest, ZipStrings) {
    eval(R"(
        (define pairs '())
        (string-for-each
            (lambda (c1 c2)
                (set! pairs (cons (cons c1 c2) pairs)))
            "abc"
            "xyz")
    )");
    expectInt("(length pairs)", 3);
    // Pairs should be in reverse: ((#\c . #\z) (#\b . #\y) (#\a . #\x))
}
