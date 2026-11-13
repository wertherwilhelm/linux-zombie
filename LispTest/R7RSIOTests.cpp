/**
 * @file R7RSIOTests.cpp
 * @brief R7RS I/O Primitive tests
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"
#include <fstream>

class R7RSIOTest : public ZombieSchemeTest {
protected:
    void SetUp() override {
        ZombieSchemeTest::SetUp();
        // Clean up any test files from previous runs
        std::remove("test_io_read.txt");
        std::remove("test_io_write.txt");
    }

    void TearDown() override {
        // Clean up test files
        std::remove("test_io_read.txt");
        std::remove("test_io_write.txt");
        ZombieSchemeTest::TearDown();
    }

    void createTestFile(const std::string& filename, const std::string& content) {
        std::ofstream file(filename);
        file << content;
        file.close();
    }

    std::string readTestFile(const std::string& filename) {
        std::ifstream file(filename);
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        file.close();
        return content;
    }
};

// ============================================================================
// read-line Tests
// ============================================================================

TEST_F(R7RSIOTest, ReadLineSingleLine) {
    createTestFile("test_io_read.txt", "hello world");
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    expectString("(read-line port)", "hello world");
    eval("(close-input-port port)");
}

TEST_F(R7RSIOTest, ReadLineMultipleLines) {
    createTestFile("test_io_read.txt", "first line\nsecond line\nthird line");
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    expectString("(read-line port)", "first line");
    expectString("(read-line port)", "second line");
    expectString("(read-line port)", "third line");
    eval("(close-input-port port)");
}

TEST_F(R7RSIOTest, ReadLineEOF) {
    createTestFile("test_io_read.txt", "single line");
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    eval("(define line1 (read-line port))");
    eval("(define line2 (read-line port))");  // Should return EOF
    expectString("line1", "single line");
    expectBool("(eof-Object? line2)", true);
    eval("(close-input-port port)");
}

TEST_F(R7RSIOTest, ReadLineEmptyLine) {
    createTestFile("test_io_read.txt", "first\n\nthird");
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    expectString("(read-line port)", "first");
    expectString("(read-line port)", "");  // Empty line
    expectString("(read-line port)", "third");
    eval("(close-input-port port)");
}

TEST_F(R7RSIOTest, ReadLineWithSpaces) {
    createTestFile("test_io_read.txt", "  spaces  at  ends  ");
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    expectString("(read-line port)", "  spaces  at  ends  ");
    eval("(close-input-port port)");
}

// ============================================================================
// read-string Tests
// ============================================================================

TEST_F(R7RSIOTest, ReadStringBasic) {
    createTestFile("test_io_read.txt", "hello world");
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    expectString("(read-string 5 port)", "hello");
    eval("(close-input-port port)");
}

TEST_F(R7RSIOTest, ReadStringFull) {
    createTestFile("test_io_read.txt", "test");
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    expectString("(read-string 4 port)", "test");
    eval("(close-input-port port)");
}

TEST_F(R7RSIOTest, ReadStringPartial) {
    createTestFile("test_io_read.txt", "hello");
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    expectString("(read-string 10 port)", "hello");  // Only 5 chars available
    eval("(close-input-port port)");
}

TEST_F(R7RSIOTest, ReadStringZero) {
    createTestFile("test_io_read.txt", "hello");
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    expectString("(read-string 0 port)", "");
    eval("(close-input-port port)");
}

TEST_F(R7RSIOTest, ReadStringMultiple) {
    createTestFile("test_io_read.txt", "abcdefghij");
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    expectString("(read-string 3 port)", "abc");
    expectString("(read-string 4 port)", "defg");
    expectString("(read-string 3 port)", "hij");
    eval("(close-input-port port)");
}

TEST_F(R7RSIOTest, ReadStringWithNewline) {
    createTestFile("test_io_read.txt", "hello\nworld");
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    expectString("(read-string 11 port)", "hello\nworld");
    eval("(close-input-port port)");
}

TEST_F(R7RSIOTest, ReadStringEOF) {
    createTestFile("test_io_read.txt", "test");
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    eval("(define s1 (read-string 4 port))");  // Read all
    eval("(define s2 (read-string 1 port))");  // Should return EOF
    expectString("s1", "test");
    expectBool("(eof-Object? s2)", true);
    eval("(close-input-port port)");
}

// ============================================================================
// write-string Tests
// ============================================================================

TEST_F(R7RSIOTest, WriteStringBasic) {
    eval("(define port (open-output-file \"test_io_write.txt\"))");
    eval("(write-string \"hello world\" port)");
    eval("(close-output-port port)");
    EXPECT_EQ(readTestFile("test_io_write.txt"), "hello world");
}

TEST_F(R7RSIOTest, WriteStringEmpty) {
    eval("(define port (open-output-file \"test_io_write.txt\"))");
    eval("(write-string \"\" port)");
    eval("(close-output-port port)");
    EXPECT_EQ(readTestFile("test_io_write.txt"), "");
}

TEST_F(R7RSIOTest, WriteStringMultiple) {
    eval("(define port (open-output-file \"test_io_write.txt\"))");
    eval("(write-string \"hello\" port)");
    eval("(write-string \" \" port)");
    eval("(write-string \"world\" port)");
    eval("(close-output-port port)");
    EXPECT_EQ(readTestFile("test_io_write.txt"), "hello world");
}

TEST_F(R7RSIOTest, WriteStringWithNewline) {
    eval("(define port (open-output-file \"test_io_write.txt\"))");
    eval("(write-string \"line1\\nline2\\nline3\" port)");
    eval("(close-output-port port)");
    EXPECT_EQ(readTestFile("test_io_write.txt"), "line1\nline2\nline3");
}

TEST_F(R7RSIOTest, WriteStringSpecialChars) {
    eval("(define port (open-output-file \"test_io_write.txt\"))");
    eval("(write-string \"tab\\there\" port)");
    eval("(close-output-port port)");
    EXPECT_EQ(readTestFile("test_io_write.txt"), "tab\there");
}

// ============================================================================
// flush-output-port Tests
// ============================================================================

TEST_F(R7RSIOTest, FlushOutputPort) {
    eval("(define port (open-output-file \"test_io_write.txt\"))");
    eval("(write-string \"test\" port)");
    eval("(flush-output-port port)");
    eval("(close-output-port port)");
    EXPECT_EQ(readTestFile("test_io_write.txt"), "test");
}

TEST_F(R7RSIOTest, FlushOutputPortMultiple) {
    eval("(define port (open-output-file \"test_io_write.txt\"))");
    eval("(write-string \"part1\" port)");
    eval("(flush-output-port port)");
    eval("(write-string \"part2\" port)");
    eval("(flush-output-port port)");
    eval("(close-output-port port)");
    EXPECT_EQ(readTestFile("test_io_write.txt"), "part1part2");
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(R7RSIOTest, ReadWriteRoundTrip) {
    // Write data
    eval("(define out-port (open-output-file \"test_io_write.txt\"))");
    eval("(write-string \"test data\" out-port)");
    eval("(close-output-port out-port)");

    // Read it back
    eval("(define in-port (open-input-file \"test_io_write.txt\"))");
    expectString("(read-string 9 in-port)", "test data");
    eval("(close-input-port in-port)");
}

TEST_F(R7RSIOTest, ReadLineAndProcess) {
    createTestFile("test_io_read.txt", "apple\nbanana\ncherry");
    eval(R"(
        (define port (open-input-file "test_io_read.txt"))
        (define lines '())
        (define (read-all-lines)
            (define line (read-line port))
            (if (eof-Object? line)
                (reverse lines)
                (begin
                    (set! lines (cons line lines))
                    (read-all-lines))))
        (define all-lines (read-all-lines))
        (close-input-port port)
    )");
    expectInt("(length all-lines)", 3);
    expectString("(car all-lines)", "apple");
    expectString("(cadr all-lines)", "banana");
    expectString("(caddr all-lines)", "cherry");
}

TEST_F(R7RSIOTest, WriteMultipleLinesWithNewline) {
    eval(R"(
        (define port (open-output-file "test_io_write.txt"))
        (write-string "first" port)
        (newline port)
        (write-string "second" port)
        (newline port)
        (write-string "third" port)
        (newline port)
        (close-output-port port)
    )");
    EXPECT_EQ(readTestFile("test_io_write.txt"), "first\nsecond\nthird\n");
}

TEST_F(R7RSIOTest, ReadStringChunks) {
    createTestFile("test_io_read.txt", "0123456789");
    eval(R"(
        (define port (open-input-file "test_io_read.txt"))
        (define chunks '())
        (define (read-chunks)
            (define chunk (read-string 3 port))
            (if (eof-Object? chunk)
                (reverse chunks)
                (begin
                    (set! chunks (cons chunk chunks))
                    (read-chunks))))
        (define all-chunks (read-chunks))
        (close-input-port port)
    )");
    expectInt("(length all-chunks)", 4);
    expectString("(car all-chunks)", "012");
    expectString("(cadr all-chunks)", "345");
    expectString("(caddr all-chunks)", "678");
    expectString("(car (cdddr all-chunks))", "9");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(R7RSIOTest, ReadLineFromEmptyFile) {
    createTestFile("test_io_read.txt", "");
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    eval("(define line (read-line port))");
    expectBool("(eof-Object? line)", true);
    eval("(close-input-port port)");
}

TEST_F(R7RSIOTest, ReadStringFromEmptyFile) {
    createTestFile("test_io_read.txt", "");
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    eval("(define str (read-string 10 port))");
    expectBool("(eof-Object? str)", true);
    eval("(close-input-port port)");
}

TEST_F(R7RSIOTest, WriteStringToSameFile) {
    eval(R"(
        (define out1 (open-output-file "test_io_write.txt"))
        (write-string "first" out1)
        (close-output-port out1)
        (define out2 (open-output-file "test_io_write.txt"))
        (write-string "second" out2)
        (close-output-port out2)
    )");
    // Second write should overwrite the first
    EXPECT_EQ(readTestFile("test_io_write.txt"), "second");
}

TEST_F(R7RSIOTest, ReadLineLongLine) {
    std::string long_line(1000, 'x');
    createTestFile("test_io_read.txt", long_line);
    eval("(define port (open-input-file \"test_io_read.txt\"))");
    eval("(define line (read-line port))");
    expectInt("(string-length line)", 1000);
    eval("(close-input-port port)");
}
