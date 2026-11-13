#include "pch.h"
#include "scheme_fn.h"
#include "scheme_engine.h"
#include "value_codec.h"
#include "i_evaluator.h"
#include "Environment.h"
#include "Base.h"
#include "Lisp.h"

// Use the embedding API's engine management
namespace embedding {
    scheme_engine* get_default_engine();
}

scheme_fn::scheme_fn()
    : m_root(nullptr) {
}

scheme_fn::scheme_fn(Object* proc)
    : m_root(proc) {
    if (!verify_procedure()) {
        m_last_error = "scheme_fn: Object is not a Procedure";
        m_root = gc_persistent_root(nullptr);  // Reset to null
    }
}

bool scheme_fn::is_valid() const {
    return m_root.is_valid() && verify_procedure();
}

bool scheme_fn::verify_procedure() const {
    Object* proc = static_cast<Object*>(m_root.get());
    if (!proc) return false;

    // Check if it's a Procedure type
    Procedure* p = dynamic_cast<Procedure*>(proc);
    return p != nullptr;
}

Object* scheme_fn::get_procedure() const {
    return static_cast<Object*>(m_root.get());
}

Object* scheme_fn::call(Object* args, Environment* env) {
    clear_error();

    if (!is_valid()) {
        m_last_error = "scheme_fn::call: invalid Procedure";
        return error(m_last_error);
    }

    Procedure* proc = dynamic_cast<Procedure*>(m_root.get());
    if (!proc) {
        m_last_error = "scheme_fn::call: not a Procedure";
        return error(m_last_error);
    }

    // Get embedding engine (creates default if needed)
    scheme_engine* engine = embedding::get_default_engine();

    // Use provided environment or engine's global env
    Environment* eval_env = env ? env : engine->get_environment();

    // Apply the Procedure using the engine's evaluator
    try {
        Object* result = proc->apply(engine->get_evaluator(), args, eval_env);

        // Check if result is an error
        a_string* err = dynamic_cast<a_string*>(result);
        if (err && err->value().find("ERROR:") == 0) {
            m_last_error = err->value();
        }

        return result;
    } catch (const std::exception& e) {
        m_last_error = std::string("scheme_fn::call exception: ") + e.what();
        return error(m_last_error);
    }
    // Non-std exceptions (e.g. ContinuationInvoked) propagate to the caller
    // rather than being silently converted to an in-band error string.
}
