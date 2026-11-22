#pragma once
#include "compilation.h"

namespace zombie::native {
enum class UnsupportedPolicy { Error, Bytecode };
// Same IEvaluator boundary as the tree walker, toy VM, bytecode, and CoreIR.
// Default fallback lets existing Scheme closures/prelude run while native
// code generation is extended. Strict mode is useful for backend development.
class NativeEvaluator final : public runtime::IEvaluator {
    runtime::Environment* env_;
    compilation::CompilationPipeline pipeline_;
public:
    explicit NativeEvaluator(runtime::Environment* env,
                             UnsupportedPolicy policy = UnsupportedPolicy::Bytecode,
                             size_t cache_capacity = 128);
    runtime::Object* eval(runtime::Object*, runtime::Environment*) override;
    runtime::Environment* get_environment() override { return env_; }
    compilation::CompilationPipeline& pipeline() noexcept { return pipeline_; }
};
} // namespace zombie::native
