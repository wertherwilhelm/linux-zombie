/**
 * @file BytevectorTests.cpp
 * @brief R7RS Bytevector tests (Section 6.9)
 */

#include "pch.h"
#include "gtest/gtest.h"
#include "TestFixture.h"
#include "a_bytevector.h"

class BytevectorTest : public ZombieSchemeTest {};

// ============================================================================
// Type Checking - bytevector?
// ============================================================================

TEST_F(BytevectorTest, BytevectorPredicateOnBytevector) {
    expectBool("(bytevector? (bytevector 1 2 3))", true);
}

TEST_F(BytevectorTest, BytevectorPredicateOnEmptyBytevector) {
    expectBool("(bytevector? (make-bytevector 0))", true);
}

TEST_F(BytevectorTest, BytevectorPredicateOnSymbol) {
    expectBool("(bytevector? 'abc)", false);
}

TEST_F(BytevectorTest, BytevectorPredicateOnString) {
    expectBool("(bytevector? \"abc\")", false);
}

TEST_F(BytevectorTest, BytevectorPredicateOnNumber) {
    expectBool("(bytevector? 123)", false);
}

TEST_F(BytevectorTest, BytevectorPredicateOnList) {
    expectBool("(bytevector? '(1 2 3))", false);
}

// ============================================================================
// Construction - make-bytevector
// ============================================================================

TEST_F(BytevectorTest, MakeBytevectorBasic) {
    expectInt("(bytevector-length (make-bytevector 3))", 3);
    expectInt("(bytevector-length (make-bytevector 0))", 0);
}

TEST_F(BytevectorTest, MakeBytevectorWithFill) {
    expectInt("(bytevector-u8-ref (make-bytevector 2 255) 0)", 255);
    expectInt("(bytevector-u8-ref (make-bytevector 2 255) 1)", 255);
    expectInt("(bytevector-u8-ref (make-bytevector 4 42) 2)", 42);
}

// ============================================================================
// Construction - bytevector
// ============================================================================

TEST_F(BytevectorTest, BytevectorBasicEmpty) {
    expectInt("(bytevector-length (bytevector))", 0);
}

TEST_F(BytevectorTest, BytevectorBasicSingle) {
    expectInt("(bytevector-u8-ref (bytevector 1) 0)", 1);
}

TEST_F(BytevectorTest, BytevectorBasicMultiple) {
    expectInt("(bytevector-u8-ref (bytevector 1 2 3) 0)", 1);
    expectInt("(bytevector-u8-ref (bytevector 1 2 3) 1)", 2);
    expectInt("(bytevector-u8-ref (bytevector 1 2 3) 2)", 3);
}

TEST_F(BytevectorTest, BytevectorBoundaryValues) {
    expectInt("(bytevector-u8-ref (bytevector 0 128 255) 0)", 0);
    expectInt("(bytevector-u8-ref (bytevector 0 128 255) 1)", 128);
    expectInt("(bytevector-u8-ref (bytevector 0 128 255) 2)", 255);
}

// ============================================================================
// Access - bytevector-length
// ============================================================================

TEST_F(BytevectorTest, BytevectorLengthEmpty) {
    expectInt("(bytevector-length (make-bytevector 0))", 0);
}

TEST_F(BytevectorTest, BytevectorLengthNonEmpty) {
    expectInt("(bytevector-length (make-bytevector 3))", 3);
    expectInt("(bytevector-length (bytevector 1 2 3 4 5))", 5);
}

// ============================================================================
// Access - bytevector-u8-ref
// ============================================================================

TEST_F(BytevectorTest, BytevectorU8RefBasic) {
    expectInt("(bytevector-u8-ref (bytevector 1 2 3) 0)", 1);
    expectInt("(bytevector-u8-ref (bytevector 1 2 3) 1)", 2);
    expectInt("(bytevector-u8-ref (bytevector 1 2 3) 2)", 3);
}

TEST_F(BytevectorTest, BytevectorU8RefMaxValue) {
    expectInt("(bytevector-u8-ref (bytevector 0 255 128) 1)", 255);
}

// ============================================================================
// Mutation - bytevector-u8-set!
// ============================================================================

TEST_F(BytevectorTest, BytevectorU8SetBasic) {
    eval("(define bv1 (bytevector 1 2 3))");
    eval("(bytevector-u8-set! bv1 0 42)");
    expectInt("(bytevector-u8-ref bv1 0)", 42);
    expectInt("(bytevector-u8-ref bv1 1)", 2);  // Others unchanged
}

TEST_F(BytevectorTest, BytevectorU8SetMaxValue) {
    eval("(define bv2 (bytevector 1 2 3))");
    eval("(bytevector-u8-set! bv2 1 255)");
    expectInt("(bytevector-u8-ref bv2 1)", 255);
}

// ============================================================================
// Copying - bytevector-copy
// ============================================================================

TEST_F(BytevectorTest, BytevectorCopyFull) {
    expectInt("(bytevector-length (bytevector-copy (bytevector 1 2 3)))", 3);
    expectInt("(bytevector-u8-ref (bytevector-copy (bytevector 1 2 3)) 0)", 1);
    expectInt("(bytevector-u8-ref (bytevector-copy (bytevector 1 2 3)) 2)", 3);
}

TEST_F(BytevectorTest, BytevectorCopyEmpty) {
    expectInt("(bytevector-length (bytevector-copy (bytevector)))", 0);
}

TEST_F(BytevectorTest, BytevectorCopyWithStart) {
    expectInt("(bytevector-length (bytevector-copy (bytevector 1 2 3 4 5) 2))", 3);
    expectInt("(bytevector-u8-ref (bytevector-copy (bytevector 1 2 3 4 5) 2) 0)", 3);
}

TEST_F(BytevectorTest, BytevectorCopyWithStartAndEnd) {
    expectInt("(bytevector-length (bytevector-copy (bytevector 1 2 3 4 5) 1 3))", 2);
    expectInt("(bytevector-u8-ref (bytevector-copy (bytevector 1 2 3 4 5) 1 3) 0)", 2);
    expectInt("(bytevector-u8-ref (bytevector-copy (bytevector 1 2 3 4 5) 1 3) 1)", 3);
}

TEST_F(BytevectorTest, BytevectorCopyIsIndependent) {
    eval("(define bv-orig (bytevector 1 2 3))");
    eval("(define bv-copy (bytevector-copy bv-orig))");
    eval("(bytevector-u8-set! bv-copy 0 99)");
    expectInt("(bytevector-u8-ref bv-orig 0)", 1);  // Original unchanged
    expectInt("(bytevector-u8-ref bv-copy 0)", 99); // Copy changed
}

// ============================================================================
// Copying - bytevector-copy!
// ============================================================================

TEST_F(BytevectorTest, BytevectorCopyMutFull) {
    eval("(define to-bv (make-bytevector 5 0))");
    eval("(define from-bv (bytevector 10 20 30))");
    eval("(bytevector-copy! to-bv 0 from-bv)");
    expectInt("(bytevector-u8-ref to-bv 0)", 10);
    expectInt("(bytevector-u8-ref to-bv 1)", 20);
    expectInt("(bytevector-u8-ref to-bv 2)", 30);
    expectInt("(bytevector-u8-ref to-bv 3)", 0);
}

TEST_F(BytevectorTest, BytevectorCopyMutWithRange) {
    eval("(define to-bv2 (make-bytevector 5 0))");
    eval("(define from-bv2 (bytevector 1 2 3 4))");
    eval("(bytevector-copy! to-bv2 1 from-bv2 1 3)");
    expectInt("(bytevector-u8-ref to-bv2 0)", 0);
    expectInt("(bytevector-u8-ref to-bv2 1)", 2);
    expectInt("(bytevector-u8-ref to-bv2 2)", 3);
    expectInt("(bytevector-u8-ref to-bv2 3)", 0);
}

// ============================================================================
// Concatenation - bytevector-append
// ============================================================================

TEST_F(BytevectorTest, BytevectorAppendEmpty) {
    expectInt("(bytevector-length (bytevector-append))", 0);
}

TEST_F(BytevectorTest, BytevectorAppendSingle) {
    expectInt("(bytevector-length (bytevector-append (bytevector 1 2 3)))", 3);
    expectInt("(bytevector-u8-ref (bytevector-append (bytevector 1 2 3)) 1)", 2);
}

TEST_F(BytevectorTest, BytevectorAppendMultiple) {
    eval("(define app-result (bytevector-append (bytevector 1 2) (bytevector 3 4) (bytevector 5 6)))");
    expectInt("(bytevector-length app-result)", 6);
    expectInt("(bytevector-u8-ref app-result 0)", 1);
    expectInt("(bytevector-u8-ref app-result 2)", 3);
    expectInt("(bytevector-u8-ref app-result 4)", 5);
}

TEST_F(BytevectorTest, BytevectorAppendWithEmpty) {
    eval("(define app-result2 (bytevector-append (bytevector) (bytevector 1 2 3) (bytevector)))");
    expectInt("(bytevector-length app-result2)", 3);
    expectInt("(bytevector-u8-ref app-result2 1)", 2);
}

// ============================================================================
// UTF-8 Conversion - utf8->string
// ============================================================================

TEST_F(BytevectorTest, Utf8ToStringBasic) {
    expectString("(utf8->string (bytevector 97 98 99))", "abc");
}

TEST_F(BytevectorTest, Utf8ToStringEmpty) {
    expectString("(utf8->string (bytevector))", "");
}

TEST_F(BytevectorTest, Utf8ToStringHello) {
    expectString("(utf8->string (bytevector 104 101 108 108 111))", "hello");
}

TEST_F(BytevectorTest, Utf8ToStringWithStart) {
    expectString("(utf8->string (bytevector 104 101 108 108 111) 3)", "lo");
}

TEST_F(BytevectorTest, Utf8ToStringWithStartAndEnd) {
    expectString("(utf8->string (bytevector 104 101 108 108 111) 1 4)", "ell");
}

TEST_F(BytevectorTest, Utf8ToStringEmptyRange) {
    expectString("(utf8->string (bytevector 104 101 108 108 111) 2 2)", "");
}

// ============================================================================
// UTF-8 Conversion - string->utf8
// ============================================================================

TEST_F(BytevectorTest, StringToUtf8Basic) {
    expectInt("(bytevector-u8-ref (string->utf8 \"abc\") 0)", 97);
    expectInt("(bytevector-u8-ref (string->utf8 \"abc\") 1)", 98);
    expectInt("(bytevector-u8-ref (string->utf8 \"abc\") 2)", 99);
}

TEST_F(BytevectorTest, StringToUtf8Empty) {
    expectInt("(bytevector-length (string->utf8 \"\"))", 0);
}

TEST_F(BytevectorTest, StringToUtf8Hello) {
    eval("(define utf8-hello (string->utf8 \"hello\"))");
    expectInt("(bytevector-length utf8-hello)", 5);
    expectInt("(bytevector-u8-ref utf8-hello 0)", 104);
    expectInt("(bytevector-u8-ref utf8-hello 1)", 101);
}

TEST_F(BytevectorTest, StringToUtf8WithStart) {
    eval("(define utf8-partial (string->utf8 \"hello\" 3))");
    expectInt("(bytevector-length utf8-partial)", 2);
    expectInt("(bytevector-u8-ref utf8-partial 0)", 108);
    expectInt("(bytevector-u8-ref utf8-partial 1)", 111);
}

TEST_F(BytevectorTest, StringToUtf8WithStartAndEnd) {
    eval("(define utf8-range (string->utf8 \"hello\" 1 4))");
    expectInt("(bytevector-length utf8-range)", 3);
    expectInt("(bytevector-u8-ref utf8-range 0)", 101);
    expectInt("(bytevector-u8-ref utf8-range 1)", 108);
    expectInt("(bytevector-u8-ref utf8-range 2)", 108);
}

// ============================================================================
// Round-trip Conversion Tests
// ============================================================================

TEST_F(BytevectorTest, Utf8RoundTripString) {
    expectString("(utf8->string (string->utf8 \"hello\"))", "hello");
    expectString("(utf8->string (string->utf8 \"\"))", "");
    expectString("(utf8->string (string->utf8 \"Scheme\"))", "Scheme");
}

TEST_F(BytevectorTest, Utf8RoundTripBytevector) {
    eval("(define roundtrip-bv (string->utf8 (utf8->string (bytevector 65 66 67))))");
    expectInt("(bytevector-length roundtrip-bv)", 3);
    expectInt("(bytevector-u8-ref roundtrip-bv 0)", 65);
    expectInt("(bytevector-u8-ref roundtrip-bv 1)", 66);
    expectInt("(bytevector-u8-ref roundtrip-bv 2)", 67);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(BytevectorTest, BytevectorInLet) {
    expectInt("(let ((bv (bytevector 1 2 3))) (bytevector-length bv))", 3);
}

TEST_F(BytevectorTest, BytevectorInDefine) {
    eval("(define my-bv (bytevector 10 20 30))");
    expectInt("(bytevector-length my-bv)", 3);
    expectInt("(bytevector-u8-ref my-bv 1)", 20);
}

TEST_F(BytevectorTest, BytevectorInLambda) {
    expectInt("((lambda (bv) (bytevector-u8-ref bv 1)) (bytevector 1 2 3))", 2);
}

TEST_F(BytevectorTest, ComplexBytevectorOperations) {
    eval("(define comp-bv (make-bytevector 3))");
    eval("(bytevector-u8-set! comp-bv 0 (* 1 2))");
    eval("(bytevector-u8-set! comp-bv 1 (* 2 2))");
    eval("(bytevector-u8-set! comp-bv 2 (* 3 2))");
    expectInt("(bytevector-u8-ref comp-bv 0)", 2);
    expectInt("(bytevector-u8-ref comp-bv 1)", 4);
    expectInt("(bytevector-u8-ref comp-bv 2)", 6);
}

TEST_F(BytevectorTest, BytevectorManualEquality) {
    eval("(define bv1 (bytevector 1 2 3))");
    eval("(define bv2 (bytevector 1 2 3))");
    expectBool("(= (bytevector-length bv1) (bytevector-length bv2))", true);
    expectBool("(= (bytevector-u8-ref bv1 0) (bytevector-u8-ref bv2 0))", true);
    expectBool("(= (bytevector-u8-ref bv1 1) (bytevector-u8-ref bv2 1))", true);
    expectBool("(= (bytevector-u8-ref bv1 2) (bytevector-u8-ref bv2 2))", true);
}
