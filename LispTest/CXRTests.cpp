#include "pch.h"
#include "TestFixture.h"

// ============================================================================
// Basic CXR Accessor Tests
// ============================================================================

TEST_F(ZombieSchemeTest, CadrAccessor) {
    // cadr = (car (cdr list)) - second element
    expectInt("(cadr '(1 2 3 4))", 2);
}

TEST_F(ZombieSchemeTest, CaddrAccessor) {
    // caddr = third element
    expectInt("(caddr '(1 2 3 4))", 3);
}

TEST_F(ZombieSchemeTest, CadddrAccessor) {
    // cadddr = fourth element
    expectInt("(cadddr '(1 2 3 4))", 4);
}

TEST_F(ZombieSchemeTest, CddrAccessor) {
    // cddr = (cdr (cdr list)) - rest after first two
    expectInt("(car (cddr '(1 2 3 4)))", 3);
}

TEST_F(ZombieSchemeTest, CdddrAccessor) {
    // cdddr = rest after first three
    expectInt("(car (cdddr '(1 2 3 4)))", 4);
}

TEST_F(ZombieSchemeTest, CddddrAccessor) {
    // cddddr = rest after first four
    Object* result = eval("(cddddr '(1 2 3 4))");
    ASSERT_EQ(result, nil);  // Should be empty list
}

// ============================================================================
// Nested List CXR Tests
// ============================================================================

TEST_F(ZombieSchemeTest, CaarAccessor) {
    // caar = (car (car list)) - first of first
    expectInt("(caar '((1 2) (3 4)))", 1);
}

TEST_F(ZombieSchemeTest, CdarAccessor) {
    // cdar = (cdr (car list)) - rest of first
    expectInt("(car (cdar '((1 2) (3 4))))", 2);
}

TEST_F(ZombieSchemeTest, CaadrAccessor) {
    // caadr = (car (car (cdr list))) - first of second
    expectInt("(caadr '((1 2) (3 4)))", 3);
}

TEST_F(ZombieSchemeTest, CadadrAccessor) {
    // cadadr = (car (cdr (car (cdr list)))) - second of second
    expectInt("(cadadr '((1 2) (3 4)))", 4);
}

// ============================================================================
// Composition Tests
// ============================================================================

TEST_F(ZombieSchemeTest, CXRComposition1) {
    // Test that caddr = (car (cdr (cdr list)))
    eval("(define lst '(1 2 3 4 5))");
    expectInt("(caddr lst)", 3);
    expectInt("(car (cdr (cdr lst)))", 3);
}

TEST_F(ZombieSchemeTest, CXRComposition2) {
    // Test that cadr = (car (cdr list))
    eval("(define lst '(10 20 30))");
    expectInt("(cadr lst)", 20);
    expectInt("(car (cdr lst))", 20);
}

TEST_F(ZombieSchemeTest, CXRComposition3) {
    // Test nested accessors
    eval("(define nested '((a b) (c d) (e f)))");
    // caar should be same as (car (car nested))
    expectString("(symbol->string (caar nested))", "a");
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, CXROnShortList) {
    // caddr on a 2-element list should fail or return nil
    Object* result = eval("(caddr '(1 2))");
    // Behavior depends on implementation - may be nil or error
}

TEST_F(ZombieSchemeTest, CXRChaining) {
    // Test chaining multiple CXR operations
    eval("(define deep '(((1 2) 3) 4))");
    expectInt("(car (car (car deep)))", 1);
}

TEST_F(ZombieSchemeTest, CXRWithMixedTypes) {
    // CXR should work on lists with mixed types (using list to evaluate expressions)
    eval("(define mixed (list 1 \"hello\" (+ 2 3) (quote symbol)))");
    expectInt("(car mixed)", 1);
    expectString("(cadr mixed)", "hello");
    expectInt("(caddr mixed)", 5);
}

// ============================================================================
// Built-in CXR Functions (if available)
// ============================================================================

TEST_F(ZombieSchemeTest, FirstSecondThird) {
    // Some Schemes provide first, second, third as aliases
    Object* result = eval("(first '(1 2 3))");
    if (result != nullptr && result != nil) {
        expectInt("(first '(1 2 3))", 1);
        expectInt("(second '(1 2 3))", 2);
        expectInt("(third '(1 2 3))", 3);
    }
}

TEST_F(ZombieSchemeTest, RestFunction) {
    // rest is often an alias for cdr
    Object* result = eval("(rest '(1 2 3))");
    if (result != nullptr && result != nil) {
        expectInt("(car (rest '(1 2 3)))", 2);
    }
}

// ============================================================================
// CXR Predicates
// ============================================================================

TEST_F(ZombieSchemeTest, CXRConsistency) {
    // Verify that accessing same element via different paths gives same result
    eval("(define data '((1 2 3) (4 5 6)))");

    // (car (car (cdr data))) should equal (caadr data)
    expectInt("(car (car (cdr data)))", 4);
    expectInt("(caadr data)", 4);
}

TEST_F(ZombieSchemeTest, CXROnQuotedLists) {
    // CXR should work on quoted lists
    expectInt("(cadr (quote (10 20 30)))", 20);
    expectInt("(caddr (quote (100 200 300 400)))", 300);
}
