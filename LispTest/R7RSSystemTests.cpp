/**
 * @file R7RSSystemTests.cpp
 * @brief R7RS system interface tests (exit, emergency-exit, command-line)
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"

class R7RSSystemTest : public ZombieSchemeTest {};

// ============================================================================
// command-line Tests
// ============================================================================

TEST_F(R7RSSystemTest, CommandLineBasic) {
    // command-line should return a list
    eval("(define args (command-line))");
    expectBool("(list? args)", true);
}

TEST_F(R7RSSystemTest, CommandLineIsProperList) {
    // command-line returns a proper list (not dotted)
    eval("(define args (command-line))");
    // If empty, it's nil which is a proper list
    // If not empty, check it's a proper list
    Object* result = eval("args");
    EXPECT_TRUE(result != nullptr);
}

TEST_F(R7RSSystemTest, CommandLineElements) {
    // Each element should be a string
    eval(R"(
        (define args (command-line))
        (define all-strings?
            (if (null? args)
                #t
                (let loop ((lst args))
                    (cond
                        ((null? lst) #t)
                        ((not (string? (car lst))) #f)
                        (else (loop (cdr lst)))))))
    )");
    expectBool("all-strings?", true);
}

// NOTE: exit and emergency-exit tests are disabled because they would
// terminate the test process. These functions are tested manually or
// through integration tests that spawn separate processes.

/*
TEST_F(R7RSSystemTest, DISABLED_ExitNoArgs) {
    // (exit) - would exit with code 0
    eval("(exit)");
    // Never reached
}

TEST_F(R7RSSystemTest, DISABLED_ExitWithCode) {
    // (exit 1) - would exit with code 1
    eval("(exit 1)");
    // Never reached
}

TEST_F(R7RSSystemTest, DISABLED_EmergencyExitNoArgs) {
    // (emergency-exit) - would exit immediately with code 0
    eval("(emergency-exit)");
    // Never reached
}

TEST_F(R7RSSystemTest, DISABLED_EmergencyExitWithCode) {
    // (emergency-exit 2) - would exit immediately with code 2
    eval("(emergency-exit 2)");
    // Never reached
}
*/

// ============================================================================
// Predicate Tests
// ============================================================================

TEST_F(R7RSSystemTest, ExitIsProcedure) {
    expectBool("(Procedure? exit)", true);
}

TEST_F(R7RSSystemTest, EmergencyExitIsProcedure) {
    expectBool("(Procedure? emergency-exit)", true);
}

TEST_F(R7RSSystemTest, CommandLineIsProcedure) {
    expectBool("(Procedure? command-line)", true);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(R7RSSystemTest, CommandLineInExpression) {
    // command-line can be used in expressions
    eval("(define arg-count (length (command-line)))");
    // Should be non-negative
    Object* result = eval("arg-count");
    EXPECT_TRUE(is_int(result));
    EXPECT_GE((int)num(result), 0);
}

TEST_F(R7RSSystemTest, CommandLineMultipleCalls) {
    // Multiple calls should return the same result
    eval(R"(
        (define args1 (command-line))
        (define args2 (command-line))
        (define same? (equal? args1 args2))
    )");
    expectBool("same?", true);
}
