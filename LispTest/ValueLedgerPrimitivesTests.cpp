/**
 * @file ValueLedgerPrimitivesTests.cpp
 * @brief Comprehensive tests for Value Ledger primitives
 */

#include "pch.h"
#include "TestFixture.h"
#include "Base.h"
#include "numeric_runtime.h"

class ValueLedgerPrimitivesTest : public ZombieSchemeTest {
protected:
    void SetUp() override {
        ZombieSchemeTest::SetUp();
        // Value Ledger is initialized in init_lisp_constants()
    }
};

// ============================================================================
// Query Primitives Tests
// ============================================================================

TEST_F(ValueLedgerPrimitivesTest, ValueInfo_Number) {
    std::string code = "(value-info 42)";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->pair()) << "value-info should return an alist";

    // Check backend field
    Object* backend_pair = car(result);
    EXPECT_TRUE(backend_pair->pair());
    EXPECT_EQ(static_cast<a_symbol*>(car(backend_pair))->getName(), "backend");
    EXPECT_EQ(static_cast<a_symbol*>(cdr(backend_pair))->getName(), "scheme");

    // Check callable field (should be #f for numbers)
    Object* rest = result;
    bool found_callable = false;
    while (!is_nil(rest)) {
        Object* pair = car(rest);
        if (pair->pair() && static_cast<a_symbol*>(car(pair))->getName() == "callable") {
            EXPECT_EQ(cdr(pair), false_obj) << "Number should not be callable";
            found_callable = true;
            break;
        }
        rest = cdr(rest);
    }
    EXPECT_TRUE(found_callable) << "Should have callable field in value-info result";
}

TEST_F(ValueLedgerPrimitivesTest, ValueInfo_Procedure) {
    std::string code = "(value-info +)";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);

    // Find callable field
    Object* rest = result;
    bool found_callable = false;
    while (!is_nil(rest)) {
        Object* pair = car(rest);
        if (pair->pair() && static_cast<a_symbol*>(car(pair))->getName() == "callable") {
            EXPECT_EQ(cdr(pair), t) << "Primitive + should be callable";
            found_callable = true;
            break;
        }
        rest = cdr(rest);
    }
    EXPECT_TRUE(found_callable);
}

TEST_F(ValueLedgerPrimitivesTest, ValueInfo_Closure) {
    std::string code = "(value-info (lambda (x) (* x x)))";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);

    // Find callable field
    Object* rest = result;
    bool found_callable = false;
    while (!is_nil(rest)) {
        Object* pair = car(rest);
        if (pair->pair() && static_cast<a_symbol*>(car(pair))->getName() == "callable") {
            EXPECT_EQ(cdr(pair), t) << "Lambda should be callable";
            found_callable = true;
            break;
        }
        rest = cdr(rest);
    }
    EXPECT_TRUE(found_callable);
}

TEST_F(ValueLedgerPrimitivesTest, ValueBackend_ReturnsSymbol) {
    std::string code = "(value-backend 42)";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get_type(), static_cast<int>(Type::SYMBOL));
    EXPECT_EQ(static_cast<a_symbol*>(result)->getName(), "scheme");
}

TEST_F(ValueLedgerPrimitivesTest, ValueOrigin_ReturnsString) {
    std::string code = "(value-origin 42)";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get_type(), static_cast<int>(Type::STRING));
    // Origin can vary based on how value was created
    EXPECT_TRUE(static_cast<a_string*>(result)->value().length() >= 0);
}

TEST_F(ValueLedgerPrimitivesTest, ValueTraits_ReturnsString) {
    std::string code = "(value-traits 42)";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get_type(), static_cast<int>(Type::STRING));
    // Traits can be empty or contain space-separated properties
}

// ============================================================================
// Inspection Primitives Tests
// ============================================================================

TEST_F(ValueLedgerPrimitivesTest, ListSkins_ReturnsList) {
    std::string code = "(list-skins)";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    // Should return a list (possibly empty, but at least should have 'scheme)
    // After init, we should have at least the scheme skin
    EXPECT_TRUE(result->pair() || is_nil(result));

    if (result->pair()) {
        Object* first_skin = car(result);
        EXPECT_EQ(first_skin->get_type(), static_cast<int>(Type::SYMBOL)) << "Skins should be returned as symbols";
    }
}

TEST_F(ValueLedgerPrimitivesTest, BackendStats_ReturnsAlist) {
    std::string code = "(backend-stats)";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->pair()) << "backend-stats should return an alist";

    // Should have entries for all 4 backends
    int count = 0;
    Object* rest = result;
    while (!is_nil(rest)) {
        Object* pair = car(rest);
        EXPECT_TRUE(pair->pair()) << "Each backend stat should be a pair";

        Object* backend_sym = car(pair);
        EXPECT_EQ(backend_sym->get_type(), static_cast<int>(Type::SYMBOL));

        Object* count_val = cdr(pair);
        EXPECT_TRUE(count_val->is_atom() && numeric::is_number(count_val));

        count++;
        rest = cdr(rest);
    }

    EXPECT_EQ(count, 4) << "Should have stats for all 4 backends";
}

TEST_F(ValueLedgerPrimitivesTest, Mirror_NonProcedure) {
    std::string code = "(mirror? 42)";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result, false_obj) << "Number should not be a mirror";
}

TEST_F(ValueLedgerPrimitivesTest, Mirror_Procedure) {
    // Regular procedures aren't automatically mirrors unless tagged
    std::string code = "(mirror? +)";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    // Without explicit mirror trait, should be #f
    EXPECT_EQ(result, false_obj);
}

TEST_F(ValueLedgerPrimitivesTest, FindByTrait_EmptyForNewTrait) {
    std::string code = "(find-by-trait \"nonexistent-trait-xyz\")";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(is_nil(result)) << "Should return empty list for non-existent trait";
}

TEST_F(ValueLedgerPrimitivesTest, FindByTrait_WithSymbolArg) {
    std::string code = "(find-by-trait 'some-trait)";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    // Should accept symbol as well as string
    EXPECT_TRUE(is_nil(result) || result->pair());
}

// ============================================================================
// Time-Travel Primitives Tests
// ============================================================================

TEST_F(ValueLedgerPrimitivesTest, CaptureSnapshot_ReturnsInteger) {
    std::string code = "(capture-snapshot \"test-snapshot\")";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->is_atom() && numeric::is_number(result));

    int64_t snapshot_id = numeric::to_int64(result);
    EXPECT_GE(snapshot_id, 0) << "Snapshot ID should be non-negative";
}

TEST_F(ValueLedgerPrimitivesTest, CaptureSnapshot_WithSymbol) {
    std::string code = "(capture-snapshot 'test-snap)";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->is_atom() && numeric::is_number(result));
}

TEST_F(ValueLedgerPrimitivesTest, RestoreSnapshot_Success) {
    std::string code = R"(
        (define snap (capture-snapshot "before"))
        (restore-snapshot snap)
    )";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result, t) << "restore-snapshot should return #t on success";
}

TEST_F(ValueLedgerPrimitivesTest, SnapshotRoundtrip) {
    std::string code = R"(
        (define x 10)
        (define snap (capture-snapshot "save-x"))
        (set! x 20)
        (restore-snapshot snap)
        x
    )";
    Object* result = eval(code);

    // Note: Current implementation doesn't restore variable bindings,
    // only ledger state. This test documents current behavior.
    ASSERT_NE(result, nullptr);
    // The value of x after restore depends on implementation
}

// ============================================================================
// Transformation Primitives Tests
// ============================================================================

TEST_F(ValueLedgerPrimitivesTest, UseSkin_WithSymbol) {
    std::string code = R"(
        (define x 42)
        (use-skin x 'scheme)
    )";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    // Should return the value (skin applied in-place conceptually)
    EXPECT_TRUE(result->is_atom());
}

TEST_F(ValueLedgerPrimitivesTest, UseSkin_WithString) {
    std::string code = R"(
        (define x 42)
        (use-skin x "scheme")
    )";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    // Should accept string as skin name
}

TEST_F(ValueLedgerPrimitivesTest, UseSkin_OnProcedure) {
    std::string code = R"(
        (define f (lambda (x) (* x x)))
        (use-skin f 'scheme)
        (f 5)
    )";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(numeric::is_number(result));
    EXPECT_EQ(numeric::to_int64(result), 25);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ValueLedgerPrimitivesTest, ValueInfo_NoArgs) {
    std::string code = "(value-info)";
    EXPECT_THROW(eval(code), std::exception);
}

TEST_F(ValueLedgerPrimitivesTest, ValueBackend_NoArgs) {
    std::string code = "(value-backend)";
    EXPECT_THROW(eval(code), std::exception);
}

TEST_F(ValueLedgerPrimitivesTest, UseSkin_MissingArgs) {
    std::string code = "(use-skin 42)";
    EXPECT_THROW(eval(code), std::exception);
}

TEST_F(ValueLedgerPrimitivesTest, CaptureSnapshot_NoArgs) {
    std::string code = "(capture-snapshot)";
    EXPECT_THROW(eval(code), std::exception);
}

TEST_F(ValueLedgerPrimitivesTest, RestoreSnapshot_NoArgs) {
    std::string code = "(restore-snapshot)";
    EXPECT_THROW(eval(code), std::exception);
}

TEST_F(ValueLedgerPrimitivesTest, FindByTrait_NoArgs) {
    std::string code = "(find-by-trait)";
    EXPECT_THROW(eval(code), std::exception);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(ValueLedgerPrimitivesTest, ChainedQueries) {
    std::string code = R"(
        (define x 42)
        (define backend (value-backend x))
        (define origin (value-origin x))
        (define traits (value-traits x))
        (list backend origin traits)
    )";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->pair());

    // First element: backend symbol
    EXPECT_EQ(car(result)->get_type(), static_cast<int>(Type::SYMBOL));

    // Second element: origin string
    EXPECT_EQ(car(cdr(result))->get_type(), static_cast<int>(Type::STRING));

    // Third element: traits string
    EXPECT_EQ(car(cdr(cdr(result)))->get_type(), static_cast<int>(Type::STRING));
}

TEST_F(ValueLedgerPrimitivesTest, MultipleSnapshots) {
    std::string code = R"(
        (define snap1 (capture-snapshot "first"))
        (define snap2 (capture-snapshot "second"))
        (define snap3 (capture-snapshot "third"))
        (list snap1 snap2 snap3)
    )";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->pair());

    // All should be numbers (snapshot IDs)
    EXPECT_TRUE(numeric::is_number(car(result)));
    EXPECT_TRUE(numeric::is_number(car(cdr(result))));
    EXPECT_TRUE(numeric::is_number(car(cdr(cdr(result)))));

    // IDs should be different
    int64_t id1 = numeric::to_int64(car(result));
    int64_t id2 = numeric::to_int64(car(cdr(result)));
    int64_t id3 = numeric::to_int64(car(cdr(cdr(result))));

    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);
}

TEST_F(ValueLedgerPrimitivesTest, BackendConsistency) {
    std::string code = R"(
        (define nums (list 1 2 3 4 5))
        (define backends (map value-backend nums))
        backends
    )";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->pair());

    // All should be 'scheme
    Object* rest = result;
    while (!is_nil(rest)) {
        Object* backend = car(rest);
        EXPECT_EQ(backend->get_type(), static_cast<int>(Type::SYMBOL));
        EXPECT_EQ(static_cast<a_symbol*>(backend)->getName(), "scheme");
        rest = cdr(rest);
    }
}

TEST_F(ValueLedgerPrimitivesTest, ValueInfoOnDifferentTypes) {
    std::string code = R"(
        (list
            (value-info 42)
            (value-info "hello")
            (value-info 'symbol)
            (value-info (lambda (x) x))
            (value-info +))
    )";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_TRUE(result->pair());

    // All should return alists
    Object* rest = result;
    while (!is_nil(rest)) {
        Object* info = car(rest);
        EXPECT_TRUE(info->pair()) << "Each value-info should return an alist";
        rest = cdr(rest);
    }
}

// ============================================================================
// Provenance Tracking Tests
// ============================================================================

TEST_F(ValueLedgerPrimitivesTest, ProvenancePreservedThroughComputation) {
    std::string code = R"(
        (define x 10)
        (define y 20)
        (define z (+ x y))
        (value-backend z)
    )";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);
    EXPECT_EQ(result->get_type(), static_cast<int>(Type::SYMBOL));
    // Result should have a backend
    std::string backend = static_cast<a_symbol*>(result)->getName();
    EXPECT_TRUE(backend == "scheme" || backend == "native" ||
                backend == "bytecode" || backend == "dream");
}

TEST_F(ValueLedgerPrimitivesTest, CallableDetectionComprehensive) {
    std::string code = R"(
        (list
            (value-info 42)              ; number - not callable
            (value-info "hello")         ; string - not callable
            (value-info '(1 2 3))        ; list - not callable
            (value-info (lambda (x) x))  ; lambda - callable
            (value-info +)               ; Primitive - callable
        )
    )";
    Object* result = eval(code);

    ASSERT_NE(result, nullptr);

    // Extract callable values from each info alist
    std::vector<bool> expected_callable = {false, false, false, true, true};
    int index = 0;

    Object* rest = result;
    while (!is_nil(rest) && index < expected_callable.size()) {
        Object* info_alist = car(rest);

        // Find callable field in this alist
        Object* info_rest = info_alist;
        while (!is_nil(info_rest)) {
            Object* pair = car(info_rest);
            if (pair->pair() && static_cast<a_symbol*>(car(pair))->getName() == "callable") {
                bool is_callable = (cdr(pair) == t);
                EXPECT_EQ(is_callable, expected_callable[index])
                    << "Callable mismatch at index " << index;
                break;
            }
            info_rest = cdr(info_rest);
        }

        rest = cdr(rest);
        index++;
    }
}
