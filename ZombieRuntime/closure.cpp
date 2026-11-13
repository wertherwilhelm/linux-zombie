#include "pch.h"
// Closure.cpp: implementation of the Closure class.
//
//////////////////////////////////////////////////////////////////////

#include "closure.h"
#include "Macro.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include "bytecode_vm.h"
#include "coreir.h"  // Include for complete type of coreir::Stmt

namespace zombie { namespace runtime {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Closure::Closure()
    : m_coreir_body(nullptr)
    , m_is_macro(false)
{

}

Closure::~Closure()
{
    // Clean up CoreIR body if present
    // Note: delete nullptr is safe, but we check explicitly for clarity
    if (m_coreir_body != nullptr) {
        delete m_coreir_body;
        m_coreir_body = nullptr;
    }
}

void Closure::set_coreir(coreir::Stmt* body) {
    delete m_coreir_body;  // Clean up old body if any
    m_coreir_body = body;   // Take ownership
}

Closure::Closure (Object *parms, Object *body, Environment *env)
    : m_coreir_body(nullptr)
    , m_is_macro(false)
{
    m_parms = parms;
    m_env = env;
    // Check if body is a single-element list by comparing rest with nil
    // If body = ((expr)), unwrap to just (expr)
    // Otherwise wrap in begin: (begin expr1 expr2 ...)
    Object* rest_val = body->is_atom() ? NULL : rest(body);
    m_body = (rest_val != NULL && (rest_val == nil || rest_val->is_nil()))
        ? first(body)
        : cons(gc_new<a_string>("begin"), body);
}

void Closure::print(std::ostream& os) const
{
    os << m_body;
}

Object* Closure::clone() const
{
    Closure* p = gc_new<Closure>(m_parms, m_body, m_env);
    p->m_is_macro = m_is_macro;
    p->m_bytecode = m_bytecode;
    p->m_constants = m_constants;
    // Note: CoreIR body contains unique_ptr which can't be copied
    // If needed, we would need to implement deep cloning of CoreIR Stmt
    // For now, cloned closures won't have CoreIR bodies
    p->m_coreir_body = nullptr;  // Explicitly ensure no CoreIR body in clone
    return p;
}

Object* Closure::apply(IEvaluator* evaluator, Object* args, Environment* env)
{
    // NOTE: args are already evaluated by the caller (eval_args), so we don't evaluate them again!
    // We just need to bind the formal parameters to the actual argument values

    // Create a new environment that extends the defining environment with the Procedure's parameters bound to args
    Environment* call_env = m_env->extend(m_parms, args);
    // Note: extend() already sets the parent to m_env, no need to set it again

    // OPTIMIZATION: If we have compiled bytecode and the evaluator is a BytecodeVM,
    // execute the bytecode directly instead of re-compiling the AST body
    if (has_bytecode()) {
        zombie::bytecode::BytecodeVM* vm = dynamic_cast<zombie::bytecode::BytecodeVM*>(evaluator);
        if (vm) {
            // Execute the precompiled bytecode in the call environment
            Object* result = vm->execute(m_bytecode, m_constants, call_env);

            // If this is a Macro, the result is code that needs to be evaluated
            if (m_is_macro) {
                result = evaluator->eval(result, env);
            }

            return result;
        }
    }

    // Fallback: Evaluate the body AST in the new environment (for non-bytecode evaluators)
    Object* result = evaluator->eval(m_body, call_env);

    // If this is a Macro, the result is code that needs to be evaluated
    // Use the m_is_macro flag since it doesn't rely on vtable dispatch
    if (m_is_macro) {
        result = evaluator->eval(result, env);
    }

    // Return the result
    return result;
}


// env isn't used right now, just trying to a


void Closure::trace_references(Tracer& tracer)
{
    // Mark parameters, body, and environment
    tracer.visit_field(m_parms);
    tracer.visit_field(m_body);

    // Environment needs cast handling
    if (m_env) {
        Object* env_obj = (Object*)m_env;
        tracer.visit_field(env_obj);
        m_env = (Environment*)env_obj;  // Update after potential move
    }

    // Mark bytecode constants if present (iterate by reference to update in-place)
    for (Object*& constant : m_constants) {
        tracer.visit_field(constant);
    }

    // Mark CoreIR captured environment values if present
    // CoreEnv is std::unordered_map<std::string, Object*>
    for (auto& [name, value] : m_coreir_captured_env) {
        if (value) {
            tracer.visit_field(value);
        }
    }
}

} } // namespace zombie::runtime
