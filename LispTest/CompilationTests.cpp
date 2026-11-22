#include "gtest/gtest.h"
#include "scheme_engine.h"
#include "compilation.h"
#include "coreir_evaluator.h"
#include "native_compiler.h"
#include "native_evaluator.h"
#include "gc_manager.h"
#include "native_primitive.h"
#include "closure.h"
#include "i_native_code.h"

using namespace zombie::compilation;
using zombie::native::NativeCompilerBackend;
using zombie::native::NativeEvaluator;
using zombie::native::UnsupportedPolicy;

class CompilationTest : public ::testing::Test {
protected:
    void SetUp() override { init_lisp_constants(); }
    static double number(scheme_engine& engine, const std::string& source) {
        auto* value = static_cast<Object*>(engine.eval_string(source));
        EXPECT_NE(value, nullptr) << engine.get_last_error();
        return value ? num(value) : -999;
    }
};

TEST_F(CompilationTest, BytecodeBackendUsesSharedPipelineAndCachesCode) {
    scheme_engine engine(EvaluatorType::COREIR);
    auto* evaluator = dynamic_cast<coreir::CoreIrEvaluator*>(engine.get_evaluator());
    ASSERT_NE(evaluator, nullptr);
    auto before = evaluator->pipeline().stats();
    EXPECT_EQ(36, number(engine, "(+ 17 19)"));
    auto after = evaluator->pipeline().stats();
    EXPECT_EQ(after.compilations, before.compilations + 1);
    EXPECT_EQ(36, number(engine, "(+ 17 19)"));
    EXPECT_EQ(evaluator->pipeline().stats().cache_hits, after.cache_hits + 1);
    EXPECT_EQ(evaluator->pipeline().stats().compilations, after.compilations);
}
TEST_F(CompilationTest, BytecodeArtifactsRootConstantsAcrossCollections) {
    scheme_engine engine;
    CompilationPipeline pipeline(std::make_unique<BytecodeCompilerBackend>());
    auto artifact = pipeline.compile(engine.read_string("'(one two three)"),
                                     engine.get_environment(), OutputKind::Bytecode);
    gc_context::current()->collect();
    auto executable = std::dynamic_pointer_cast<ExecutableCode>(artifact);
    ASSERT_NE(executable, nullptr);
    EXPECT_EQ("(one two three)", engine.print_value(executable->execute(engine.get_environment())));
}
TEST_F(CompilationTest, MutableLiteralUnitsBypassCache) {
    scheme_engine engine;
    CompilationPipeline pipeline(std::make_unique<BytecodeCompilerBackend>());
    auto one = pipeline.compile(engine.read_string("'(1 2)"), engine.get_environment(), OutputKind::Bytecode);
    auto two = pipeline.compile(engine.read_string("'(1 2)"), engine.get_environment(), OutputKind::Bytecode);
    EXPECT_NE(one, two);
    EXPECT_EQ(pipeline.cache_size(), 0u);
}
TEST_F(CompilationTest, ObjectOutputIsExplicitlyUnsupported) {
    scheme_engine engine;
    NativeCompilerBackend native;
    EXPECT_FALSE(native.supports(OutputKind::NativeObject));
    BytecodeCompilerBackend bytecode;
    coreir::CoreIRFunction f("constant", {}, coreir::Stmt::makeReturn(coreir::Atom::makeLit(t)));
    EXPECT_THROW(native.compile(f, OutputKind::NativeObject), UnsupportedCompilation);
    EXPECT_THROW(bytecode.compile(f, OutputKind::NativeJit), UnsupportedCompilation);
}

namespace {
class DummyArtifact final : public CompiledArtifact {
public:
    OutputKind output_kind() const noexcept override { return OutputKind::Bytecode; }
    size_t code_size() const noexcept override { return 0; }
};
}
TEST(CompilationCacheTest, EvictsLeastRecentlyUsedAndPinsActiveArtifacts) {
    CompilationCache cache(2);
    auto first = std::make_shared<DummyArtifact>();
    auto second = std::make_shared<DummyArtifact>();
    auto third = std::make_shared<DummyArtifact>();
    cache.insert("one", first); cache.insert("two", second);
    EXPECT_EQ(cache.find("one"), first);
    EXPECT_TRUE(cache.insert("three", third));
    EXPECT_EQ(cache.find("two"), nullptr);
    EXPECT_EQ(cache.find("one"), first);
    cache.clear();
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_EQ(first->code_size(), 0u); // An external owner survives eviction/clear.
}
TEST(CompilationCacheTest, ZeroCapacityDisablesRetention) {
    CompilationCache cache(0);
    cache.insert("unit", std::make_shared<DummyArtifact>());
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_EQ(cache.find("unit"), nullptr);
}

class NativeCompilationTest : public CompilationTest {
protected:
    void SetUp() override {
        CompilationTest::SetUp();
        if (!NativeCompilerBackend::available()) GTEST_SKIP() << "Native x86-64 backend disabled/unavailable";
    }
};
TEST_F(NativeCompilationTest, EmitsAndExecutesNativeCode) {
    scheme_engine engine;
    CompilationPipeline pipeline(std::make_unique<NativeCompilerBackend>());
    auto artifact = pipeline.compile(engine.read_string("(+ (* 7 8) 9)"),
                                     engine.get_environment(), OutputKind::NativeJit);
    ASSERT_EQ(artifact->output_kind(), OutputKind::NativeJit);
    EXPECT_GT(artifact->code_size(), 0u);
    auto executable = std::dynamic_pointer_cast<ExecutableCode>(artifact);
    ASSERT_NE(executable, nullptr);
    EXPECT_EQ(65, num(executable->execute(engine.get_environment())));
}
TEST_F(NativeCompilationTest, StrictCompilationRejectsClosuresWithoutRunningSideEffects) {
    scheme_engine engine;
    NativeEvaluator evaluator(engine.get_environment(), UnsupportedPolicy::Error);
    auto expression = engine.read_string("(begin (define should-not-run 42) (lambda (x) x))");
    EXPECT_THROW(evaluator.eval(expression, engine.get_environment()), UnsupportedCompilation);
    EXPECT_EQ(engine.eval_string("should-not-run"), nullptr);
}
TEST_F(NativeCompilationTest, IntegratesWithEngineAndFallbackSupportsRecursion) {
    scheme_engine engine(EvaluatorType::NATIVE);
    EXPECT_EQ(engine.get_evaluator_type(), EvaluatorType::NATIVE);
    EXPECT_EQ(55, number(engine, "(+ 20 35)"));
    EXPECT_EQ(720, number(engine,
        "(define (fact n) (if (< n 2) 1 (* n (fact (- n 1))))) (fact 6)"));
    auto* evaluator = dynamic_cast<NativeEvaluator*>(engine.get_evaluator());
    ASSERT_NE(evaluator, nullptr);
    EXPECT_GT(evaluator->pipeline().stats().fallback_compilations, 0u);
}
TEST_F(NativeCompilationTest, ReusesNativeCodeButReexecutesSideEffectsAndReadsCurrentGlobals) {
    scheme_engine engine;
    NativeEvaluator evaluator(engine.get_environment(), UnsupportedPolicy::Error);
    engine.eval_string("(define counter 0)");
    auto run = [&] {
        return evaluator.eval(engine.read_string("(begin (set! counter (+ counter 1)) counter)"),
                              engine.get_environment());
    };
    EXPECT_EQ(1, num(run()));
    auto before = evaluator.pipeline().stats();
    EXPECT_EQ(2, num(run()));
    EXPECT_EQ(evaluator.pipeline().stats().cache_hits, before.cache_hits + 1);
    EXPECT_EQ(evaluator.pipeline().stats().compilations, before.compilations);
    engine.eval_string("(set! counter 100)");
    EXPECT_EQ(101, num(run()));
}
TEST_F(NativeCompilationTest, NativeBranchesAreLazyAndUseSchemeTruth) {
    scheme_engine engine;
    NativeEvaluator evaluator(engine.get_environment(), UnsupportedPolicy::Error);
    for (const auto& source : {"(if #t 17 missing)", "(if #f missing 17)",
                               "(if '() 17 missing)", "(if 0 17 missing)",
                               "(+ (if #f missing 10) (if #t 7 missing))"}) {
        EXPECT_EQ(17, num(evaluator.eval(engine.read_string(source), engine.get_environment()))) << source;
    }
}
TEST_F(NativeCompilationTest, RuntimeErrorsReturnThroughGeneratedFramesAndDoNotTriggerFallback) {
    scheme_engine engine(EvaluatorType::NATIVE);
    auto* evaluator = dynamic_cast<NativeEvaluator*>(engine.get_evaluator());
    ASSERT_NE(evaluator, nullptr);
    auto before = evaluator->pipeline().stats().fallback_compilations;
    EXPECT_EQ(engine.eval_string("(+ 1 \"bad\")"), nullptr);
    EXPECT_FALSE(engine.get_last_error().empty());
    EXPECT_EQ(evaluator->pipeline().stats().fallback_compilations, before);
    EXPECT_EQ(11, number(engine, "(+ 5 6)"));
}
TEST_F(NativeCompilationTest, ScalarConstantsAndNativeArtifactsSurviveCacheAndCompilerDestruction) {
    scheme_engine engine;
    std::shared_ptr<CompiledArtifact> artifact;
    {
        CompilationPipeline pipeline(std::make_unique<NativeCompilerBackend>());
        artifact = pipeline.compile(engine.read_string("(+ 913 17)"), engine.get_environment(), OutputKind::NativeJit);
        pipeline.clear_cache();
    }
    gc_context::current()->collect();
    auto executable = std::dynamic_pointer_cast<ExecutableCode>(artifact);
    ASSERT_NE(executable, nullptr);
    EXPECT_EQ(930, num(executable->execute(engine.get_environment())));
}
TEST_F(NativeCompilationTest, MacroRedefinitionLowersBeforeCacheLookup) {
    scheme_engine engine;
    NativeEvaluator evaluator(engine.get_environment(), UnsupportedPolicy::Error);
    engine.eval_string("(define-syntax choice (syntax-rules () ((_ ) 17)))");
    EXPECT_EQ(17, num(evaluator.eval(engine.read_string("(choice)"), engine.get_environment())));
    engine.eval_string("(define-syntax choice (syntax-rules () ((_ ) 29)))");
    EXPECT_EQ(29, num(evaluator.eval(engine.read_string("(choice)"), engine.get_environment())));
}
TEST_F(NativeCompilationTest, NumericTowerAndPrimitiveArityMatchRuntime) {
    scheme_engine native(EvaluatorType::NATIVE);
    for (auto source : {"(+)", "(*)", "(- 7)", "(/ 2)", "(/ 3 2)",
                        "(+ 1/3 1/6)", "(+ 1 2 3 4)", "(* 2 3 4)",
                        "(< 1 2 3)", "(= +nan.0 +nan.0)", "(not '())"}) {
        scheme_engine reference;
        auto* want = reference.eval_string(source);
        ASSERT_NE(want, nullptr) << source;
        std::string expected = reference.print_value(want);
        auto* got = native.eval_string(source);
        ASSERT_NE(got, nullptr) << source << ": " << native.get_last_error();
        EXPECT_EQ(native.print_value(got), expected) << source;
    }
}
TEST_F(NativeCompilationTest, EngineResetClearsCodeAndBindings) {
    scheme_engine engine(EvaluatorType::NATIVE);
    EXPECT_EQ(42, number(engine, "(define x 40) (+ x 2)"));
    engine.reset();
    EXPECT_EQ(engine.get_evaluator_type(), EvaluatorType::NATIVE);
    EXPECT_EQ(engine.eval_string("x"), nullptr);
    EXPECT_EQ(5, number(engine, "(+ 2 3)"));
}
TEST_F(NativeCompilationTest, PipelineEvictionAndDisabledCacheDoNotChangeResults) {
    scheme_engine engine;
    for (size_t capacity : {0u, 2u}) {
        NativeEvaluator evaluator(engine.get_environment(), UnsupportedPolicy::Error, capacity);
        for (auto source : {"(+ 1 2)", "(+ 3 4)", "(+ 5 6)", "(+ 1 2)"})
            ASSERT_NE(evaluator.eval(engine.read_string(source), engine.get_environment()), nullptr);
        EXPECT_EQ(evaluator.pipeline().cache_size(), capacity);
        EXPECT_EQ(evaluator.pipeline().stats().compilations, 4u);
        EXPECT_EQ(evaluator.pipeline().stats().evictions, capacity ? 2u : 0u);
    }
}
TEST_F(NativeCompilationTest, NativeFrameRootsIntermediateValuesDuringRuntimeCollection) {
    scheme_engine engine;
    auto* primitive = gc_new<zombie::runtime::NativePrimitive>("+", 2, 2,
        +[](IEvaluator*, Object* args, Environment*) -> Object* {
            gc_context::current()->collect();
            return gc_new<a_int>(static_cast<int>(num(first(args)) + num(first(rest(args)))));
        });
    engine.get_environment()->define(primitive);
    NativeEvaluator evaluator(engine.get_environment(), UnsupportedPolicy::Error);
    EXPECT_EQ(10, num(evaluator.eval(engine.read_string("(+ (+ 1 2) (+ 3 4))"), engine.get_environment())));
}

TEST_F(NativeCompilationTest, CachedFallbackRetainsItsDiagnostic) {
    scheme_engine engine;
    CompilationPipeline pipeline(std::make_unique<NativeCompilerBackend>(), 8,
                                 std::make_unique<BytecodeCompilerBackend>());
    auto compile = [&](const char* source) {
        return pipeline.compile(engine.read_string(source), engine.get_environment(), OutputKind::NativeJit);
    };
    auto fallback = compile("(define (f x) (+ x 1))");
    EXPECT_EQ(fallback->output_kind(), OutputKind::Bytecode);
    std::string reason = pipeline.last_fallback_reason();
    EXPECT_FALSE(reason.empty());
    compile("(+ 17 19)");
    EXPECT_TRUE(pipeline.last_fallback_reason().empty());
    EXPECT_EQ(compile("(define (f x) (+ x 1))"), fallback);
    EXPECT_EQ(pipeline.last_fallback_reason(), reason);
}
// Integer division and the list primitives CoreIR lowers as Prim nodes are
// compiled natively (NativeJit artifact, no bytecode fallback) and match the
// tree-walk reference exactly. Arguments are literals / cons so the whole unit
// stays native; `(list ...)` would be a general call and fall back.
TEST_F(NativeCompilationTest, CompilesIntegerDivisionAndListPrimitivesNatively) {
    scheme_engine reference;
    scheme_engine engine;
    CompilationPipeline pipeline(std::make_unique<NativeCompilerBackend>());
    for (auto source : {"(modulo 17 5)", "(modulo -17 5)", "(quotient 17 5)",
                        "(remainder -17 5)", "(length '(1 2 3 4))", "(length '())",
                        "(list-ref '(10 20 30) 1)", "(list? '(1 2))",
                        "(list? (cons 1 2))", "(list? 7)"}) {
        auto artifact = pipeline.compile(engine.read_string(source), engine.get_environment(),
                                         OutputKind::NativeJit);
        EXPECT_EQ(artifact->output_kind(), OutputKind::NativeJit) << source;  // native, not fallback
        auto executable = std::dynamic_pointer_cast<ExecutableCode>(artifact);
        ASSERT_NE(executable, nullptr) << source;
        auto* want = reference.eval_string(source);
        ASSERT_NE(want, nullptr) << source;
        EXPECT_EQ(engine.print_value(executable->execute(engine.get_environment())),
                  reference.print_value(want)) << source;
    }
}
// Non-tail general procedure calls compile natively (NativeJit, not fallback):
// the native caller evaluates operator + operands and applies the callee through
// the runtime ABI (a bytecode closure runs its bytecode on the frame's VM). Tail
// calls still fall back to the trampolining bytecode backend (see the TCO tests).
TEST_F(NativeCompilationTest, CompilesNonTailGeneralCallsNatively) {
    scheme_engine engine(EvaluatorType::NATIVE);
    ASSERT_EQ(81, number(engine, "(define (sq x) (* x x)) (sq 9)"));
    auto* evaluator = dynamic_cast<NativeEvaluator*>(engine.get_evaluator());
    ASSERT_NE(evaluator, nullptr);
    struct Case { const char* src; double want; };
    for (auto c : {Case{"(sq 9)", 81}, Case{"(+ (sq 3) (sq 4))", 25},
                   Case{"(* (sq 2) 10)", 40}}) {
        auto artifact = evaluator->pipeline().compile(
            engine.read_string(c.src), engine.get_environment(), OutputKind::NativeJit);
        EXPECT_EQ(artifact->output_kind(), OutputKind::NativeJit) << c.src;  // native caller
        auto exec = std::dynamic_pointer_cast<ExecutableCode>(artifact);
        ASSERT_NE(exec, nullptr) << c.src;
        EXPECT_EQ(c.want, num(exec->execute(engine.get_environment()))) << c.src;
    }
}

// Closure::apply dispatches to attached native code in preference to bytecode /
// AST (the runtime half of the native-callee ABI). Exercised with a stub so it
// validates the contract independently of the asmjit backend.
namespace {
class StubNativeCode : public zombie::runtime::INativeCode {
public:
    int calls = 0;
    Object* invoke(Environment* call_env) override {
        ++calls;
        // Prove the call env carries the bound parameter.
        return call_env->lookup("x");
    }
    void trace(Tracer&) override {}
};
}
TEST(ClosureNativeCodeTest, ApplyPrefersAttachedNativeCode) {
    init_lisp_constants();
    scheme_engine engine;
    auto stub = std::make_shared<StubNativeCode>();
    Object* params = engine.read_string("(x)");
    auto* closure = gc_new<zombie::runtime::Closure>(params, nil, engine.get_environment());
    EXPECT_FALSE(closure->has_native());
    closure->set_native(stub);
    EXPECT_TRUE(closure->has_native());
    Object* args = cons(gc_new<a_int>(7), nil);
    Object* result = closure->apply(nullptr, args, engine.get_environment());
    EXPECT_EQ(1, stub->calls);
    EXPECT_EQ(7, num(result));
}
