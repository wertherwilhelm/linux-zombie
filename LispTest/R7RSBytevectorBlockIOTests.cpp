/**
 * @file R7RSBytevectorBlockIOTests.cpp
 * @brief R7RS bytevector block I/O tests (read-bytevector, write-bytevector, read-bytevector!)
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"
#include <fstream>

class R7RSBytevectorBlockIOTest : public ZombieSchemeTest {
protected:
    void SetUp() override {
        ZombieSchemeTest::SetUp();
        // Clean up any test files from previous runs
        std::remove("test_bytevector_block_io.bin");
    }

    void TearDown() override {
        // Clean up test files
        std::remove("test_bytevector_block_io.bin");
        std::remove("test_bytevector_block_io_copy.bin");
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
// read-bytevector Tests
// ============================================================================

TEST_F(R7RSBytevectorBlockIOTest, ReadBytevectorBasic) {
    createBinaryFile("test_bytevector_block_io.bin", {10, 20, 30, 40, 50});
    eval(R"(
        (define port (open-input-file "test_bytevector_block_io.bin"))
        (define bv (read-bytevector 3 port))
        (close-input-port port)
    )");

    expectBool("(bytevector? bv)", true);
    expectInt("(bytevector-length bv)", 3);
    expectInt("(bytevector-u8-ref bv 0)", 10);
    expectInt("(bytevector-u8-ref bv 1)", 20);
    expectInt("(bytevector-u8-ref bv 2)", 30);
}

TEST_F(R7RSBytevectorBlockIOTest, ReadBytevectorAll) {
    createBinaryFile("test_bytevector_block_io.bin", {100, 200});
    eval(R"(
        (define port (open-input-file "test_bytevector_block_io.bin"))
        (define bv (read-bytevector 10 port))  ; Request more than available
        (close-input-port port)
    )");

    expectInt("(bytevector-length bv)", 2);
    expectInt("(bytevector-u8-ref bv 0)", 100);
    expectInt("(bytevector-u8-ref bv 1)", 200);
}

TEST_F(R7RSBytevectorBlockIOTest, ReadBytevectorEOF) {
    createBinaryFile("test_bytevector_block_io.bin", {42});
    eval(R"(
        (define port (open-input-file "test_bytevector_block_io.bin"))
        (define bv1 (read-bytevector 1 port))
        (define bv2 (read-bytevector 1 port))  ; Should be EOF
        (close-input-port port)
    )");

    expectInt("(bytevector-length bv1)", 1);
    expectBool("(eof-Object? bv2)", true);
}

TEST_F(R7RSBytevectorBlockIOTest, ReadBytevectorEmpty) {
    createBinaryFile("test_bytevector_block_io.bin", {});
    eval(R"(
        (define port (open-input-file "test_bytevector_block_io.bin"))
        (define bv (read-bytevector 5 port))
        (close-input-port port)
    )");

    expectBool("(eof-Object? bv)", true);
}

TEST_F(R7RSBytevectorBlockIOTest, ReadBytevectorZero) {
    createBinaryFile("test_bytevector_block_io.bin", {1, 2, 3});
    eval(R"(
        (define port (open-input-file "test_bytevector_block_io.bin"))
        (define bv (read-bytevector 0 port))
        (close-input-port port)
    )");

    expectBool("(bytevector? bv)", true);
    expectInt("(bytevector-length bv)", 0);
}

// ============================================================================
// write-bytevector Tests
// ============================================================================

TEST_F(R7RSBytevectorBlockIOTest, WriteBytevectorBasic) {
    eval(R"(
        (define bv (bytevector 65 66 67))
        (define port (open-output-file "test_bytevector_block_io.bin"))
        (write-bytevector bv port)
        (close-output-port port)
    )");

    auto data = readBinaryFile("test_bytevector_block_io.bin");
    ASSERT_EQ(data.size(), 3);
    EXPECT_EQ(data[0], 65);  // 'A'
    EXPECT_EQ(data[1], 66);  // 'B'
    EXPECT_EQ(data[2], 67);  // 'C'
}

TEST_F(R7RSBytevectorBlockIOTest, WriteBytevectorSlice) {
    eval(R"(
        (define bv (bytevector 10 20 30 40 50))
        (define port (open-output-file "test_bytevector_block_io.bin"))
        (write-bytevector bv port 1 4)  ; Write bytes [1,4)
        (close-output-port port)
    )");

    auto data = readBinaryFile("test_bytevector_block_io.bin");
    ASSERT_EQ(data.size(), 3);
    EXPECT_EQ(data[0], 20);
    EXPECT_EQ(data[1], 30);
    EXPECT_EQ(data[2], 40);
}

TEST_F(R7RSBytevectorBlockIOTest, WriteBytevectorEmpty) {
    eval(R"(
        (define bv (bytevector))
        (define port (open-output-file "test_bytevector_block_io.bin"))
        (write-bytevector bv port)
        (close-output-port port)
    )");

    auto data = readBinaryFile("test_bytevector_block_io.bin");
    EXPECT_EQ(data.size(), 0);
}

TEST_F(R7RSBytevectorBlockIOTest, WriteBytevectorToStringPort) {
    eval(R"(
        (define bv (bytevector 72 101 108 108 111))  ; "Hello"
        (define port (open-output-string))
        (write-bytevector bv port)
        (define result (get-output-string port))
    )");

    expectString("result", "Hello");
}

// ============================================================================
// read-bytevector! Tests
// ============================================================================

TEST_F(R7RSBytevectorBlockIOTest, ReadBytevectorBangBasic) {
    createBinaryFile("test_bytevector_block_io.bin", {10, 20, 30, 40, 50});
    eval(R"(
        (define bv (make-bytevector 5 0))
        (define port (open-input-file "test_bytevector_block_io.bin"))
        (define n (read-bytevector! bv port))
        (close-input-port port)
    )");

    expectInt("n", 5);
    expectInt("(bytevector-u8-ref bv 0)", 10);
    expectInt("(bytevector-u8-ref bv 1)", 20);
    expectInt("(bytevector-u8-ref bv 2)", 30);
    expectInt("(bytevector-u8-ref bv 3)", 40);
    expectInt("(bytevector-u8-ref bv 4)", 50);
}

TEST_F(R7RSBytevectorBlockIOTest, ReadBytevectorBangPartial) {
    createBinaryFile("test_bytevector_block_io.bin", {100, 200});
    eval(R"(
        (define bv (make-bytevector 5 0))
        (define port (open-input-file "test_bytevector_block_io.bin"))
        (define n (read-bytevector! bv port))
        (close-input-port port)
    )");

    expectInt("n", 2);  // Only 2 bytes were read
    expectInt("(bytevector-u8-ref bv 0)", 100);
    expectInt("(bytevector-u8-ref bv 1)", 200);
    expectInt("(bytevector-u8-ref bv 2)", 0);  // Unchanged
}

TEST_F(R7RSBytevectorBlockIOTest, ReadBytevectorBangWithRange) {
    createBinaryFile("test_bytevector_block_io.bin", {10, 20, 30, 40, 50});
    eval(R"(
        (define bv (make-bytevector 7 255))
        (define port (open-input-file "test_bytevector_block_io.bin"))
        (define n (read-bytevector! bv port 2 5))  ; Read into bv[2..5)
        (close-input-port port)
    )");

    expectInt("n", 3);  // Read 3 bytes
    expectInt("(bytevector-u8-ref bv 0)", 255);  // Unchanged
    expectInt("(bytevector-u8-ref bv 1)", 255);  // Unchanged
    expectInt("(bytevector-u8-ref bv 2)", 10);   // Read data
    expectInt("(bytevector-u8-ref bv 3)", 20);
    expectInt("(bytevector-u8-ref bv 4)", 30);
    expectInt("(bytevector-u8-ref bv 5)", 255);  // Unchanged
    expectInt("(bytevector-u8-ref bv 6)", 255);  // Unchanged
}

TEST_F(R7RSBytevectorBlockIOTest, ReadBytevectorBangEOF) {
    createBinaryFile("test_bytevector_block_io.bin", {42});
    eval(R"(
        (define bv (make-bytevector 3 0))
        (define port (open-input-file "test_bytevector_block_io.bin"))
        (define n1 (read-bytevector! bv port))
        (define n2 (read-bytevector! bv port))  ; Should be EOF
        (close-input-port port)
    )");

    expectInt("n1", 1);
    expectBool("(eof-Object? n2)", true);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(R7RSBytevectorBlockIOTest, ReadWriteRoundTrip) {
    eval(R"(
        ; Create and write a bytevector
        (define bv1 (bytevector 65 66 67 68 69))
        (define out (open-output-file "test_bytevector_block_io.bin"))
        (write-bytevector bv1 out)
        (close-output-port out)

        ; Read it back
        (define in (open-input-file "test_bytevector_block_io.bin"))
        (define bv2 (read-bytevector 5 in))
        (close-input-port in)
    )");

    expectInt("(bytevector-length bv2)", 5);
    expectInt("(bytevector-u8-ref bv2 0)", 65);
    expectInt("(bytevector-u8-ref bv2 1)", 66);
    expectInt("(bytevector-u8-ref bv2 2)", 67);
    expectInt("(bytevector-u8-ref bv2 3)", 68);
    expectInt("(bytevector-u8-ref bv2 4)", 69);
}

TEST_F(R7RSBytevectorBlockIOTest, CopyFile) {
    createBinaryFile("test_bytevector_block_io.bin", {1, 2, 3, 4, 5});
    eval(R"(
        (define in (open-input-file "test_bytevector_block_io.bin"))
        (define out (open-output-file "test_bytevector_block_io_copy.bin"))
        (define bv (read-bytevector 10 in))  ; Read all
        (write-bytevector bv out)
        (close-input-port in)
        (close-output-port out)
    )");

    auto data = readBinaryFile("test_bytevector_block_io_copy.bin");
    ASSERT_EQ(data.size(), 5);
    EXPECT_EQ(data[0], 1);
    EXPECT_EQ(data[1], 2);
    EXPECT_EQ(data[2], 3);
    EXPECT_EQ(data[3], 4);
    EXPECT_EQ(data[4], 5);
}

TEST_F(R7RSBytevectorBlockIOTest, MultipleReads) {
    createBinaryFile("test_bytevector_block_io.bin", {10, 20, 30, 40, 50, 60});
    eval(R"(
        (define port (open-input-file "test_bytevector_block_io.bin"))
        (define bv1 (read-bytevector 2 port))
        (define bv2 (read-bytevector 2 port))
        (define bv3 (read-bytevector 2 port))
        (close-input-port port)
    )");

    expectInt("(bytevector-u8-ref bv1 0)", 10);
    expectInt("(bytevector-u8-ref bv1 1)", 20);
    expectInt("(bytevector-u8-ref bv2 0)", 30);
    expectInt("(bytevector-u8-ref bv2 1)", 40);
    expectInt("(bytevector-u8-ref bv3 0)", 50);
    expectInt("(bytevector-u8-ref bv3 1)", 60);
}

TEST_F(R7RSBytevectorBlockIOTest, WriteBytevectorMultiple) {
    eval(R"(
        (define port (open-output-file "test_bytevector_block_io.bin"))
        (write-bytevector (bytevector 11 21) port)
        (write-bytevector (bytevector 31 41) port)
        (write-bytevector (bytevector 51 61) port)
        (close-output-port port)
    )");

    auto data = readBinaryFile("test_bytevector_block_io.bin");
    ASSERT_EQ(data.size(), 6);
    EXPECT_EQ(data[0], 11);
    EXPECT_EQ(data[1], 21);
    EXPECT_EQ(data[2], 31);
    EXPECT_EQ(data[3], 41);
    EXPECT_EQ(data[4], 51);
    EXPECT_EQ(data[5], 61);
}
