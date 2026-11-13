#pragma once

#include "coreir.h"
#include "Object.h"
#include "Environment.h"
#include "expander.h"
#include <memory>
#include <string>

// Forward declarations
class ConsCell;

namespace coreir {

// ============================================================================
// Scheme → CoreIR Lowering (with Expander Integration)
// ============================================================================
//
// Architecture: Expander-First Design
//
// This lowering pass integrates the Expander to handle macros and desugaring
// BEFORE lowering to CoreIR. This ensures that:
//   1. Macro expansion is shared across all backends (CoreIR, Bytecode, Interpreter)
//   2. CoreIR only needs to handle core forms (no let*, case, macros, etc.)
//   3. Future backends get Macro support for free
//
// Pipeline:
//   Scheme Code → [Expander] → Expanded Scheme → [This Lowering] → CoreIR
//

class SchemeToCoreirLowering {
public:
    // Constructor takes environment for Macro expansion
    // If env is nullptr, operates without expansion (useful for testing core forms)
    SchemeToCoreirLowering(Environment* env = nullptr);

    // Lower a Scheme expression to CoreIR
    // Returns a CoreIRFunction for top-level definitions
    // or a statement for inline expressions
    std::unique_ptr<CoreIRFunction> lower_define(Object* scheme_expr);

    // Lower a Scheme expression to a CoreIR statement
    std::unique_ptr<Stmt> lower_expr(Object* scheme_expr);

    // Lower an arbitrary Scheme expression to an executable CoreIRFunction
    // Wraps the expression in a zero-argument function that can be compiled and executed
    // Example: (+ 1 2) becomes (define (__main__) (+ 1 2))
    std::unique_ptr<CoreIRFunction> lower_expression_to_function(Object* scheme_expr,
                                                                   const std::string& func_name = "__main__");

    // Lower a Scheme expression to a CoreIR Atom
    // If the expression is complex, creates Let bindings and returns the temporary variable
    Atom lower_to_atom(Object* scheme_expr, std::unique_ptr<Stmt>& Continuation);

private:
    Environment* env_;          // Environment for Macro expansion (can be nullptr)
    Expander* expander_;       // Macro expander (created if env_ is not null)
    int temp_counter_;         // For generating unique temporary variable names

    // Generate a unique temporary variable name
    std::string gen_temp();

    // Expand expression using the expander (if available)
    // Returns the expanded form, or the original if no expander
    Object* expand(Object* expr);

    // Check if an expression is atomic (doesn't need to be normalized)
    bool is_atomic(Object* expr);

    // Lower specific forms
    std::unique_ptr<Stmt> lower_if(Object* cond, Object* then_expr, Object* else_expr);
    std::unique_ptr<Stmt> lower_let(Object* bindings, Object* body);
    std::unique_ptr<Stmt> lower_begin(Object* exprs);
    std::unique_ptr<Stmt> lower_cond(Object* clauses);
    std::unique_ptr<Stmt> lower_let_star(Object* bindings, Object* body);
    std::unique_ptr<Stmt> lower_letrec(Object* bindings, Object* body);
    std::unique_ptr<Stmt> lower_set(Object* var, Object* value);
    std::unique_ptr<Stmt> lower_values(Object* args);
    std::unique_ptr<Stmt> lower_call_with_values(Object* producer, Object* consumer);
    std::unique_ptr<Stmt> lower_lambda_body(Object* body);
    std::unique_ptr<Expr> lower_lambda(Object* params, Object* body);
    std::unique_ptr<Expr> lower_primitive_call(const std::string& op, Object* args);
    std::unique_ptr<Expr> lower_call(Object* func, Object* args);
    std::unique_ptr<Expr> lower_expr_to_expr(Object* scheme_expr);

    // Helper: Normalize Primitive call with potentially complex arguments
    std::unique_ptr<Stmt> lower_primitive_call_stmt(const std::string& op, Object* args);

    // Helper: Convert cons-list to vector
    template<typename T>
    std::vector<T> cons_to_vector(Object* lst, T (*converter)(Object*));

    // Helper: Extract symbol name from Object
    std::string get_symbol_name(Object* sym);

    // Helper: Check if symbol is a Primitive operation
    bool is_primitive_op(const std::string& name);

    // Helper: Infer type hint from Scheme Object
    TypeHint infer_type(Object* obj);

    // Helper: Mark tail calls in a statement (mutates the statement tree)
    void mark_tail_calls(Stmt* stmt);

    // Helper: Extract result variable name from a Stmt chain (finds the Return statement)
    std::string extract_result_var(Stmt* stmt);

    // Helper: Inline a stmt chain before Continuation (replaces Return with Continuation)
    std::unique_ptr<Stmt> inline_stmt_chain(std::unique_ptr<Stmt> chain, std::unique_ptr<Stmt> Continuation);
};

} // namespace coreir
