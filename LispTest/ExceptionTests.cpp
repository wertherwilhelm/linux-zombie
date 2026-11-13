/**
 * @file ExceptionTests.cpp
 * @brief R7RS Exception handling tests (Section 6.11)
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"
#include "a_exception.h"

// DISABLED: Causes test pollution when enabled, affecting tests that run afterward.
// GC safety fixes applied to ALL primitive install() functions (2026-01-23).
// Tests pass in isolation but enabling this suite causes cascading failures
// in ParameterTest and ValueLedgerPrimitivesTest due to cumulative state issues.
class ExceptionTest : public ZombieSchemeTest {};

// ============================================================================
// Exception Object Creation and Predicates
// ============================================================================

TEST_F(ExceptionTest, ErrorObjectPredicateOnException) {
    // Note: 'error' Primitive raises an exception, so we can't easily test it
    // without guard syntax. Instead, create an exception Object directly.
    a_exception* exc = gc_new<a_exception>("test error", nil, a_exception::GENERAL);
    // GC safety: protect exc before allocating string
    gc_root<a_exception> guard_exc(exc);
    gc_root<a_string> name(gc_new<a_string>("test-exc"));
    env->define(name.get(), exc);

    expectBool("(error-Object? test-exc)", true);
}

TEST_F(ExceptionTest, ErrorObjectPredicateOnNonException) {
    expectBool("(error-Object? \"not an error\")", false);
    expectBool("(error-Object? 123)", false);
    expectBool("(error-Object? '(1 2 3))", false);
}

TEST_F(ExceptionTest, FileErrorPredicateExists) {
    EXPECT_TRUE(primitiveExists("file-error?"));
    expectBool("(file-error? \"not an error\")", false);
}

TEST_F(ExceptionTest, ReadErrorPredicateExists) {
    EXPECT_TRUE(primitiveExists("read-error?"));
    expectBool("(read-error? \"not an error\")", false);
}

// ============================================================================
// Exception Primitive Existence
// ============================================================================

TEST_F(ExceptionTest, RaisePrimitiveExists) {
    EXPECT_TRUE(primitiveExists("raise"));
}

TEST_F(ExceptionTest, RaiseContinuablePrimitiveExists) {
    EXPECT_TRUE(primitiveExists("raise-continuable"));
}

TEST_F(ExceptionTest, ErrorPrimitiveExists) {
    EXPECT_TRUE(primitiveExists("error"));
}

TEST_F(ExceptionTest, ErrorObjectMessageExists) {
    EXPECT_TRUE(primitiveExists("error-Object-message"));
}

TEST_F(ExceptionTest, ErrorObjectIrritantsExists) {
    EXPECT_TRUE(primitiveExists("error-Object-irritants"));
}

// ============================================================================
// Exception Object Construction (via C++)
// ============================================================================

TEST_F(ExceptionTest, CreateExceptionObjectDirectly) {
    // Create exception Object directly using C++
    a_exception* exc = gc_new<a_exception>("test message", nil, a_exception::GENERAL);
    gc_root<a_exception> guard_exc(exc);  // GC safety

    EXPECT_TRUE(is_exception(exc));
    EXPECT_FALSE(is_file_exception(exc));
    EXPECT_FALSE(is_read_exception(exc));
    EXPECT_EQ(exc->message(), "test message");
}

TEST_F(ExceptionTest, CreateFileExceptionDirectly) {
    a_exception* exc = gc_new<a_exception>("file error", nil, a_exception::FILE);
    gc_root<a_exception> guard_exc(exc);  // GC safety

    EXPECT_TRUE(is_exception(exc));
    EXPECT_TRUE(is_file_exception(exc));
    EXPECT_FALSE(is_read_exception(exc));
}

TEST_F(ExceptionTest, CreateReadExceptionDirectly) {
    a_exception* exc = gc_new<a_exception>("read error", nil, a_exception::READ);
    gc_root<a_exception> guard_exc(exc);  // GC safety

    EXPECT_TRUE(is_exception(exc));
    EXPECT_FALSE(is_file_exception(exc));
    EXPECT_TRUE(is_read_exception(exc));
}

TEST_F(ExceptionTest, ExceptionWithIrritants) {
    // GC safety: protect allocations during cons and exception creation
    gc_root<a_int> int1(gc_new<a_int>(1));
    gc_root<a_int> int2(gc_new<a_int>(2));
    gc_root<Object> inner_cons(cons(int2.get(), nil));
    Object* irritants = cons(int1.get(), inner_cons.get());
    gc_root<Object> guard_irritants(irritants);
    a_exception* exc = gc_new<a_exception>("test", irritants, a_exception::GENERAL);
    gc_root<a_exception> guard_exc(exc);  // GC safety

    EXPECT_EQ(exc->irritants(), irritants);
}

// ============================================================================
// Exception Predicates via Scheme
// ============================================================================

TEST_F(ExceptionTest, ErrorObjectPredicateViaDefine) {
    // Create exception via C++ and test via Scheme
    a_exception* exc = gc_new<a_exception>("test", nil, a_exception::GENERAL);
    gc_root<a_exception> guard_exc(exc);  // GC safety
    gc_root<a_string> name(gc_new<a_string>("test-exc"));
    env->define(name.get(), exc);

    expectBool("(error-Object? test-exc)", true);
}

TEST_F(ExceptionTest, FileErrorPredicateViaDefine) {
    a_exception* exc = gc_new<a_exception>("file error", nil, a_exception::FILE);
    gc_root<a_exception> guard_exc(exc);  // GC safety
    gc_root<a_string> name(gc_new<a_string>("file-exc"));
    env->define(name.get(), exc);

    expectBool("(file-error? file-exc)", true);
    expectBool("(error-Object? file-exc)", true);
}

TEST_F(ExceptionTest, ReadErrorPredicateViaDefine) {
    a_exception* exc = gc_new<a_exception>("read error", nil, a_exception::READ);
    gc_root<a_exception> guard_exc(exc);  // GC safety
    gc_root<a_string> name(gc_new<a_string>("read-exc"));
    env->define(name.get(), exc);

    expectBool("(read-error? read-exc)", true);
    expectBool("(error-Object? read-exc)", true);
}

// ============================================================================
// Exception Accessors
// ============================================================================

TEST_F(ExceptionTest, ErrorObjectMessage) {
    a_exception* exc = gc_new<a_exception>("my error message", nil, a_exception::GENERAL);
    gc_root<a_exception> guard_exc(exc);  // GC safety
    gc_root<a_string> name(gc_new<a_string>("my-exc"));
    env->define(name.get(), exc);

    expectString("(error-Object-message my-exc)", "my error message");
}

TEST_F(ExceptionTest, ErrorObjectIrritantsEmpty) {
    a_exception* exc = gc_new<a_exception>("test", nil, a_exception::GENERAL);
    gc_root<a_exception> guard_exc(exc);  // GC safety
    gc_root<a_string> name(gc_new<a_string>("exc1"));
    env->define(name.get(), exc);

    expectNil("(error-Object-irritants exc1)");
}

TEST_F(ExceptionTest, ErrorObjectIrritantsWithValues) {
    // GC safety: protect allocations during cons and exception creation
    gc_root<a_int> int_val(gc_new<a_int>(42));
    gc_root<a_string> str_val(gc_new<a_string>("foo"));
    gc_root<Object> inner_cons(cons(str_val.get(), nil));
    Object* irritants = cons(int_val.get(), inner_cons.get());
    gc_root<Object> guard_irritants(irritants);
    a_exception* exc = gc_new<a_exception>("test", irritants, a_exception::GENERAL);
    gc_root<a_exception> guard_exc(exc);  // GC safety
    gc_root<a_string> name(gc_new<a_string>("exc2"));
    env->define(name.get(), exc);

    expectInt("(car (error-Object-irritants exc2))", 42);
    expectString("(car (cdr (error-Object-irritants exc2)))", "foo");
}

// ============================================================================
// Exception Equality
// ============================================================================

TEST_F(ExceptionTest, ExceptionEquality) {
    a_exception* exc1 = gc_new<a_exception>("same message", nil, a_exception::GENERAL);
    gc_root<a_exception> guard_exc1(exc1);  // GC safety: protect before next allocation
    a_exception* exc2 = gc_new<a_exception>("same message", nil, a_exception::GENERAL);

    EXPECT_TRUE(exc1->is_equal(exc2));
}

TEST_F(ExceptionTest, ExceptionInequalityDifferentMessage) {
    a_exception* exc1 = gc_new<a_exception>("message 1", nil, a_exception::GENERAL);
    gc_root<a_exception> guard_exc1(exc1);  // GC safety: protect before next allocation
    a_exception* exc2 = gc_new<a_exception>("message 2", nil, a_exception::GENERAL);

    EXPECT_FALSE(exc1->is_equal(exc2));
}

TEST_F(ExceptionTest, ExceptionInequalityDifferentKind) {
    a_exception* exc1 = gc_new<a_exception>("test", nil, a_exception::GENERAL);
    gc_root<a_exception> guard_exc1(exc1);  // GC safety: protect before next allocation
    a_exception* exc2 = gc_new<a_exception>("test", nil, a_exception::FILE);

    EXPECT_FALSE(exc1->is_equal(exc2));
}

// ============================================================================
// Exception Printing
// ============================================================================

TEST_F(ExceptionTest, ExceptionPrintFormat) {
    a_exception* exc = gc_new<a_exception>("test error", nil, a_exception::GENERAL);
    gc_root<a_exception> guard_exc(exc);  // GC safety

    std::ostringstream os;
    exc->print(os);

    std::string output = os.str();
    EXPECT_TRUE(output.find("test error") != std::string::npos);
    EXPECT_TRUE(output.find("#<error:") != std::string::npos);
}

TEST_F(ExceptionTest, FileErrorPrintFormat) {
    a_exception* exc = gc_new<a_exception>("file error", nil, a_exception::FILE);
    gc_root<a_exception> guard_exc(exc);  // GC safety

    std::ostringstream os;
    exc->print(os);

    std::string output = os.str();
    EXPECT_TRUE(output.find("file-error") != std::string::npos);
}

TEST_F(ExceptionTest, ReadErrorPrintFormat) {
    a_exception* exc = gc_new<a_exception>("read error", nil, a_exception::READ);
    gc_root<a_exception> guard_exc(exc);  // GC safety

    std::ostringstream os;
    exc->print(os);

    std::string output = os.str();
    EXPECT_TRUE(output.find("read-error") != std::string::npos);
}

// ============================================================================
// GC Integration
// ============================================================================

TEST_F(ExceptionTest, ExceptionGCTracing) {
    // GC safety: protect allocations during cons and exception creation
    gc_root<a_int> int1(gc_new<a_int>(1));
    gc_root<a_int> int2(gc_new<a_int>(2));
    gc_root<Object> inner_cons(cons(int2.get(), nil));
    Object* irritants = cons(int1.get(), inner_cons.get());
    gc_root<Object> guard_irritants(irritants);
    a_exception* exc = gc_new<a_exception>("test", irritants, a_exception::GENERAL);
    gc_root<a_exception> guard_exc(exc);  // GC safety

    // Exception should properly trace its irritants
    EXPECT_EQ(exc->irritants(), irritants);

    // Store in environment to keep alive
    gc_root<a_string> name(gc_new<a_string>("gc-exc"));
    env->define(name.get(), exc);

    // Force GC (if we had a manual trigger)
    // The irritants should still be accessible
    EXPECT_EQ(exc->irritants(), irritants);
}

// ============================================================================
// Exception Cloning
// ============================================================================

TEST_F(ExceptionTest, ExceptionClone) {
    // GC safety: protect allocations during cons and exception creation
    gc_root<a_int> int_val(gc_new<a_int>(42));
    Object* irritants = cons(int_val.get(), nil);
    gc_root<Object> guard_irritants(irritants);
    a_exception* exc1 = gc_new<a_exception>("original", irritants, a_exception::GENERAL);
    // Protect exc1 before cloning (which may allocate)
    gc_root<a_exception> guard_exc1(exc1);

    Object* cloned = exc1->clone();
    a_exception* exc2 = static_cast<a_exception*>(cloned);

    EXPECT_EQ(exc2->message(), "original");
    EXPECT_EQ(exc2->kind(), a_exception::GENERAL);
    EXPECT_TRUE(exc1->is_equal(exc2));
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(ExceptionTest, ExceptionInLet) {
    a_exception* exc = gc_new<a_exception>("let test", nil, a_exception::GENERAL);
    gc_root<a_exception> guard_exc(exc);  // GC safety
    gc_root<a_string> name(gc_new<a_string>("let-exc"));
    env->define(name.get(), exc);

    expectBool("(let ((e let-exc)) (error-Object? e))", true);
}

TEST_F(ExceptionTest, ExceptionInLambda) {
    a_exception* exc = gc_new<a_exception>("lambda test", nil, a_exception::GENERAL);
    gc_root<a_exception> guard_exc(exc);  // GC safety
    gc_root<a_string> name(gc_new<a_string>("lambda-exc"));
    env->define(name.get(), exc);

    expectBool("((lambda (e) (error-Object? e)) lambda-exc)", true);
}

TEST_F(ExceptionTest, MultipleExceptionTypes) {
    a_exception* general = gc_new<a_exception>("general", nil, a_exception::GENERAL);
    gc_root<a_exception> guard_general(general);  // GC safety
    a_exception* file = gc_new<a_exception>("file", nil, a_exception::FILE);
    gc_root<a_exception> guard_file(file);  // GC safety
    a_exception* read = gc_new<a_exception>("read", nil, a_exception::READ);
    gc_root<a_exception> guard_read(read);  // GC safety

    gc_root<a_string> name1(gc_new<a_string>("gen-exc"));
    env->define(name1.get(), general);
    gc_root<a_string> name2(gc_new<a_string>("file-exc"));
    env->define(name2.get(), file);
    gc_root<a_string> name3(gc_new<a_string>("read-exc"));
    env->define(name3.get(), read);

    expectBool("(error-Object? gen-exc)", true);
    expectBool("(error-Object? file-exc)", true);
    expectBool("(error-Object? read-exc)", true);

    expectBool("(file-error? gen-exc)", false);
    expectBool("(file-error? file-exc)", true);
    expectBool("(file-error? read-exc)", false);

    expectBool("(read-error? gen-exc)", false);
    expectBool("(read-error? file-exc)", false);
    expectBool("(read-error? read-exc)", true);
}

// ============================================================================
// with-exception-handler / raise / raise-continuable (R7RS 6.11)
// ============================================================================

TEST_F(ExceptionTest, WithExceptionHandlerExists) {
    EXPECT_TRUE(primitiveExists("with-exception-handler"));
}

TEST_F(ExceptionTest, WithExceptionHandlerNoRaise) {
    // Thunk returns normally; handler never runs
    expectInt("(with-exception-handler (lambda (e) 0) (lambda () 5))", 5);
}

TEST_F(ExceptionTest, RaiseContinuableResumesWithHandlerResult) {
    // Canonical R7RS example: handler result resumes at the raise point
    expectInt(
        "(with-exception-handler"
        "  (lambda (e) (+ e 1))"
        "  (lambda () (+ 10 (raise-continuable 5))))",
        16);
}

TEST_F(ExceptionTest, HandlerReceivesRaisedPayload) {
    expectSymbol(
        "(with-exception-handler"
        "  (lambda (e) e)"
        "  (lambda () (raise-continuable 'payload)))",
        "payload");
}

TEST_F(ExceptionTest, NestedHandlersInnerRaisesToOuter) {
    // Inner handler re-raises continuably; outer handler's value flows back
    // through the inner raise to the original raise point
    expectInt(
        "(with-exception-handler"
        "  (lambda (e) 100)"
        "  (lambda ()"
        "    (with-exception-handler"
        "      (lambda (e) (raise-continuable (+ e 1)))"
        "      (lambda () (raise-continuable 1)))))",
        100);
}

TEST_F(ExceptionTest, RaiseHandlerReturningIsAnError) {
    // For non-continuable raise, a handler that returns triggers a
    // secondary exception, which (with no outer handler) unwinds
    EXPECT_THROW(
        eval("(with-exception-handler (lambda (e) 42) (lambda () (raise 'boom)))"),
        std::runtime_error);
}

TEST_F(ExceptionTest, ErrorInvokesHandler) {
    // (error ...) is a raise of an error object, so the handler sees it...
    // but since error is non-continuable, the handler returning is itself an
    // error. Use guard (escaping handler) for the well-behaved version below.
    EXPECT_THROW(
        eval("(with-exception-handler (lambda (e) 1) (lambda () (error \"x\")))"),
        std::runtime_error);
}

TEST_F(ExceptionTest, RaiseWithNoHandlerStillThrows) {
    EXPECT_THROW(eval("(raise 'unhandled)"), std::runtime_error);
}

// ============================================================================
// guard (R7RS 4.2.7)
// ============================================================================

TEST_F(ExceptionTest, GuardCatchesRaise) {
    expectSymbol("(guard (e (#t 'caught)) (raise 'boom))", "caught");
}

TEST_F(ExceptionTest, GuardBindsRaisedObject) {
    expectSymbol("(guard (e ((symbol? e) e)) (raise 'boom))", "boom");
}

TEST_F(ExceptionTest, GuardElseClause) {
    expectInt("(guard (e (else 42)) (error \"anything\"))", 42);
}

TEST_F(ExceptionTest, GuardNoRaiseReturnsBody) {
    expectInt("(guard (e (else 99)) 7)", 7);
}

TEST_F(ExceptionTest, GuardSelectsMatchingClause) {
    expectInt(
        "(guard (e ((string? e) 1)"
        "          ((symbol? e) 2)"
        "          (else 3))"
        "  (raise 'sym))",
        2);
}

TEST_F(ExceptionTest, GuardRethrowsWhenNoClauseMatches) {
    EXPECT_THROW(eval("(guard (e ((string? e) 1)) (raise 'sym))"), std::runtime_error);
}

TEST_F(ExceptionTest, GuardCatchesErrorObjects) {
    expectString(
        "(guard (e ((error-Object? e) (error-Object-message e)))"
        "  (error \"the message\"))",
        "the message");
}

TEST_F(ExceptionTest, GuardCatchesPrimitiveErrors) {
    // C++-side errors (no Scheme raise involved) are also guardable
    expectSymbol("(guard (e (else 'err)) (/ 10 0))", "err");
}

TEST_F(ExceptionTest, GuardNested) {
    // Inner guard rethrows (no match); outer guard catches
    expectSymbol(
        "(guard (outer (else 'from-outer))"
        "  (guard (inner ((string? inner) 'from-inner))"
        "    (raise 'sym)))",
        "from-outer");
}

TEST_F(ExceptionTest, GuardBodyMultipleExpressions) {
    expectInt("(guard (e (else 0)) 1 2 3)", 3);
}

TEST_F(ExceptionTest, GuardWorksWithExceptionHandlerStack) {
    // guard's handler escapes immediately, so it composes with raise
    // from inside nested code
    expectSymbol(
        "(guard (e (else 'escaped))"
        "  ((lambda () (raise 'deep))))",
        "escaped");
}
