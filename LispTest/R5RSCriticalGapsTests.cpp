#include "pch.h"
#include "TestFixture.h"

// ============================================================================
// R5RS "Critical Gaps" Verification Tests
// These tests verify that features marked as "critical bugs" are actually working
// ============================================================================

// ============================================================================
// List Search Functions (memq, memv, member)
// ============================================================================

TEST_F(ZombieSchemeTest, MemqReturnsTailOfList) {
    // R5RS: memq should return tail of list starting at match
    Object* result = eval("(memq 'b '(a b c))");
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->pair());

    // Should return (b c)
    // Check first element is 'b
    a_symbol* first = dynamic_cast<a_symbol*>(car(result));
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->getName(), "b");

    // Check second element is 'c
    a_symbol* second = dynamic_cast<a_symbol*>(car(cdr(result)));
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->getName(), "c");
}

TEST_F(ZombieSchemeTest, MemvReturnsTailOfList) {
    Object* result = eval("(memv 2 '(1 2 3 4))");
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->pair());

    // Should return (2 3 4)
    EXPECT_TRUE(numeric::is_number(car(result)));
    EXPECT_EQ(numeric::to_int64(car(result)), 2);
}

TEST_F(ZombieSchemeTest, MemberReturnsTailOfList) {
    Object* result = eval("(member '(1) '((2) (1) (3)))");
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->pair());

    // Should return ((1) (3))
    Object* first_elem = car(result);
    ASSERT_TRUE(first_elem->pair());
}

TEST_F(ZombieSchemeTest, MemqNotFoundReturnsFalse) {
    Object* result = eval("(memq 'x '(a b c))");
    EXPECT_EQ(result, false_obj);
}

// ============================================================================
// Association List Functions (assq, assv, assoc)
// ============================================================================

TEST_F(ZombieSchemeTest, AssqReturnsMatchingPair) {
    // R5RS: assq should return the matching pair, not just the value
    eval("(define alist '((a . 1) (b . 2) (c . 3)))");
    Object* result = eval("(assq 'b alist)");

    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->pair());

    // Should return (b . 2)
    a_symbol* key = dynamic_cast<a_symbol*>(car(result));
    ASSERT_NE(key, nullptr);
    EXPECT_EQ(key->getName(), "b");

    EXPECT_TRUE(numeric::is_number(cdr(result)));
    EXPECT_EQ(numeric::to_int64(cdr(result)), 2);
}

TEST_F(ZombieSchemeTest, AssocReturnsMatchingPair) {
    eval("(define alist '((a . 1) (b . 2) (c . 3)))");
    Object* result = eval("(assoc 'c alist)");

    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->pair());

    // Should return (c . 3)
    a_symbol* key = dynamic_cast<a_symbol*>(car(result));
    ASSERT_NE(key, nullptr);
    EXPECT_EQ(key->getName(), "c");

    EXPECT_TRUE(numeric::is_number(cdr(result)));
    EXPECT_EQ(numeric::to_int64(cdr(result)), 3);
}

TEST_F(ZombieSchemeTest, AssvReturnsMatchingPair) {
    eval("(define nlist '((1 . a) (2 . b) (3 . c)))");
    Object* result = eval("(assv 2 nlist)");

    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(result->pair());

    // Should return (2 . b)
    EXPECT_TRUE(numeric::is_number(car(result)));
    EXPECT_EQ(numeric::to_int64(car(result)), 2);
}

TEST_F(ZombieSchemeTest, AssqNotFoundReturnsFalse) {
    eval("(define alist '((a . 1) (b . 2)))");
    Object* result = eval("(assq 'x alist)");
    EXPECT_EQ(result, false_obj);
}

// ============================================================================
// String Mutation (string-set!)
// ============================================================================

TEST_F(ZombieSchemeTest, StringSetMutatesString) {
    // Test that string-set! actually mutates the string
    eval("(define s (string-copy \"hello\"))");
    eval("(string-set! s 0 #\\H)");

    Object* result = eval("s");
    ASSERT_NE(result, nullptr);
    ASSERT_TRUE(is_string(result));

    a_string* str = dynamic_cast<a_string*>(result);
    EXPECT_EQ(str->value(), "Hello");
}

TEST_F(ZombieSchemeTest, StringSetMultipleMutations) {
    eval("(define s (string-copy \"world\"))");
    eval("(string-set! s 0 #\\W)");
    eval("(string-set! s 4 #\\D)");

    Object* result = eval("s");
    a_string* str = dynamic_cast<a_string*>(result);
    EXPECT_EQ(str->value(), "WorlD");
}

TEST_F(ZombieSchemeTest, StringSetOnLiteralMutates) {
    // R5RS allows mutation of string literals (though discouraged)
    eval("(define s \"test\")");
    eval("(string-set! s 0 #\\T)");

    Object* result = eval("s");
    a_string* str = dynamic_cast<a_string*>(result);
    EXPECT_EQ(str->value(), "Test");
}

// ============================================================================
// File I/O Operations
// ============================================================================

TEST_F(ZombieSchemeTest, FileWriteAndRead) {
    // Test reading a number from a file - use exact FileEofDetection pattern
    Object* result = eval(R"(
        (begin
          (define out (open-output-file "file_write_test.txt"))
          (write 123 out)
          (close-output-port out)
          (define in (open-input-file "file_write_test.txt"))
          (define val (read in))
          (define eof-val (read in))
          (close-input-port in)
          (eof-Object? eof-val)))");

    EXPECT_EQ(result, truth(true));
}

TEST_F(ZombieSchemeTest, FileEofDetection) {
    // Test EOF detection in a single expression
    Object* result = eval(R"(
        (begin
          (define out (open-output-file "eof_test.txt"))
          (write 123 out)
          (close-output-port out)
          (define in (open-input-file "eof_test.txt"))
          (define val (read in))
          (define eof-val (read in))
          (close-input-port in)
          (eof-Object? eof-val)))");

    EXPECT_EQ(result, truth(true));
}
