#include "zombie_runtime_types.h"
#include "coreir_evaluator.h"

namespace coreir {
CoreIrEvaluator::CoreIrEvaluator(zombie::runtime::Environment* env, bool debug)
    : env_(env), pipeline_(std::make_unique<zombie::compilation::BytecodeCompilerBackend>(debug)) {}

zombie::runtime::Object* CoreIrEvaluator::eval(zombie::runtime::Object* expr,
                                             zombie::runtime::Environment* env) {
    return pipeline_.evaluate(expr, env ? env : env_, zombie::compilation::OutputKind::Bytecode);
}
} // namespace coreir
