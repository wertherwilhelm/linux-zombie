#pragma once

#include "Base.h"
#include "Environment.h"
#include "Object.h"

namespace zombie { namespace runtime {

// Forward declarations
class IEvaluator;

/**
 * @file SyntaxRulesTransformer.h
 * @brief Hygienic syntax-rules Macro transformer (R5RS/R7RS).
 */

/**
 * @brief R5RS syntax-rules transformer for hygienic macros.
 *
 * Stores transformation rules and performs Macro expansion by:
 * - Matching an input form against a list of pattern/template pairs.
 * - Producing an output form with pattern variables substituted.
 * - Preserving lexical hygiene by gensym-based renaming of introduced identifiers.
 *
 * This transformer does not evaluate; it only rewrites syntax.
 *
 * @details
 * Example Macro (typical syntax-rules shape):
 * @code
 * (define-syntax when
 *   (syntax-rules ()
 *     ((when test expr ...)
 *      (if test (begin expr ...)))))
 * @endcode
 *
 * Example with ellipsis:
 * @code
 * (define-syntax and
 *   (syntax-rules ()
 *     ((and) #t)
 *     ((and x) x)
 *     ((and x y ...) (if x (and y ...) #f))))
 * @endcode
 *
 * Literals list:
 * - Identifiers in the literals list must match exactly in patterns.
 * - Non-literal identifiers are treated as pattern variables.
 *
 * Ellipsis handling:
 * - "..." (and ".." as accepted alias) repeats the preceding pattern segment.
 * - Repeated variables are bound to lists of matches and re-expanded in templates.
 *
 * Hygiene:
 * - Identifiers introduced by the transformer are renamed to avoid accidental
 *   capture of user bindings, while referenced identifiers in the input keep
 *   their original binding.
 *
 * Matching limitations:
 * - This implementation supports a single ellipsis marker ("..." or "..") and
 *   does not attempt full R7RS pattern disambiguation.
 * - Nested ellipsis forms are supported only to the extent required by the
 *   current expander; complex repetition patterns may not match.
 */
class SyntaxRulesTransformer : public Base {
public:
    /**
     * @brief Construct an empty transformer.
     */
    SyntaxRulesTransformer();

    /**
     * @brief Construct a transformer with literals and rules.
     * @param literals List of literal identifiers.
     * @param rules List of (pattern template) pairs.
     */
    SyntaxRulesTransformer(Object* literals, Object* rules);

    /**
     * @brief Destroy the transformer.
     */
    virtual ~SyntaxRulesTransformer();

    /**
     * @brief Expand a Macro call by matching and applying a rule.
     * @param input Macro invocation form.
     * @param env Environment for expansion (hygiene context).
     * @return Expanded form or nullptr on failure.
     */
    Object* expand(Object* input, Environment* env);

    /**
     * @brief Return runtime instance type code.
     */
    int instance_of() override { return static_cast<int>(Base::atype::SYNTAX_RULES_TRANSFORMER); }

    /**
     * @brief Treated as an atom in the Object system.
     */
    int is_atom() const override { return 1; }  // Treated as atomic value

private:
    Object* m_literals;  // List of literal identifier symbols
    Object* m_rules;     // List of (pattern template) pairs

    /**
     * @brief Match a pattern against input.
     * @return Bindings or nullptr if no match.
     */
    Object* match_pattern(Object* pattern, Object* input);

    /**
     * @brief Check if a symbol is in the literals list.
     */
    bool is_literal(Object* sym);

    /**
     * @brief Check if Object is an ellipsis symbol ("..." or "..").
     */
    bool is_ellipsis_symbol(Object* obj);

    /**
     * @brief Expand a template by substituting bindings.
     */
    Object* expand_template(Object* templ, Object* bindings, Environment* env);

    /**
     * @brief Check if pattern contains ellipsis.
     */
    bool has_ellipsis(Object* pattern);

    /**
     * @brief Match pattern with ellipsis support.
     */
    Object* match_with_ellipsis(Object* pattern, Object* input);

    /**
     * @brief Expand template with ellipsis support.
     */
    Object* expand_with_ellipsis(Object* templ, Object* bindings);

    /**
     * @brief Generate hygienic identifiers for introduced bindings.
     */
    int m_gensym_counter;
    Object* make_hygienic(Object* expr, Object* pattern_vars, Environment* macro_env);

    /**
     * @brief Hygiene: alpha-rename the identifiers a template introduces as
     * binders (via let, let-star, letrec, letrec-star, lambda, do, including
     * named let) to fresh gensyms, so they cannot capture user identifiers passed
     * in as macro arguments. Pattern variables, literals, syntactic keywords and
     * free references are left untouched (never in the binder scope map).
     * scope is an alist of (original-symbol . gensym-symbol) in scope.
     */
    Object* hyg_expr(Object* e, Object* bindings, Object* scope);
    Object* hyg_list(Object* lst, Object* bindings, Object* scope);
    // Rename introduced binders in a formals/binding-name structure, extending
    // `scope` (in/out) with the fresh names.
    Object* hyg_formals(Object* formals, Object* bindings, Object*& scope);
    // Rename a list of ((v e) ...) bindings: binder v (if introduced) -> gensym
    // added to `bscope`; init e renamed in bscope when `seq` (let*) else initScope.
    Object* hyg_let_bindings(Object* binds, Object* bindings, Object* initScope,
                             Object*& bscope, bool seq);
    // A symbol is an introduced binder candidate if it's a symbol that is not a
    // pattern variable, not a literal, and not the ellipsis.
    bool hyg_is_introduced(Object* sym, Object* bindings);
    Object* hyg_fresh(Object* orig);  // fresh gensym symbol for an original binder

    /**
     * @brief Lookup binding for a pattern variable.
     */
    Object* lookup_binding(Object* var, Object* bindings);

    /**
     * @brief Add a binding for a pattern variable.
     */
    Object* add_binding(Object* var, Object* value, Object* bindings);

    /**
     * @brief Collect pattern variables referenced by a template.
     */
    Object* collect_pattern_vars(Object* templ, Object* bindings);
};

} } // namespace zombie::runtime
