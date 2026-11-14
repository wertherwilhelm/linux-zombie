#include "pch.h"
#include "TestFixture.h"
#include "scheme_to_coreir.h"
#include "coreir_to_bytecode.h"
#include "bytecode_vm.h"
#include "numeric_runtime.h"
#include "scheme_reader.h"

using namespace zombie::runtime;

namespace {
// Drive the full CoreIR pipeline end-to-end:
//   Scheme text -> read -> SchemeToCoreir (lower) -> CoreIRToBytecode -> BytecodeVM.
// Returns the evaluated Object*. Throws propagate (so unimplemented paths surface).
Object* run_coreir(Environment* env, const std::string& code) {
    scheme_reader reader;
    Object* expr = reader.read_from_string(code);
    coreir::SchemeToCoreirLowering lowering(env);
    auto func = lowering.lower_expression_to_function(expr, "__main__");
    coreir::CoreIRToBytecode translator;
    auto cf = translator.compile_function(*func);
    zombie::bytecode::BytecodeVM vm(env);
    return vm.execute(cf.bytecode, cf.constants, env);
}

long long as_int(Object* o) { return numeric::to_int64(o); }
}  // namespace

class CoreIRPipelineTest : public ZombieSchemeTest {};

TEST_F(CoreIRPipelineTest, Arithmetic)      { EXPECT_EQ(as_int(run_coreir(env, "(+ 1 2)")), 3); }
TEST_F(CoreIRPipelineTest, NestedArithmetic){ EXPECT_EQ(as_int(run_coreir(env, "(* (+ 1 2) (- 10 4))")), 18); }
TEST_F(CoreIRPipelineTest, IfTrue)          { EXPECT_EQ(as_int(run_coreir(env, "(if (< 1 2) 10 20)")), 10); }
TEST_F(CoreIRPipelineTest, IfFalse)         { EXPECT_EQ(as_int(run_coreir(env, "(if (> 1 2) 10 20)")), 20); }
TEST_F(CoreIRPipelineTest, Let)             { EXPECT_EQ(as_int(run_coreir(env, "(let ((x 5) (y 7)) (+ x y))")), 12); }
TEST_F(CoreIRPipelineTest, LetStar)         { EXPECT_EQ(as_int(run_coreir(env, "(let* ((x 5) (y (* x 2))) (+ x y))")), 15); }
TEST_F(CoreIRPipelineTest, Begin)           { EXPECT_EQ(as_int(run_coreir(env, "(begin 1 2 (+ 1 2))")), 3); }
TEST_F(CoreIRPipelineTest, Cond)            { EXPECT_EQ(as_int(run_coreir(env, "(cond ((= 1 2) 10) (else 20))")), 20); }
TEST_F(CoreIRPipelineTest, LambdaApply)     { EXPECT_EQ(as_int(run_coreir(env, "((lambda (x) (* x x)) 5)")), 25); }

// --- Harder cases: probe the real gaps (local mutation, closures, recursion) ---
TEST_F(CoreIRPipelineTest, SetLocalMutation) { EXPECT_EQ(as_int(run_coreir(env, "(let ((x 1)) (set! x 2) x)")), 2); }
TEST_F(CoreIRPipelineTest, ClosureCapture)   { EXPECT_EQ(as_int(run_coreir(env, "(((lambda (x) (lambda (y) (+ x y))) 3) 4)")), 7); }
TEST_F(CoreIRPipelineTest, LetrecRecursion)  { EXPECT_EQ(as_int(run_coreir(env, "(letrec ((f (lambda (n) (if (< n 2) 1 (* n (f (- n 1))))))) (f 5))")), 120); }
TEST_F(CoreIRPipelineTest, DefineInBegin)    { EXPECT_EQ(as_int(run_coreir(env, "(begin (define sq (lambda (x) (* x x))) (sq 6))")), 36); }
TEST_F(CoreIRPipelineTest, NamedLetLoop)     { EXPECT_EQ(as_int(run_coreir(env, "(let loop ((i 0) (acc 0)) (if (< i 5) (loop (+ i 1) (+ acc i)) acc))")), 10); }

// Isolate: multi-argument lambda application + multi-arg recursion
TEST_F(CoreIRPipelineTest, MultiArgLambda)   { EXPECT_EQ(as_int(run_coreir(env, "((lambda (a b) (+ a b)) 3 4)")), 7); }
TEST_F(CoreIRPipelineTest, MultiArgLetrec)   { EXPECT_EQ(as_int(run_coreir(env, "(letrec ((g (lambda (a b) (if (< a b) (g (+ a 1) b) a)))) (g 0 3))")), 3); }

// The exact form the named-let desugar should produce:
TEST_F(CoreIRPipelineTest, HandDesugaredNamedLet) {
    EXPECT_EQ(as_int(run_coreir(env,
        "(letrec ((loop (lambda (i acc) (if (< i 5) (loop (+ i 1) (+ acc i)) acc)))) (loop 0 0))")), 10);
}
