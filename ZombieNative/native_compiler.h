#pragma once
#include "compilation.h"

namespace zombie::native {
class NativeCompilerBackend final : public compilation::ICompilerBackend {
    struct Runtime;
    std::shared_ptr<Runtime> runtime_;
public:
    NativeCompilerBackend();
    ~NativeCompilerBackend() override;
    static bool available() noexcept;
    std::string name() const override { return "asmjit-x86-64-boxed-v1"; }
    bool supports(compilation::OutputKind output) const noexcept override;
    std::shared_ptr<compilation::CompiledArtifact> compile(const coreir::CoreIRFunction&,
                                                          compilation::OutputKind) override;
};
} // namespace zombie::native
