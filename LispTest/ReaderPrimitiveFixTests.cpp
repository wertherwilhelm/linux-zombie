/**
 * @file ReaderPrimitiveFixTests.cpp
 * @brief Regression tests for five reader/IO/number-primitive bugs surfaced by
 *        running a non-trivial Scheme program (an ALGOL/M-expression reader that
 *        drives the tokenizer char-by-char over string ports). Each was a real
 *        interpreter bug, not a bug in that program.
 */

#include "gtest/gtest.h"
#include "zombie_runtime_types.h"
#include "TestFixture.h"
#include <cstdio>

class ReaderPrimitiveFixTest : public ZombieSchemeTest {};

// (1) Delimiter characters are valid single-char literals: #\; #\( #\) were
//     mis-read (';' as a comment, '(' / ')' as parens / "empty character literal").
TEST_F(ReaderPrimitiveFixTest, DelimiterCharLiterals) {
    expectInt("(char->integer #\\;)", 59);
    expectInt("(char->integer #\\()", 40);
    expectInt("(char->integer #\\))", 41);
    expectInt("(char->integer #\\,)", 44);
    // named characters still work
    expectInt("(char->integer #\\space)", 32);
    expectInt("(char->integer #\\newline)", 10);
    expectInt("(char->integer #\\a)", 97);
}

// (2) eof-object? / eof-object exist under the R7RS names (was registered as the
//     mis-cased "eof-Object?", and eof-object was missing entirely).
TEST_F(ReaderPrimitiveFixTest, EofObjectPrimitives) {
    expectBool("(eof-object? (eof-object))", true);
    expectBool("(eof-object? (read-char (open-input-string \"\")))", true);
    expectBool("(eof-object? #\\a)", false);
    expectBool("(procedure? eof-object?)", true);
    expectBool("(procedure? eof-object)", true);
}

// (3) read-char consumes exactly ONE character. It used to call get_line() to
//     "consume the character", swallowing the rest of the line — so the first
//     read-char returned a char and every subsequent one returned EOF.
TEST_F(ReaderPrimitiveFixTest, ReadCharAdvancesOneChar) {
    evalMulti({
        "(define p (open-input-string \"abc\"))",
        "(define a (read-char p))",
        "(define b (read-char p))",
        "(define c (read-char p))",
        "(define d (read-char p))"
    });
    expectInt("(char->integer a)", 'a');
    expectInt("(char->integer b)", 'b');
    expectInt("(char->integer c)", 'c');
    expectBool("(eof-object? d)", true);
    // peek-char does not consume
    evalMulti({
        "(define q (open-input-string \"xy\"))",
        "(define pk (peek-char q))",
        "(define r1 (read-char q))"
    });
    expectInt("(char->integer pk)", 'x');
    expectInt("(char->integer r1)", 'x');
}

// (4) string->number returns #f (not nil) for a non-number, so the common idiom
//     (or (string->number s) default) works. nil is truthy here, so returning it
//     made every non-numeric token read as ().
TEST_F(ReaderPrimitiveFixTest, StringToNumberReturnsFalse) {
    expectBool("(string->number \"square\")", false);
    expectBool("(eq? (string->number \"not-a-number\") #f)", true);
    expectInt("(or (string->number \"abc\") 42)", 42);
    expectInt("(string->number \"7\")", 7);
    expectBool("(symbol? (or (string->number \"foo\") (string->symbol \"foo\")))", true);
}

// (5) load skips ';' line comments (and handles #\; char literals, strings, etc.)
//     — it used a hand-rolled paren-scanner that read comment text as code.
TEST_F(ReaderPrimitiveFixTest, LoadSkipsComments) {
    const char* path = "test_load_comments.scm";
    FILE* f = std::fopen(path, "w");
    ASSERT_NE(f, nullptr);
    std::fprintf(f,
        "; a leading comment\n"
        "(define semi #\\;)   ; a #\\; char literal and a trailing comment\n"
        ";; another comment line\n"
        "(+ 40 2)\n");
    std::fclose(f);

    expectInt("(load \"test_load_comments.scm\")", 42);
    expectInt("(char->integer semi)", 59);
    std::remove(path);
}

// (6) #(...) vector literals: the reader rejected them ("expected '\\' after
//     '#'"), yet vectors print as #(...) and exist as values. Added to the reader.
TEST_F(ReaderPrimitiveFixTest, VectorLiterals) {
    expectInt("(vector-ref '#(10 20 30) 1)", 20);
    expectInt("(vector-length '#(1 2 3 4))", 4);
    expectInt("(vector-length '#())", 0);
    expectString("(symbol->string (vector-ref '#(a b c) 2))", "c");
}

// (7) vector literals self-evaluate (R7RS) — an unquoted #(...) used to throw
//     "Unexpected atomic type".
TEST_F(ReaderPrimitiveFixTest, VectorSelfEvaluates) {
    expectInt("(vector-ref #(5 6 7) 0)", 5);
}

// (8) equal? does element-wise vector comparison (was pointer-only -> #f).
TEST_F(ReaderPrimitiveFixTest, EqualOnVectors) {
    expectBool("(equal? '#(1 2 3) (vector 1 2 3))", true);
    expectBool("(equal? #(1 2) #(1 3))", false);
    expectBool("(equal? #(1 2) #(1 2 3))", false);
    expectBool("(equal? (vector 1 (vector 2 3)) (vector 1 (vector 2 3)))", true);
}
