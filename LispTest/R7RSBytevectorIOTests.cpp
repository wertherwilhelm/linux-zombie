/**
 * @file R7RSBytevectorIOTests.cpp
 * @brief R7RS bytevector I/O tests (read-u8, write-u8, peek-u8, u8-ready?)
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"
#include <fstream>

class R7RSBytevectorIOTest : public ZombieSchemeTest {
protected:
    void SetUp() override {
        ZombieSchemeTest::SetUp();
        // Clean up any test files from previous runs
        std::remove("test_bytevector_io.bin");
    }

    void TearDown() override {
        // Clean up test files
        std::remove("test_bytevector_io.bin");
        ZombieSchemeTest::TearDown();
    }

    void createBinaryFile(const std::string& filename, const std::vector<unsigned char>& data) {
        std::ofstream file(filename, std::ios::binary);
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
    }

    std::vector<unsigned char> readBinaryFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        std::vector<unsigned char> data(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );
        file.close();
        return data;
    }
};

// ============================================================================
// read-u8 Tests
// ============================================================================

TEST_F(R7RSBytevectorIOTest, ReadU8Basic) {
    createBinaryFile("test_bytevector_io.bin", {0, 127, 255});
    eval("(define port (open-input-file \"test_bytevector_io.bin\"))");
    expectInt("(read-u8 port)", 0);
    expectInt("(read-u8 port)", 127);
    expectInt("(read-u8 port)", 255);
    eval("(close-input-port port)");
}

TEST_F(R7RSBytevectorIOTest, ReadU8EOF) {
    createBinaryFile("test_bytevector_io.bin", {42});
    eval("(define port (open-input-file \"test_bytevector_io.bin\"))");
    eval("(define byte1 (read-u8 port))");
    eval("(define byte2 (read-u8 port))");
    expectInt("byte1", 42);
    expectBool("(eof-Object? byte2)", true);
    eval("(close-input-port port)");
}

TEST_F(R7RSBytevectorIOTest, ReadU8EmptyFile) {
    createBinaryFile("test_bytevector_io.bin", {});
    eval("(define port (open-input-file \"test_bytevector_io.bin\"))");
    eval("(define byte (read-u8 port))");
    expectBool("(eof-Object? byte)", true);
    eval("(close-input-port port)");
}

TEST_F(R7RSBytevectorIOTest, ReadU8Multiple) {
    createBinaryFile("test_bytevector_io.bin", {1, 2, 3, 4, 5});
    eval("(define port (open-input-file \"test_bytevector_io.bin\"))");
    expectInt("(read-u8 port)", 1);
    expectInt("(read-u8 port)", 2);
    expectInt("(read-u8 port)", 3);
    expectInt("(read-u8 port)", 4);
    expectInt("(read-u8 port)", 5);
    eval("(close-input-port port)");
}

TEST_F(R7RSBytevectorIOTest, ReadU8FromStringPort) {
    // String ports can also be used with read-u8
    eval("(define port (open-input-string \"ABC\"))");
    expectInt("(read-u8 port)", 65);  // 'A'
    expectInt("(read-u8 port)", 66);  // 'B'
    expectInt("(read-u8 port)", 67);  // 'C'
}

// ============================================================================
// peek-u8 Tests
// ============================================================================

TEST_F(R7RSBytevectorIOTest, PeekU8Basic) {
    createBinaryFile("test_bytevector_io.bin", {42, 100});
    eval("(define port (open-input-file \"test_bytevector_io.bin\"))");
    expectInt("(peek-u8 port)", 42);  // Peek doesn't consume
    expectInt("(peek-u8 port)", 42);  // Still the same
    expectInt("(read-u8 port)", 42);  // Now consume
    expectInt("(peek-u8 port)", 100); // Next byte
    eval("(close-input-port port)");
}

TEST_F(R7RSBytevectorIOTest, PeekU8EOF) {
    createBinaryFile("test_bytevector_io.bin", {});
    eval("(define port (open-input-file \"test_bytevector_io.bin\"))");
    eval("(define byte (peek-u8 port))");
    expectBool("(eof-Object? byte)", true);
    eval("(close-input-port port)");
}

TEST_F(R7RSBytevectorIOTest, PeekU8BeforeRead) {
    createBinaryFile("test_bytevector_io.bin", {10, 20, 30});
    eval(R"(
        (define port (open-input-file "test_bytevector_io.bin"))
        (define peek1 (peek-u8 port))
        (define read1 (read-u8 port))
        (define peek2 (peek-u8 port))
        (define read2 (read-u8 port))
    )");
    expectInt("peek1", 10);
    expectInt("read1", 10);
    expectInt("peek2", 20);
    expectInt("read2", 20);
    eval("(close-input-port port)");
}

// ============================================================================
// u8-ready? Tests
// ============================================================================

TEST_F(R7RSBytevectorIOTest, U8ReadyBasic) {
    createBinaryFile("test_bytevector_io.bin", {42});
    eval("(define port (open-input-file \"test_bytevector_io.bin\"))");
    expectBool("(u8-ready? port)", true);
    eval("(close-input-port port)");
}

TEST_F(R7RSBytevectorIOTest, U8ReadyAfterRead) {
    createBinaryFile("test_bytevector_io.bin", {1, 2});
    eval("(define port (open-input-file \"test_bytevector_io.bin\"))");
    expectBool("(u8-ready? port)", true);
    eval("(read-u8 port)");
    expectBool("(u8-ready? port)", true);  // Still ready for next byte
    eval("(close-input-port port)");
}

TEST_F(R7RSBytevectorIOTest, U8ReadyAtEOF) {
    createBinaryFile("test_bytevector_io.bin", {42});
    eval("(define port (open-input-file \"test_bytevector_io.bin\"))");
    eval("(read-u8 port)");  // Consume the only byte
    expectBool("(u8-ready? port)", true);  // Returns true even at EOF
    eval("(close-input-port port)");
}

// ============================================================================
// write-u8 Tests
// ============================================================================

TEST_F(R7RSBytevectorIOTest, WriteU8Basic) {
    eval("(define port (open-output-file \"test_bytevector_io.bin\"))");
    eval("(write-u8 42 port)");
    eval("(close-output-port port)");

    auto data = readBinaryFile("test_bytevector_io.bin");
    ASSERT_EQ(data.size(), 1);
    EXPECT_EQ(data[0], 42);
}

TEST_F(R7RSBytevectorIOTest, WriteU8Multiple) {
    eval(R"(
        (define port (open-output-file "test_bytevector_io.bin"))
        (write-u8 0 port)
        (write-u8 127 port)
        (write-u8 255 port)
        (close-output-port port)
    )");

    auto data = readBinaryFile("test_bytevector_io.bin");
    ASSERT_EQ(data.size(), 3);
    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[1], 127);
    EXPECT_EQ(data[2], 255);
}

TEST_F(R7RSBytevectorIOTest, WriteU8Sequence) {
    eval(R"(
        (define port (open-output-file "test_bytevector_io.bin"))
        (define (write-bytes lst)
            (if (null? lst)
                #t
                (begin
                    (write-u8 (car lst) port)
                    (write-bytes (cdr lst)))))
        (write-bytes '(65 66 67 68 69))
        (close-output-port port)
    )");

    auto data = readBinaryFile("test_bytevector_io.bin");
    ASSERT_EQ(data.size(), 5);
    EXPECT_EQ(data[0], 65);  // 'A'
    EXPECT_EQ(data[1], 66);  // 'B'
    EXPECT_EQ(data[2], 67);  // 'C'
    EXPECT_EQ(data[3], 68);  // 'D'
    EXPECT_EQ(data[4], 69);  // 'E'
}

TEST_F(R7RSBytevectorIOTest, WriteU8ToStringPort) {
    // Writing bytes to string output port
    eval(R"(
        (define port (open-output-string))
        (write-u8 65 port)
        (write-u8 66 port)
        (write-u8 67 port)
        (define result (get-output-string port))
    )");
    expectString("result", "ABC");
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(R7RSBytevectorIOTest, ReadWriteRoundTrip) {
    eval(R"(
        ; Write bytes
        (define out (open-output-file "test_bytevector_io.bin"))
        (write-u8 10 out)
        (write-u8 20 out)
        (write-u8 30 out)
        (close-output-port out)

        ; Read them back
        (define in (open-input-file "test_bytevector_io.bin"))
        (define b1 (read-u8 in))
        (define b2 (read-u8 in))
        (define b3 (read-u8 in))
        (close-input-port in)
    )");
    expectInt("b1", 10);
    expectInt("b2", 20);
    expectInt("b3", 30);
}

TEST_F(R7RSBytevectorIOTest, PeekAndRead) {
    createBinaryFile("test_bytevector_io.bin", {5, 10, 15});
    eval(R"(
        (define port (open-input-file "test_bytevector_io.bin"))
        (define p1 (peek-u8 port))
        (define r1 (read-u8 port))
        (define p2 (peek-u8 port))
        (define r2 (read-u8 port))
        (define same? (= p1 r1))
        (close-input-port port)
    )");
    expectInt("p1", 5);
    expectInt("r1", 5);
    expectInt("p2", 10);
    expectInt("r2", 10);
    expectBool("same?", true);
}

TEST_F(R7RSBytevectorIOTest, CopyFileByte) {
    createBinaryFile("test_bytevector_io.bin", {100, 200, 50});
    eval(R"(
        (define in (open-input-file "test_bytevector_io.bin"))
        (define out (open-output-file "test_bytevector_io_copy.bin"))

        (define (copy-bytes)
            (define byte (read-u8 in))
            (if (eof-Object? byte)
                #t
                (begin
                    (write-u8 byte out)
                    (copy-bytes))))

        (copy-bytes)
        (close-input-port in)
        (close-output-port out)
    )");

    auto data = readBinaryFile("test_bytevector_io_copy.bin");
    ASSERT_EQ(data.size(), 3);
    EXPECT_EQ(data[0], 100);
    EXPECT_EQ(data[1], 200);
    EXPECT_EQ(data[2], 50);
    std::remove("test_bytevector_io_copy.bin");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(R7RSBytevectorIOTest, WriteU8BoundaryValues) {
    eval(R"(
        (define port (open-output-file "test_bytevector_io.bin"))
        (write-u8 0 port)     ; Minimum
        (write-u8 255 port)   ; Maximum
        (close-output-port port)
    )");

    auto data = readBinaryFile("test_bytevector_io.bin");
    ASSERT_EQ(data.size(), 2);
    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[1], 255);
}

TEST_F(R7RSBytevectorIOTest, ReadU8BoundaryBytes) {
    // Test reading boundary byte values (0, 127, 255)
    createBinaryFile("test_bytevector_io.bin", {0, 127, 255});

    eval(R"(
        (define port (open-input-file "test_bytevector_io.bin"))
        (define byte-0 (read-u8 port))
        (define byte-127 (read-u8 port))
        (define byte-255 (read-u8 port))
        (close-input-port port)
    )");

    expectInt("byte-0", 0);
    expectInt("byte-127", 127);
    expectInt("byte-255", 255);
}
