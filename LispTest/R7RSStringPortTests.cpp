/**
 * @file R7RSStringPortTests.cpp
 * @brief R7RS string port tests (open-input-string, open-output-string, get-output-string)
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"

class R7RSStringPortTest : public ZombieSchemeTest {};

// ============================================================================
// open-input-string Tests
// ============================================================================

TEST_F(R7RSStringPortTest, OpenInputStringBasic) {
    eval("(define port (open-input-string \"hello world\"))");
    expectBool("(input-port? port)", true);
}

TEST_F(R7RSStringPortTest, ReadFromInputString) {
    eval("(define port (open-input-string \"hello world\"))");
    expectString("(read-line port)", "hello world");
}

TEST_F(R7RSStringPortTest, ReadCharFromInputString) {
    eval("(define port (open-input-string \"abc\"))");
    expectString("(read-string 1 port)", "a");
    expectString("(read-string 1 port)", "b");
    expectString("(read-string 1 port)", "c");
}

TEST_F(R7RSStringPortTest, ReadLineFromInputString) {
    eval("(define port (open-input-string \"line1\\nline2\\nline3\"))");
    expectString("(read-line port)", "line1");
    expectString("(read-line port)", "line2");
    expectString("(read-line port)", "line3");
}

TEST_F(R7RSStringPortTest, ReadStringFromInputString) {
    eval("(define port (open-input-string \"hello world\"))");
    expectString("(read-string 5 port)", "hello");
    expectString("(read-string 6 port)", " world");
}

TEST_F(R7RSStringPortTest, InputStringEOF) {
    eval("(define port (open-input-string \"test\"))");
    eval("(define line1 (read-line port))");
    eval("(define line2 (read-line port))");
    expectString("line1", "test");
    expectBool("(eof-Object? line2)", true);
}

TEST_F(R7RSStringPortTest, EmptyInputString) {
    eval("(define port (open-input-string \"\"))");
    eval("(define result (read-line port))");
    expectBool("(eof-Object? result)", true);
}

// ============================================================================
// open-output-string and get-output-string Tests
// ============================================================================

TEST_F(R7RSStringPortTest, OpenOutputStringBasic) {
    eval("(define port (open-output-string))");
    expectBool("(output-port? port)", true);
}

TEST_F(R7RSStringPortTest, WriteToOutputString) {
    eval(R"(
        (define port (open-output-string))
        (write-string "hello" port)
        (define result (get-output-string port))
    )");
    expectString("result", "hello");
}

TEST_F(R7RSStringPortTest, MultipleWritesToOutputString) {
    eval(R"(
        (define port (open-output-string))
        (write-string "hello" port)
        (write-string " " port)
        (write-string "world" port)
        (define result (get-output-string port))
    )");
    expectString("result", "hello world");
}

TEST_F(R7RSStringPortTest, WriteAndNewlineToOutputString) {
    eval(R"(
        (define port (open-output-string))
        (write-string "line1" port)
        (newline port)
        (write-string "line2" port)
        (newline port)
        (define result (get-output-string port))
    )");
    expectString("result", "line1\nline2\n");
}

TEST_F(R7RSStringPortTest, DisplayToOutputString) {
    eval(R"(
        (define port (open-output-string))
        (display "test" port)
        (define result (get-output-string port))
    )");
    expectString("result", "test");
}

TEST_F(R7RSStringPortTest, WriteCharToOutputString) {
    eval(R"(
        (define port (open-output-string))
        (write-char #\a port)
        (write-char #\b port)
        (write-char #\c port)
        (define result (get-output-string port))
    )");
    expectString("result", "abc");
}

TEST_F(R7RSStringPortTest, MultipleGetsFromOutputString) {
    eval(R"(
        (define port (open-output-string))
        (write-string "first" port)
        (define result1 (get-output-string port))
        (write-string "second" port)
        (define result2 (get-output-string port))
    )");
    expectString("result1", "first");
    expectString("result2", "firstsecond");  // Accumulated output
}

TEST_F(R7RSStringPortTest, EmptyOutputString) {
    eval(R"(
        (define port (open-output-string))
        (define result (get-output-string port))
    )");
    expectString("result", "");
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(R7RSStringPortTest, ReadWriteRoundTrip) {
    eval(R"(
        ; Write to output string port
        (define out (open-output-string))
        (write-string "test data" out)
        (define str (get-output-string out))

        ; Read from input string port
        (define in (open-input-string str))
        (define result (read-line in))
    )");
    expectString("result", "test data");
}

TEST_F(R7RSStringPortTest, ProcessLinesFromString) {
    eval(R"(
        (define port (open-input-string "apple\nbanana\ncherry"))
        (define lines '())
        (define (read-all-lines)
            (define line (read-line port))
            (if (eof-Object? line)
                (reverse lines)
                (begin
                    (set! lines (cons line lines))
                    (read-all-lines))))
        (define all-lines (read-all-lines))
    )");
    expectInt("(length all-lines)", 3);
    expectString("(car all-lines)", "apple");
    expectString("(cadr all-lines)", "banana");
    expectString("(caddr all-lines)", "cherry");
}

TEST_F(R7RSStringPortTest, BuildStringWithLoop) {
    eval(R"(
        (define port (open-output-string))
        (define (write-numbers n)
            (if (> n 0)
                (begin
                    (write-string (number->string n) port)
                    (write-string " " port)
                    (write-numbers (- n 1)))
                #t))
        (write-numbers 5)
        (define result (get-output-string port))
    )");
    expectString("result", "5 4 3 2 1 ");
}

TEST_F(R7RSStringPortTest, StringPortsWithMixedIO) {
    eval(R"(
        ; Create output port and write mixed content
        (define out (open-output-string))
        (display "Number: " out)
        (write-string (number->string 42) out)
        (newline out)
        (display "String: " out)
        (write-string "test" out)

        ; Get the result
        (define result (get-output-string out))
    )");
    expectString("result", "Number: 42\nString: test");
}
