/**
 * @file coreir_evaluator.h
 * @brief IEvaluator over the CoreIR pipeline (Scheme -> CoreIR -> bytecode -> VM)
 *
 * Wraps the CoreIR lowering + bytecode translation behind the shared
 * IEvaluator interface so it can be selected like any other evaluator
 * (EvaluatorType::COREIR / `--coreir`). Each eval() call lowers the
 * expression to a CoreIR function, reuses or compiles a bytecode artifact, and
 * executes it against the shared environment. Top-level definitions persist
 * across REPL lines, exactly as the direct bytecode path does.
 */
#pragma once

#include "i_evaluator.h"
#include "compilation.h"

namespace coreir {

class CoreIrEvaluator : public zombie::runtime::IEvaluator {
public:
    explicit CoreIrEvaluator(zombie::runtime::Environment* env, bool debug = false);

    zombie::runtime::Object* eval(zombie::runtime::Object* expr,
                                  zombie::runtime::Environment* env) override;
    zombie::runtime::Environment* get_environment() override { return env_; }

    zombie::compilation::CompilationPipeline& pipeline() noexcept { return pipeline_; }

private:
    zombie::runtime::Environment* env_;   // shared global environment
    zombie::compilation::CompilationPipeline pipeline_;
};

} // namespace coreir
