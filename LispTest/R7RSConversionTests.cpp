/**
 * @file R7RSConversionTests.cpp
 * @brief R7RS string<->vector conversion tests
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"

class R7RSConversionTest : public ZombieSchemeTest {};

// ============================================================================
// string->vector Tests
// ============================================================================

TEST_F(R7RSConversionTest, StringToVectorBasic) {
    eval("(define v (string->vector \"hello\"))");
    expectInt("(vector-length v)", 5);
    expectBool("(char=? (vector-ref v 0) #\\h)", true);
    expectBool("(char=? (vector-ref v 1) #\\e)", true);
    expectBool("(char=? (vector-ref v 4) #\\o)", true);
}

TEST_F(R7RSConversionTest, StringToVectorEmpty) {
    eval("(define v (string->vector \"\"))");
    expectInt("(vector-length v)", 0);
}

TEST_F(R7RSConversionTest, StringToVectorWithStart) {
    eval("(define v (string->vector \"hello\" 1))");
    expectInt("(vector-length v)", 4);
    expectBool("(char=? (vector-ref v 0) #\\e)", true);
    expectBool("(char=? (vector-ref v 3) #\\o)", true);
}

TEST_F(R7RSConversionTest, StringToVectorWithStartAndEnd) {
    eval("(define v (string->vector \"hello\" 1 4))");
    expectInt("(vector-length v)", 3);
    expectBool("(char=? (vector-ref v 0) #\\e)", true);
    expectBool("(char=? (vector-ref v 1) #\\l)", true);
    expectBool("(char=? (vector-ref v 2) #\\l)", true);
}

TEST_F(R7RSConversionTest, StringToVectorEmptyRange) {
    eval("(define v (string->vector \"hello\" 2 2))");
    expectInt("(vector-length v)", 0);
}

TEST_F(R7RSConversionTest, StringToVectorSingleChar) {
    eval("(define v (string->vector \"x\"))");
    expectInt("(vector-length v)", 1);
    expectBool("(char=? (vector-ref v 0) #\\x)", true);
}

TEST_F(R7RSConversionTest, StringToVectorSpecialChars) {
    eval("(define v (string->vector \"a\\nb\\tc\"))");
    expectInt("(vector-length v)", 5);
    expectBool("(char=? (vector-ref v 0) #\\a)", true);
    expectBool("(char=? (vector-ref v 1) #\\newline)", true);
    expectBool("(char=? (vector-ref v 2) #\\b)", true);
    expectBool("(char=? (vector-ref v 3) #\\tab)", true);
    expectBool("(char=? (vector-ref v 4) #\\c)", true);
}

TEST_F(R7RSConversionTest, StringToVectorDigits) {
    eval("(define v (string->vector \"12345\"))");
    expectInt("(vector-length v)", 5);
    expectBool("(char=? (vector-ref v 0) #\\1)", true);
    expectBool("(char=? (vector-ref v 4) #\\5)", true);
}

// ============================================================================
// vector->string Tests
// ============================================================================

TEST_F(R7RSConversionTest, VectorToStringBasic) {
    eval("(define s (vector->string (vector #\\h #\\e #\\l #\\l #\\o)))");
    expectString("s", "hello");
}

TEST_F(R7RSConversionTest, VectorToStringEmpty) {
    eval("(define s (vector->string (vector)))");
    expectString("s", "");
}

TEST_F(R7RSConversionTest, VectorToStringSingleChar) {
    eval("(define s (vector->string (vector #\\x)))");
    expectString("s", "x");
}

TEST_F(R7RSConversionTest, VectorToStringWithStart) {
    eval("(define v (vector #\\h #\\e #\\l #\\l #\\o))");
    eval("(define s (vector->string v 1))");
    expectString("s", "ello");
}

TEST_F(R7RSConversionTest, VectorToStringWithStartAndEnd) {
    eval("(define v (vector #\\h #\\e #\\l #\\l #\\o))");
    eval("(define s (vector->string v 1 4))");
    expectString("s", "ell");
}

TEST_F(R7RSConversionTest, VectorToStringEmptyRange) {
    eval("(define v (vector #\\h #\\e #\\l #\\l #\\o))");
    eval("(define s (vector->string v 2 2))");
    expectString("s", "");
}

TEST_F(R7RSConversionTest, VectorToStringSpecialChars) {
    eval("(define s (vector->string (vector #\\a #\\newline #\\b #\\tab #\\c)))");
    expectString("s", "a\nb\tc");
}

TEST_F(R7RSConversionTest, VectorToStringDigits) {
    eval("(define s (vector->string (vector #\\1 #\\2 #\\3 #\\4 #\\5)))");
    expectString("s", "12345");
}

TEST_F(R7RSConversionTest, VectorToStringSpaces) {
    eval("(define s (vector->string (vector #\\a #\\space #\\b #\\space #\\c)))");
    expectString("s", "a b c");
}

// ============================================================================
// Round-trip Conversion Tests
// ============================================================================

TEST_F(R7RSConversionTest, RoundTripStringVectorString) {
    eval("(define original \"hello world\")");
    eval("(define vec (string->vector original))");
    eval("(define restored (vector->string vec))");
    expectString("restored", "hello world");
}

TEST_F(R7RSConversionTest, RoundTripVectorStringVector) {
    eval("(define original (vector #\\t #\\e #\\s #\\t))");
    eval("(define str (vector->string original))");
    eval("(define restored (string->vector str))");
    expectInt("(vector-length restored)", 4);
    expectBool("(char=? (vector-ref restored 0) #\\t)", true);
    expectBool("(char=? (vector-ref restored 3) #\\t)", true);
}

TEST_F(R7RSConversionTest, RoundTripWithRange) {
    eval("(define original \"hello\")");
    eval("(define vec (string->vector original 1 4))");
    eval("(define str (vector->string vec))");
    expectString("str", "ell");
}

TEST_F(R7RSConversionTest, RoundTripEmptyString) {
    eval("(define original \"\")");
    eval("(define vec (string->vector original))");
    eval("(define restored (vector->string vec))");
    expectString("restored", "");
}

TEST_F(R7RSConversionTest, RoundTripSpecialChars) {
    eval("(define original \"a\\nb\\tc\")");
    eval("(define vec (string->vector original))");
    eval("(define restored (vector->string vec))");
    expectString("restored", "a\nb\tc");
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(R7RSConversionTest, ConvertAndModify) {
    eval("(define vec (string->vector \"hello\"))");
    eval("(vector-set! vec 0 #\\H)");
    eval("(define str (vector->string vec))");
    expectString("str", "Hello");
}

TEST_F(R7RSConversionTest, ConvertAndMap) {
    eval(R"(
        (define vec (string->vector "abc"))
        (define upper-vec (vector-map
            (lambda (c) (integer->char (- (char->integer c) 32)))
            vec))
        (define result (vector->string upper-vec))
    )");
    expectString("result", "ABC");
}

TEST_F(R7RSConversionTest, ConvertAndFilter) {
    eval(R"(
        (define original "a1b2c3")
        (define vec (string->vector original))
        (define filtered '())
        (vector-for-each
            (lambda (c)
                (if (char>=? c #\a)
                    (if (char<=? c #\z)
                        (set! filtered (cons c filtered)))))
            vec)
        (define result (vector->string (list->vector (reverse filtered))))
    )");
    expectString("result", "abc");
}

TEST_F(R7RSConversionTest, SubstringViaVector) {
    eval("(define original \"hello world\")");
    eval("(define vec (string->vector original 0 5))");
    eval("(define result (vector->string vec))");
    expectString("result", "hello");
}

TEST_F(R7RSConversionTest, ConcatenateViaVector) {
    eval(R"(
        (define vec1 (string->vector "hello"))
        (define vec2 (string->vector " world"))
        (define combined (vector-append vec1 vec2))
        (define result (vector->string combined))
    )");
    expectString("result", "hello world");
}

// ============================================================================
// Practical Examples
// ============================================================================

TEST_F(R7RSConversionTest, ReverseString) {
    eval(R"(
        (define (reverse-string str)
            (define vec (string->vector str))
            (define len (vector-length vec))
            (define reversed (make-vector len))
            (define i 0)
            (vector-for-each
                (lambda (c)
                    (vector-set! reversed (- (- len 1) i) c)
                    (set! i (+ i 1)))
                vec)
            (vector->string reversed))
        (define result (reverse-string "hello"))
    )");
    expectString("result", "olleh");
}

TEST_F(R7RSConversionTest, CharacterCount) {
    eval(R"(
        (define (count-char str target)
            (define vec (string->vector str))
            (define count 0)
            (vector-for-each
                (lambda (c)
                    (if (char=? c target)
                        (set! count (+ count 1))))
                vec)
            count)
        (define result (count-char "hello world" #\l))
    )");
    expectInt("result", 3);
}

TEST_F(R7RSConversionTest, ToUpperCase) {
    eval(R"(
        (define (char-upcase c)
            (if (char>=? c #\a)
                (if (char<=? c #\z)
                    (integer->char (- (char->integer c) 32))
                    c)
                c))
        (define (string-upcase str)
            (define vec (string->vector str))
            (define upper (vector-map char-upcase vec))
            (vector->string upper))
        (define result (string-upcase "hello"))
    )");
    expectString("result", "HELLO");
}

TEST_F(R7RSConversionTest, RotateString) {
    eval(R"(
        (define (rotate-string str n)
            (define len (string-length str))
            (if (= len 0)
                str
                (begin
                    (define offset (modulo n len))
                    (define vec (string->vector str))
                    (define rotated (make-vector len))
                    (define i 0)
                    (vector-for-each
                        (lambda (c)
                            (define new-pos (modulo (+ i offset) len))
                            (vector-set! rotated new-pos c)
                            (set! i (+ i 1)))
                        vec)
                    (vector->string rotated))))
        (define result (rotate-string "abcde" 2))
    )");
    expectString("result", "deabc");
}

TEST_F(R7RSConversionTest, IsPalindrome) {
    eval(R"(
        (define (is-palindrome? str)
            (define vec (string->vector str))
            (define len (vector-length vec))
            (define (check i)
                (if (>= i (quotient len 2))
                    #t
                    (if (char=? (vector-ref vec i)
                                (vector-ref vec (- (- len 1) i)))
                        (check (+ i 1))
                        #f)))
            (check 0))
        (define test1 (is-palindrome? "racecar"))
        (define test2 (is-palindrome? "hello"))
    )");
    expectBool("test1", true);
    expectBool("test2", false);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(R7RSConversionTest, SingleCharacterRoundTrip) {
    eval("(define v (string->vector \"a\"))");
    eval("(define s (vector->string v))");
    expectString("s", "a");
}

TEST_F(R7RSConversionTest, WhitespaceOnly) {
    eval("(define v (string->vector \"   \"))");
    expectInt("(vector-length v)", 3);
    eval("(define s (vector->string v))");
    expectString("s", "   ");
}

TEST_F(R7RSConversionTest, ConvertFullRange) {
    eval("(define original \"test\")");
    eval("(define v (string->vector original 0 4))");
    eval("(define s (vector->string v 0 4))");
    expectString("s", "test");
}
