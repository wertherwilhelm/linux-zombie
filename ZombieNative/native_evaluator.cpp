#include "native_evaluator.h"
#include "native_compiler.h"

namespace zombie::native {
NativeEvaluator::NativeEvaluator(runtime::Environment* env, UnsupportedPolicy policy, size_t capacity)
    : env_(env), pipeline_(std::make_unique<NativeCompilerBackend>(), capacity,
        policy == UnsupportedPolicy::Bytecode
            ? std::make_unique<compilation::BytecodeCompilerBackend>() : nullptr) {
    if (!NativeCompilerBackend::available())
        throw compilation::UnsupportedCompilation("Native evaluator requires an asmjit-enabled x86-64 build");
}
runtime::Object* NativeEvaluator::eval(runtime::Object* expr, runtime::Environment* env) {
    return pipeline_.evaluate(expr, env ? env : env_, compilation::OutputKind::NativeJit);
}
} // namespace zombie::native
