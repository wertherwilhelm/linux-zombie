#pragma once

#include "Object.h"
#include "Environment.h"

namespace zombie { namespace runtime {

/**
 * @file expander.h
 * @brief Macro expansion and desugaring phase.
 *
 * The Expander separates syntactic transformations from evaluation.
 * It runs BEFORE the VM evaluates code, transforming:
 * - Macros (syntax-rules) into their expanded forms
 * - Desugaring forms (let, let*, letrec, case) into core forms
 *
 * This separation is important because:
 * - Some forms need lazy evaluation (if, and, or, cond, begin)
 * - Other forms can be eagerly transformed to simpler constructs
 *
 * Design:
 * - Expander does NOT evaluate code, only transforms syntax
 * - VM handles all evaluation, including lazy special forms
 * - Macro expansion happens recursively until a fixed point
 *
 * @code
 * Expander exp(env);
 * Object* expanded = exp.expand(expr);  // Transform before eval
 * Object* result = vm.eval(expanded, env);
 * @endcode
 */

/**
 * @brief Macro expansion and desugaring engine.
 */
class Expander {
public:
    explicit Expander(Environment* env);

    /**
     * @brief Expand macros and desugar an expression.
     * @param expr Expression to expand.
     * @return Transformed expression ready for VM evaluation.
     */
    Object* expand(Object* expr);

private:
    Environment* env_;

    // -------------------------------------------------------------------------
    // Recursive expansion
    // -------------------------------------------------------------------------

    /**
     * @brief Expand a list (function call or special form).
     */
    Object* expand_list(Object* list);

    /**
     * @brief Expand an atom (variable, literal, etc.).
     */
    Object* expand_atom(Object* atom);

    // -------------------------------------------------------------------------
    // Desugaring transformations
    // -------------------------------------------------------------------------

    /**
     * @brief Transform (let ((var val) ...) body...) into core form.
     *
     * (let ((var val) ...) body...)
     *     -> ((lambda (var...) body...) val...)
     */
    Object* expand_let(Object* bindings, Object* body);

    /**
     * @brief Transform (let* ((var val) ...) body...) into nested let forms.
     */
    Object* expand_let_star(Object* bindings, Object* body);

    /**
     * @brief Transform (letrec ((var val) ...) body...) into core form.
     *
     * (letrec ((var val) ...) body...)
     *     -> (let ((var nil) ...) (set! var val) ... body...)
     */
    Object* expand_letrec(Object* bindings, Object* body);

    /**
     * @brief Transform (case key ((datum...) result) ...) into core form.
     *
     * (case key ((datum...) result) ...)
     *     -> (cond ((memv key '(datum...)) result) ...)
     */
    Object* expand_case(Object* key_expr, Object* clauses);

    /**
     * @brief Transform (cond-expand ...) to the first matching clause body.
     *
     * R7RS conditional feature expansion (Section 4.2.1)
     */
    Object* expand_cond_expand(Object* clauses);

    /**
     * @brief Transform (do ...) into a named let with loop.
     */
    Object* expand_do(Object* bindings, Object* test_clause, Object* body);

    /**
     * @brief Transform quasiquote expression.
     *
     * Handles unquote (,) and unquote-splicing (,@).
     */
    Object* expand_quasiquote(Object* expr);

    /**
     * @brief Transform (and e1 e2 ...) into nested if statements.
     *
     * (and) -> #t
     * (and e) -> e
     * (and e1 e2 e3) -> (if e1 (and e2 e3) #f)
     */
    Object* expand_and(Object* args);

    /**
     * @brief Transform (or e1 e2 ...) into nested if statements.
     *
     * (or) -> #f
     * (or e) -> e
     * (or e1 e2 e3) -> (let ((tmp e1)) (if tmp tmp (or e2 e3)))
     */
    Object* expand_or(Object* args);

    /**
     * @brief Transform (when test expr1 expr2 ...) into core form.
     *
     * (when test expr1 expr2 ...) -> (if test (begin expr1 expr2 ...))
     * R7RS conditional execution (Section 4.2.1)
     */
    Object* expand_when(Object* test, Object* body);

    /**
     * @brief Transform (unless test expr1 expr2 ...) into core form.
     *
     * (unless test expr1 expr2 ...) -> (if (not test) (begin expr1 expr2 ...))
     * R7RS conditional execution (Section 4.2.1)
     */
    Object* expand_unless(Object* test, Object* body);

    /**
     * @brief Transform (guard ...) into core form.
     *
     * R7RS exception handling (Section 6.11)
     * Note: Full implementation requires continuations; this is a simplified version
     */
    Object* expand_guard(Object* var, Object* clauses, Object* body);

    /**
     * @brief Transform (case-lambda ((formals body ...) ...)).
     *
     * R7RS multi-arity procedures (Section 4.2.9).
     * Returns a case-lambda Procedure Object.
     */
    Object* expand_case_lambda(Object* clauses);

    // -------------------------------------------------------------------------
    // Local Macro handling (let-syntax, letrec-syntax)
    // -------------------------------------------------------------------------

    /**
     * @brief Expand let-syntax by creating a local Macro environment.
     */
    Object* expand_let_syntax(Object* bindings, Object* body);

    /**
     * @brief Expand letrec-syntax with mutually recursive macros.
     */
    Object* expand_letrec_syntax(Object* bindings, Object* body);

    // -------------------------------------------------------------------------
    // Macro expansion
    // -------------------------------------------------------------------------

    /**
     * @brief Try to expand expr as a Macro invocation.
     * @return Expanded form if expr is a Macro, nullptr otherwise.
     */
    Object* try_expand_macro(Object* expr);

    /**
     * @brief Check if symbol is bound to a Macro transformer.
     */
    bool is_macro(Object* symbol);

    /**
     * @brief Get the Macro transformer for a symbol.
     */
    Object* get_macro_transformer(Object* symbol);
};

} } // namespace zombie::runtime
