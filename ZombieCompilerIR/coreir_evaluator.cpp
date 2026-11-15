#include "zombie_runtime_types.h"
#include "coreir_evaluator.h"
#include "scheme_to_coreir.h"
#include "coreir_to_bytecode.h"

namespace coreir {

using zombie::runtime::Object;
using zombie::runtime::Environment;

CoreIrEvaluator::CoreIrEvaluator(Environment* env, bool debug)
    : env_(env), vm_(env, debug) {}

Object* CoreIrEvaluator::eval(Object* expr, Environment* env) {
    Environment* e = env ? env : env_;

    // Scheme -> CoreIR (lowering runs the Expander internally for macros/desugar)
    SchemeToCoreirLowering lowering(e);
    auto func = lowering.lower_expression_to_function(expr, "__main__");

    // CoreIR -> bytecode
    CoreIRToBytecode translator;
    auto compiled = translator.compile_function(*func);

    // Execute on the backend VM against the shared environment.
    return vm_.execute(compiled.bytecode, compiled.constants, e);
}

} // namespace coreir
