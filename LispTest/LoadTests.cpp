#include "pch.h"
#include "TestFixture.h"
#include <fstream>

// ============================================================================
// Load Function Tests
// ============================================================================

class LoadTest : public ZombieSchemeTest {
protected:
    void SetUp() override {
        ZombieSchemeTest::SetUp();
    }

    void TearDown() override {
        // Clean up any test files
        remove("test_load.scm");
        remove("test_load_multi.scm");
        remove("test_load_define.scm");
        ZombieSchemeTest::TearDown();
    }

    // Helper to create a test file with Scheme code
    void createTestFile(const std::string& filename, const std::string& content) {
        std::ofstream file(filename);
        ASSERT_TRUE(file.is_open()) << "Failed to create test file: " << filename;
        file << content;
        file.close();

        // Verify file was created
        std::ifstream check(filename);
        ASSERT_TRUE(check.is_open()) << "Test file not found after creation: " << filename;
        check.close();
    }
};

// ----------------------------------------------------------------------------
// Basic Load Tests
// ----------------------------------------------------------------------------

TEST_F(LoadTest, LoadSimpleExpression) {
    // Create a file with a simple expression
    createTestFile("test_load.scm", "(+ 1 2 3)");

    // Load the file and check the result
    expectInt("(load \"test_load.scm\")", 6);
}

TEST_F(LoadTest, LoadMultipleExpressions) {
    // Create a file with multiple expressions
    createTestFile("test_load_multi.scm",
        "(define x 10)\n"
        "(define y 20)\n"
        "(+ x y)\n");

    // Load should return the result of the last expression
    expectInt("(load \"test_load_multi.scm\")", 30);

    // The definitions should be available in the environment
    expectInt("x", 10);
    expectInt("y", 20);
}

TEST_F(LoadTest, LoadWithDefine) {
    // Create a file that defines a function
    createTestFile("test_load_define.scm",
        "(define (square n) (* n n))\n"
        "(square 5)\n");

    // Load the file
    expectInt("(load \"test_load_define.scm\")", 25);

    // The function should be available
    expectInt("(square 10)", 100);
}

TEST_F(LoadTest, LoadNonExistentFile) {
    // Try to load a file that doesn't exist
    // This should throw an exception
    EXPECT_THROW({
        eval("(load \"nonexistent_file.scm\")");
    }, std::runtime_error);
}

TEST_F(LoadTest, LoadEmptyFile) {
    // Create an empty file
    createTestFile("test_load.scm", "");

    // Load should return nil for an empty file
    Object* result = eval("(load \"test_load.scm\")");
    EXPECT_EQ(result, nil);
}
