/**
 * @file ParameterTests.cpp
 * @brief R7RS Parameter tests (Section 4.2.6)
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"
#include "a_parameter.h"

class ParameterTest : public ZombieSchemeTest {};

// ============================================================================
// Parameter Creation and Predicates
// ============================================================================

TEST_F(ParameterTest, MakeParameterBasic) {
    eval("(define p (make-parameter 10))");
    EXPECT_TRUE(primitiveExists("p"));
}

TEST_F(ParameterTest, ParameterPredicateOnParameter) {
    eval("(define p (make-parameter 42))");
    expectBool("(parameter? p)", true);
}

TEST_F(ParameterTest, ParameterPredicateOnNonParameter) {
    expectBool("(parameter? 123)", false);
    expectBool("(parameter? \"string\")", false);
    expectBool("(parameter? '(1 2 3))", false);
    expectBool("(parameter? (lambda (x) x))", false);
}

TEST_F(ParameterTest, ParameterPredicateExists) {
    EXPECT_TRUE(primitiveExists("parameter?"));
    EXPECT_TRUE(primitiveExists("make-parameter"));
}

// ============================================================================
// Parameter Access (Calling with No Arguments)
// ============================================================================

TEST_F(ParameterTest, ParameterGetValue) {
    eval("(define p1 (make-parameter 100))");
    expectInt("(p1)", 100);
}

TEST_F(ParameterTest, ParameterGetValueString) {
    eval("(define p2 (make-parameter \"hello\"))");
    expectString("(p2)", "hello");
}

TEST_F(ParameterTest, ParameterGetValueList) {
    eval("(define p3 (make-parameter '(1 2 3)))");
    expectInt("(car (p3))", 1);
    expectInt("(car (cdr (p3)))", 2);
}

TEST_F(ParameterTest, ParameterDefaultNil) {
    eval("(define p4 (make-parameter '()))");
    expectNil("(p4)");
}

// ============================================================================
// Parameter Mutation (Calling with One Argument)
// ============================================================================

TEST_F(ParameterTest, ParameterSetValue) {
    eval("(define p5 (make-parameter 10))");
    expectInt("(p5)", 10);

    eval("(p5 20)");
    expectInt("(p5)", 20);
}

TEST_F(ParameterTest, ParameterSetMultipleTimes) {
    eval("(define p6 (make-parameter 1))");
    expectInt("(p6)", 1);

    eval("(p6 2)");
    expectInt("(p6)", 2);

    eval("(p6 3)");
    expectInt("(p6)", 3);
}

TEST_F(ParameterTest, ParameterSetDifferentTypes) {
    eval("(define p7 (make-parameter 42))");
    expectInt("(p7)", 42);

    eval("(p7 \"changed\")");
    expectString("(p7)", "changed");

    eval("(p7 '(a b c))");
    expectSymbol("(car (p7))", "a");
}

// ============================================================================
// Parameters are Procedures
// ============================================================================

TEST_F(ParameterTest, ParameterIsCallable) {
    eval("(define p8 (make-parameter 99))");

    // Should be able to call it
    expectInt("(p8)", 99);

    // Should work in function calls
    expectInt("((lambda (f) (f)) p8)", 99);
}

TEST_F(ParameterTest, ParameterPassedToLambda) {
    eval("(define p9 (make-parameter 5))");
    expectInt("((lambda (param) (param)) p9)", 5);
}

TEST_F(ParameterTest, ParameterInLet) {
    eval("(define p10 (make-parameter 100))");
    expectInt("(let ((x (p10))) x)", 100);
}

// ============================================================================
// Multiple Parameters
// ============================================================================

TEST_F(ParameterTest, MultipleIndependentParameters) {
    eval("(define px (make-parameter 1))");
    eval("(define py (make-parameter 2))");
    eval("(define pz (make-parameter 3))");

    expectInt("(px)", 1);
    expectInt("(py)", 2);
    expectInt("(pz)", 3);

    eval("(px 10)");
    expectInt("(px)", 10);
    expectInt("(py)", 2);  // Others unchanged
    expectInt("(pz)", 3);
}

TEST_F(ParameterTest, ParameterComputations) {
    eval("(define pa (make-parameter 5))");
    eval("(define pb (make-parameter 3))");

    expectInt("(+ (pa) (pb))", 8);
    expectInt("(* (pa) (pb))", 15);
}

// ============================================================================
// Parameter Creation from C++
// ============================================================================

TEST_F(ParameterTest, CreateParameterDirectly) {
    // GC safety: protect inner allocation during outer allocation
    gc_root<a_int> val(gc_new<a_int>(42));
    a_parameter* param = gc_new<a_parameter>(val.get(), nil);

    EXPECT_TRUE(is_parameter(param));
    expectInt("42", 42);  // Just verify basic evaluation works
}

TEST_F(ParameterTest, ParameterDirectAccess) {
    // GC safety: protect inner allocation during outer allocation
    gc_root<a_int> val(gc_new<a_int>(100));
    a_parameter* param = gc_new<a_parameter>(val.get(), nil);
    EXPECT_EQ(dynamic_cast<a_int*>(param->get_value())->value(), 100);
}

TEST_F(ParameterTest, ParameterDirectMutation) {
    // GC safety: protect inner allocation during outer allocation
    gc_root<a_int> val(gc_new<a_int>(10));
    a_parameter* param = gc_new<a_parameter>(val.get(), nil);
    EXPECT_EQ(dynamic_cast<a_int*>(param->get_value())->value(), 10);

    // GC safety: protect new value during set_value call
    gc_root<a_int> new_val(gc_new<a_int>(20));
    param->set_value(new_val.get());
    EXPECT_EQ(dynamic_cast<a_int*>(param->get_value())->value(), 20);
}

// ============================================================================
// Parameter Type Tests
// ============================================================================

// Re-enabled 2026-06-10: previously crashed in full suite (ACCESS_VIOLATION
// after ~1500 tests, suspected test pollution); no longer reproduces.
TEST_F(ParameterTest, ParameterWithNil) {
    eval("(define pnil (make-parameter '()))");
    expectBool("(null? (pnil))", true);
}

TEST_F(ParameterTest, ParameterWithBoolean) {
    eval("(define pbool (make-parameter #t))");
    expectBool("(pbool)", true);

    eval("(pbool #f)");
    expectBool("(pbool)", false);
}

TEST_F(ParameterTest, ParameterWithSymbol) {
    eval("(define psym (make-parameter 'foo))");
    expectSymbol("(psym)", "foo");
}

// ============================================================================
// Parameter Converters (R7RS: converter applies to init and on set)
// ============================================================================

TEST_F(ParameterTest, ConverterAppliedToInitialValue) {
    eval("(define pconv (make-parameter 5 (lambda (x) (* x 2))))");
    expectInt("(pconv)", 10);
}

TEST_F(ParameterTest, ConverterAppliedOnSet) {
    eval("(define pconv2 (make-parameter 5 (lambda (x) (* x 2))))");
    eval("(pconv2 7)");
    expectInt("(pconv2)", 14);
}

TEST_F(ParameterTest, ConverterMustBeProcedure) {
    // make-parameter with a non-procedure converter raises an error
    EXPECT_THROW(eval("(make-parameter 5 42)"), std::runtime_error);
}

// ============================================================================
// parameterize (R7RS 4.2.6)
// ============================================================================

TEST_F(ParameterTest, ParameterizeBasic) {
    eval("(define pz (make-parameter 10))");
    expectInt("(parameterize ((pz 20)) (pz))", 20);
    expectInt("(pz)", 10);  // restored after dynamic extent
}

TEST_F(ParameterTest, ParameterizeMultipleBindings) {
    eval("(define pa (make-parameter 1))");
    eval("(define pb (make-parameter 2))");
    expectInt("(parameterize ((pa 10) (pb 20)) (+ (pa) (pb)))", 30);
    expectInt("(+ (pa) (pb))", 3);
}

TEST_F(ParameterTest, ParameterizeAppliesConverter) {
    eval("(define pc (make-parameter 5 (lambda (x) (* x 2))))");
    expectInt("(pc)", 10);
    expectInt("(parameterize ((pc 7)) (pc))", 14);
    // Restore must NOT re-apply the converter: still 10, not 20
    expectInt("(pc)", 10);
}

TEST_F(ParameterTest, ParameterizeNested) {
    eval("(define pn (make-parameter 1))");
    expectInt(
        "(parameterize ((pn 2))"
        "  (parameterize ((pn 3)) (pn)))",
        3);
    expectInt("(pn)", 1);
}

TEST_F(ParameterTest, ParameterizeRestoresOnError) {
    eval("(define pe (make-parameter 10))");
    // Body raises; guard catches OUTSIDE the parameterize extent, so the
    // old value must already be restored when the clause runs
    expectInt(
        "(guard (e (else (pe)))"
        "  (parameterize ((pe 99)) (error \"boom\")))",
        10);
}

TEST_F(ParameterTest, ParameterizeRequiresParameter) {
    EXPECT_THROW(eval("(parameterize ((42 1)) 5)"), std::runtime_error);
}

// ============================================================================
// Parameter Printing
// ============================================================================

TEST_F(ParameterTest, ParameterPrintFormat) {
    // GC safety: protect allocations
    gc_root<a_int> val(gc_new<a_int>(42));
    gc_root<a_parameter> param(gc_new<a_parameter>(val.get(), nil));

    std::ostringstream os;
    param.get()->print(os);

    std::string output = os.str();
    EXPECT_TRUE(output.find("#<parameter") != std::string::npos);
}

// ============================================================================
// Parameter Equality
// ============================================================================

TEST_F(ParameterTest, ParameterEqualitySameObject) {
    // GC safety: protect allocations
    gc_root<a_int> val(gc_new<a_int>(10));
    gc_root<a_parameter> p1(gc_new<a_parameter>(val.get(), nil));
    EXPECT_TRUE(p1.get()->is_equal(p1.get()));
}

TEST_F(ParameterTest, ParameterEqualityDifferentObjects) {
    // GC safety: protect inner allocation during outer allocation
    gc_root<a_int> val1(gc_new<a_int>(10));
    a_parameter* p1 = gc_new<a_parameter>(val1.get(), nil);
    // Must protect p1 before allocating p2
    gc_root<a_parameter> guard_p1(p1);

    gc_root<a_int> val2(gc_new<a_int>(10));
    a_parameter* p2 = gc_new<a_parameter>(val2.get(), nil);

    // Different parameter objects are not equal, even with same value
    EXPECT_FALSE(p1->is_equal(p2));
}

// ============================================================================
// GC Integration
// ============================================================================

TEST_F(ParameterTest, ParameterGCTracing) {
    // GC safety: protect value during parameter allocation
    gc_root<a_int> value(gc_new<a_int>(42));
    a_parameter* param = gc_new<a_parameter>(value.get(), nil);
    gc_root<a_parameter> guard_param(param);  // GC safety

    // Parameter should properly trace its value
    EXPECT_EQ(param->get_value(), value.get());

    // Store in environment to keep alive
    gc_root<a_string> name(gc_new<a_string>("gc-param"));
    env->define(name.get(), param);

    // Value should still be accessible
    EXPECT_EQ(param->get_value(), value.get());
}

TEST_F(ParameterTest, ParameterCloning) {
    // GC safety: protect inner allocation during outer allocation
    gc_root<a_int> val(gc_new<a_int>(100));
    a_parameter* p1 = gc_new<a_parameter>(val.get(), nil);
    // Protect p1 before cloning (which may allocate)
    gc_root<a_parameter> guard_p1(p1);

    Object* cloned = p1->clone();
    a_parameter* p2 = static_cast<a_parameter*>(cloned);

    // Cloned parameter should have same value
    EXPECT_EQ(dynamic_cast<a_int*>(p2->get_value())->value(), 100);

    // But should be different objects
    EXPECT_FALSE(p1->is_equal(p2));
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(ParameterTest, ParameterInDefine) {
    eval("(define my-param (make-parameter 'initial))");
    expectSymbol("(my-param)", "initial");
}

TEST_F(ParameterTest, ParameterInNestedLet) {
    eval("(define pnest (make-parameter 10))");
    expectInt("(let ((a (pnest))) (let ((b (pnest))) (+ a b)))", 20);
}

TEST_F(ParameterTest, ParameterAsArgument) {
    eval("(define parg (make-parameter 5))");
    expectInt("((lambda (p) (p)) parg)", 5);
}

TEST_F(ParameterTest, ParameterWithComputation) {
    eval("(define pcomp (make-parameter (* 3 4)))");
    expectInt("(pcomp)", 12);

    eval("(pcomp (+ 5 5))");
    expectInt("(pcomp)", 10);
}

TEST_F(ParameterTest, MultipleParametersInExpression) {
    eval("(define p-x (make-parameter 2))");
    eval("(define p-y (make-parameter 3))");
    eval("(define p-z (make-parameter 4))");

    expectInt("(+ (* (p-x) (p-y)) (p-z))", 10);  // 2*3 + 4 = 10
}
