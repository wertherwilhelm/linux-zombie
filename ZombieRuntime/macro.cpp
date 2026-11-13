#include "pch.h"

#include "Macro.h"
#include "gc_manager.h"
#include "gc_ptr.h"

namespace zombie { namespace runtime {

Macro::Macro()
    : Closure()  // Explicitly call Base constructor
{
    m_is_macro = true;
}

Macro::~Macro() {
}

/** Make a Macro from a parameter List, body, and environment. **/
Macro::Macro (Object *parms, Object *body, Environment*env)
    : Closure(parms, body, env)
{
    m_is_macro = true;  // Set flag so instance_of() returns MACRO
}

// Macro apply: don't evaluate args, bind them directly, evaluate body, then evaluate result
Object* Macro::apply(IEvaluator* evaluator, Object* args, Environment* env) {
    // Create environment with unevaluated args bound to params
    Environment* macro_env = m_env->extend(m_parms, args);
    macro_env->set_parent(m_env);

    // Evaluate the Macro body to get the expansion
    Object* expansion = evaluator->eval(m_body, macro_env);

    // Evaluate the expansion in the caller's environment
    Object* result = evaluator->eval(expansion, env);

    return result;
}

/** Replace the old cons cell with the Macro expansion, and return it. **/
ConsCell *Macro::expand(Lisp *interpreter, ConsCell *oldPair, Object *args, Environment*env) {
    Object *expansion = apply(interpreter, args, env);
    if (!expansion->is_atom()) {
        oldPair->car(((ConsCell*)expansion)->car());
        oldPair->cdr(((ConsCell*)expansion)->cdr());
    } else {
        gc_root<a_string> begin_sym(gc_new<a_string>("begin"));
        oldPair->car(begin_sym.get());
        oldPair->cdr(cons(expansion, NULL));
    }
    return oldPair;
}

/** Macro expand an expression **/
Object* Macro::macro_expand(IEvaluator *evaluator, Object *x) {
    // If x is an atom (not a list), return it unchanged
    if (x->is_atom()) {
        return x;
    }
    // x is a list - evaluate the first element to see if it's a Macro
    Object *fn = evaluator->eval(first(x), evaluator->get_environment());
    // Check if fn is a Macro
    Macro* m = dynamic_cast<Macro*>(fn);
    if (!m) {
        return x;  // Not a Macro, return unchanged
    }
    // It's a Macro - expand it (but don't evaluate the expansion)
    Environment* macro_env = m->m_env->extend(m->m_parms, rest(x));
    macro_env->set_parent(m->m_env);
    Object* expansion = evaluator->eval(m->m_body, macro_env);
    return expansion;  // Return the expansion, not the evaluated result
}

} } // namespace zombie::runtime
