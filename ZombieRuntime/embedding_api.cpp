#include "pch.h"
#include "embedding_api.h"
#include "scheme_engine.h"
#include "scheme_reader.h"
#include "Lisp.h"
#include "i_evaluator.h"
#include "Base.h"
#include "cons_cell.h"
#include "gc_manager.h"
#include "gc_ptr.h"


// ============================================================================
// Embedding Engine Management
// ============================================================================

// Thread-local embedding engine (can be configured per-thread)
static thread_local scheme_engine* tls_embedding_engine = nullptr;

// Global default engine (created lazily if no thread-local set)
static scheme_engine* g_default_embedding_engine = nullptr;

// Get the current embedding engine (creates default if needed)
static scheme_engine* get_embedding_engine() {
    // Prefer thread-local engine
    if (tls_embedding_engine) {
        return tls_embedding_engine;
    }

    // Fall back to global default, create lazily if needed
    if (!g_default_embedding_engine) {
        g_default_embedding_engine = new scheme_engine();
    }
    return g_default_embedding_engine;
}

namespace embedding {

// ============================================================================
// Public API for configuring embedding engine
// ============================================================================

void set_embedding_engine(scheme_engine* engine) {
    tls_embedding_engine = engine;
}

scheme_engine* get_default_engine() {
    return get_embedding_engine();
}

// ============================================================================
// Compilation Functions
// ============================================================================

scheme_fn compile_proc(const std::string& code, bool expand_macros) {
    scheme_engine* engine = get_embedding_engine();

    // Parse the source
    Object* expr = engine->read_string(code);

    // Check for parse error
    if (!expr) {
        return scheme_fn();  // Invalid
    }

    a_string* err = dynamic_cast<a_string*>(expr);
    if (err && err->value().find("ERROR:") == 0) {
        return scheme_fn();  // Invalid
    }

    // Optionally expand macros
    if (expand_macros) {
        // TODO: Hook into Macro expansion system
        // For now, skip Macro expansion
    }

    // Evaluate the expression
    Object* proc = engine->eval_expr(expr);

    // Check if evaluation resulted in error
    if (!proc) {
        return scheme_fn();  // Invalid
    }

    err = dynamic_cast<a_string*>(proc);
    if (err && err->value().find("ERROR:") == 0) {
        return scheme_fn();  // Invalid
    }

    // Verify it's a Procedure
    Procedure* p = dynamic_cast<Procedure*>(proc);
    if (!p) {
        return scheme_fn();  // Invalid - not a Procedure
    }

    // Wrap in scheme_fn
    return scheme_fn(proc);
}

scheme_fn compile_thunk_proc(const std::string& expr, bool expand_macros) {
    // Wrap expression as (lambda () <expr>)
    std::string thunk_code = "(lambda () " + expr + ")";
    return compile_proc(thunk_code, expand_macros);
}

scheme_fn compile_program_proc(const std::string& program, bool expand_macros) {
    scheme_engine* engine = get_embedding_engine();

    // Parse the program (multiple forms)
    Object* forms = engine->read_program(program);

    // Check for parse error
    if (!forms) {
        return scheme_fn();  // Invalid
    }

    a_string* err = dynamic_cast<a_string*>(forms);
    if (err && err->value().find("ERROR:") == 0) {
        return scheme_fn();  // Invalid
    }

    // Wrap forms in (begin ...) if multiple forms
    Object* expr;
    if (forms == nil) {
        // No forms - error
        return scheme_fn();
    } else if (cdr(forms) == nil) {
        // Single form - use directly
        expr = car(forms);
    } else {
        // Multiple forms - wrap in begin
        gc_root<a_string> begin_sym(gc_new<a_string>("begin"));
        expr = cons(begin_sym.get(), forms);
    }

    // Optionally expand macros
    if (expand_macros) {
        // TODO: Hook into Macro expansion system
    }

    // Evaluate the program
    Object* result = engine->eval_expr(expr);

    // Check if evaluation resulted in error
    if (!result) {
        return scheme_fn();  // Invalid
    }

    err = dynamic_cast<a_string*>(result);
    if (err && err->value().find("ERROR:") == 0) {
        return scheme_fn();  // Invalid
    }

    // Verify last value is a Procedure
    Procedure* p = dynamic_cast<Procedure*>(result);
    if (!p) {
        return scheme_fn();  // Invalid - result not a Procedure
    }

    // Wrap in scheme_fn
    return scheme_fn(result);
}

} // namespace embedding
