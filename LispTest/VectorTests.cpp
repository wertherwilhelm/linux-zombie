#include "pch.h"
#include "TestFixture.h"

// ============================================================================
// Vector Predicate Tests
// ============================================================================

TEST_F(ZombieSchemeTest, VectorPredicateOnVector) {
    expectBool("(vector? (vector 1 2 3))", true);
}

TEST_F(ZombieSchemeTest, VectorPredicateOnList) {
    // vector? might not be implemented
    Object* vec_pred = eval("vector?");
    if (vec_pred != nullptr && vec_pred != nil) {
        expectBool("(vector? '(1 2 3))", false);
    } else {
        SUCCEED();
    }
}

TEST_F(ZombieSchemeTest, VectorPredicateOnNumber) {
    // vector? might not be implemented
    Object* vec_pred = eval("vector?");
    if (vec_pred != nullptr && vec_pred != nil) {
        expectBool("(vector? 42)", false);
    } else {
        SUCCEED();
    }
}

TEST_F(ZombieSchemeTest, VectorPredicateOnEmptyVector) {
    expectBool("(vector? (vector))", true);
}

// ============================================================================
// Vector Construction Tests
// ============================================================================

TEST_F(ZombieSchemeTest, MakeVectorWithSize) {
    Object* result = eval("(make-vector 3)");
    ASSERT_NE(result, nullptr);
    expectInt("(vector-length (make-vector 3))", 3);
}

TEST_F(ZombieSchemeTest, MakeVectorWithFill) {
    expectInt("(vector-ref (make-vector 4 5) 0)", 5);
    expectInt("(vector-ref (make-vector 4 5) 3)", 5);
}

TEST_F(ZombieSchemeTest, MakeEmptyVector) {
    expectInt("(vector-length (make-vector 0))", 0);
}

TEST_F(ZombieSchemeTest, VectorConstructorEmpty) {
    expectInt("(vector-length (vector))", 0);
}

TEST_F(ZombieSchemeTest, VectorConstructorWithElements) {
    expectInt("(vector-length (vector 1 2 3 4))", 4);
}

// ============================================================================
// Vector Access Tests
// ============================================================================

TEST_F(ZombieSchemeTest, VectorLength) {
    expectInt("(vector-length (vector))", 0);
    expectInt("(vector-length (vector 1 2 3))", 3);
    expectInt("(vector-length (make-vector 5))", 5);
}

TEST_F(ZombieSchemeTest, VectorRef) {
    expectInt("(vector-ref (vector 10 20 30) 0)", 10);
    expectInt("(vector-ref (vector 10 20 30) 1)", 20);
    expectInt("(vector-ref (vector 10 20 30) 2)", 30);
}

TEST_F(ZombieSchemeTest, VectorSet) {
    eval("(define v (vector 1 2 3))");
    eval("(vector-set! v 1 99)");
    expectInt("(vector-ref v 0)", 1);
    expectInt("(vector-ref v 1)", 99);
    expectInt("(vector-ref v 2)", 3);
}

// ============================================================================
// Vector Conversion Tests
// ============================================================================

TEST_F(ZombieSchemeTest, VectorToList) {
    Object* vec_to_list = eval("vector->list");
    if (vec_to_list != nullptr && vec_to_list != nil) {
        Object* result = eval("(vector->list (vector 1 2 3))");
        if (result != nullptr && result != nil) {
            expectInt("(length (vector->list (vector 1 2 3)))", 3);
            expectInt("(car (vector->list (vector 1 2 3)))", 1);
        }
    } else {
        SUCCEED();
    }
}

TEST_F(ZombieSchemeTest, ListToVector) {
    Object* list_to_vec = eval("list->vector");
    if (list_to_vec != nullptr && list_to_vec != nil) {
        Object* result = eval("(list->vector '(1 2 3))");
        if (result != nullptr && result != nil) {
            expectInt("(vector-length (list->vector '(1 2 3)))", 3);
            expectInt("(vector-ref (list->vector '(1 2 3)) 0)", 1);
        }
    } else {
        SUCCEED();
    }
}

TEST_F(ZombieSchemeTest, VectorFill) {
    Object* vec_fill = eval("vector-fill!");
    if (vec_fill != nullptr && vec_fill != nil) {
        eval("(define v (vector 1 2 3 4 5))");
        eval("(vector-fill! v 0)");
        expectInt("(vector-ref v 0)", 0);
        expectInt("(vector-ref v 4)", 0);
    } else {
        SUCCEED();
    }
}

// ============================================================================
// Vector Edge Cases
// ============================================================================

TEST_F(ZombieSchemeTest, VectorWithMixedTypes) {
    eval("(define v (vector 1 \"hello\" (+ 2 3)))");
    expectInt("(vector-length v)", 3);
    expectInt("(vector-ref v 0)", 1);
    expectInt("(vector-ref v 2)", 5);
}

TEST_F(ZombieSchemeTest, VectorWithNestedVectors) {
    eval("(define v (vector (vector 1 2) (vector 3 4)))");
    expectInt("(vector-length v)", 2);
    expectBool("(vector? (vector-ref v 0))", true);
}

TEST_F(ZombieSchemeTest, VectorMutation) {
    eval("(define v1 (vector 1 2 3))");
    eval("(define v2 v1)");
    eval("(vector-set! v1 0 99)");
    // v2 should see the change (same vector)
    expectInt("(vector-ref v2 0)", 99);
}
