#include "pch.h"
#include "expander.h"
#include "cons_cell.h"
#include "atom.h"
#include "constants.h"  // For intern_symbol()
#include "Macro.h"
#include "syntax_rules_transformer.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include <iostream>
#include <functional>

namespace zombie { namespace runtime {

// ============================================================================
// Helper functions
// ============================================================================

// Helper: Check if an expression is a specific keyword (handles both string and symbol)
static inline bool is_keyword(Object* expr, const std::string& keyword) {
    return (is_string(expr) && ((a_string*)expr)->value() == keyword) ||
           (is_symbol(expr) && ((a_symbol*)expr)->value() == keyword);
}

// Helper: Extract identifier name from symbol or string
static inline std::string get_identifier_name(Object* obj) {
    if (is_symbol(obj)) {
        return ((a_symbol*)obj)->value();
    } else if (is_string(obj)) {
        return ((a_string*)obj)->value();
    }
    return "";
}

// ============================================================================
// Expander class
// ============================================================================

Expander::Expander(Environment* env)
    : env_(env) {
}

// ============================================================================
// Main entry point
// ============================================================================

Object* Expander::expand(Object* expr) {
    if (!expr) return expr;

    // Atoms expand to themselves (or Macro expansion for symbols)
    if (expr->is_atom()) {
        return expand_atom(expr);
    }

    // Lists might be special forms, Macro calls, or function calls
    return expand_list(expr);
}

// ============================================================================
// Recursive expansion
// ============================================================================

Object* Expander::expand_atom(Object* atom) {
    // Atoms (numbers, strings, symbols) expand to themselves
    // Note: We don't do variable lookup here - that's the VM's job
    return atom;
}

Object* Expander::expand_list(Object* list) {
    if (is_nil(list) || list->is_nil()) {
        return list;
    }

    // List must be a pair (not an atom like number or symbol)
    if (!list->pair()) {
        throw std::runtime_error("expand_list: expected list but got atom");
    }

    Object* first_elem = first(list);
    Object* args = rest(list);

    // Check if it's a special form that needs desugaring
    std::string name = get_identifier_name(first_elem);

    // Desugaring forms - transform to core language
    if (name == "let") {
        // Validate args before destructuring
        if (is_nil(args) || !args->pair()) {
            throw std::runtime_error("let: malformed syntax - expected (let bindings body...)");
        }

        // Check for named let: (let name ((var val) ...) body...)
        // vs regular let: (let ((var val) ...) body...)
        // Named let has a symbol as first arg, regular let has a list
        // Note: nil is an atom, but (let () body) is regular let, not named let
        if (first(args)->is_atom() && !is_nil(first(args))) {
            // Named let - don't desugar, but DO expand sub-expressions
            // (let name ((var val) ...) body...) stays as named let
            // but bindings values and body expressions get expanded
            Object* loop_name = first(args);
            Object* bindings = second(args);
            Object* body_exprs = rest(rest(args));

            // Expand binding values
            Object* expanded_bindings = nil;
            Object* bindings_tail = nil;
            for (Object* b = bindings; !is_nil(b); b = rest(b)) {
                if (!b->pair()) {
                    throw std::runtime_error("named let: bindings must be a proper list");
                }
                Object* binding = first(b);
                if (!binding->pair()) {
                    throw std::runtime_error("named let: each binding must be a pair (var val)");
                }
                Object* var = first(binding);
                Object* val = second(binding);
                Object* expanded_val = expand(val);
                Object* new_binding = cons(var, cons(expanded_val, nil));
                Object* cell = cons(new_binding, nil);
                if (expanded_bindings == nil) {
                    expanded_bindings = cell;
                    bindings_tail = cell;
                } else {
                    set_rest(bindings_tail, cell);
                    bindings_tail = cell;
                }
            }

            // Expand body expressions
            Object* expanded_body = nil;
            Object* body_tail = nil;
            for (Object* e = body_exprs; !is_nil(e); e = rest(e)) {
                if (!e->pair()) {
                    throw std::runtime_error("named let: body must be a proper list of expressions");
                }
                Object* expanded_expr = expand(first(e));
                Object* cell = cons(expanded_expr, nil);
                if (expanded_body == nil) {
                    expanded_body = cell;
                    body_tail = cell;
                } else {
                    set_rest(body_tail, cell);
                    body_tail = cell;
                }
            }

            // Reconstruct: (let name expanded-bindings expanded-body...)
            return cons(first_elem, cons(loop_name, cons(expanded_bindings, expanded_body)));
        }

        // Regular let
        Object* bindings = first(args);
        Object* body = rest(args);
        return expand_let(bindings, body);
    }

    if (name == "let*") {
        // Validate args before destructuring
        if (is_nil(args) || !args->pair()) {
            throw std::runtime_error("let*: malformed syntax - expected (let* bindings body...)");
        }
        Object* bindings = first(args);
        Object* body = rest(args);
        return expand_let_star(bindings, body);
    }

    if (name == "letrec") {
        // Validate args before destructuring
        if (is_nil(args) || !args->pair()) {
            throw std::runtime_error("letrec: malformed syntax - expected (letrec bindings body...)");
        }
        Object* bindings = first(args);
        Object* body = rest(args);
        return expand_letrec(bindings, body);
    }

    if (name == "case") {
        Object* key_expr = first(args);
        Object* clauses = rest(args);
        return expand_case(key_expr, clauses);
    }

    if (name == "cond-expand") {
        // (cond-expand clause...)
        // Each clause is ((feature-requirement) body...)
        // Expands to the first clause whose feature requirement is satisfied
        return expand_cond_expand(args);
    }

    if (name == "do") {
        Object* bindings = first(args);
        Object* test_clause = first(rest(args));
        Object* body = rest(rest(args));
        return expand_do(bindings, test_clause, body);
    }

    if (name == "quasiquote") {
        Object* expr = first(args);
        return expand_quasiquote(expr);
    }

    if (name == "guard") {
        // (guard (var clause...) body...)
        if (is_nil(args) || !args->pair()) {
            throw std::runtime_error("guard: malformed syntax - expected (guard (var clause...) body...)");
        }
        Object* guard_spec = first(args);
        if (is_nil(guard_spec) || !guard_spec->pair()) {
            throw std::runtime_error("guard: expected (var clause...) as first argument");
        }
        Object* var = first(guard_spec);
        Object* clauses = rest(guard_spec);
        Object* body = rest(args);
        return expand_guard(var, clauses, body);
    }

    if (name == "case-lambda") {
        // (case-lambda ((formals body ...) ...))
        return expand_case_lambda(args);
    }

    // Global Macro definition - install transformer in environment
    if (name == "define-syntax") {
        Object* macro_name = first(args);
        Object* transformer_spec = second(args);

        // Parse (syntax-rules ...)
        if (transformer_spec->is_atom()) {
            throw std::runtime_error("define-syntax: requires syntax-rules form");
        }

        Object* sr_keyword = first(transformer_spec);
        std::string sr_name = get_identifier_name(sr_keyword);

        if (sr_name != "syntax-rules") {
            throw std::runtime_error("define-syntax: requires syntax-rules");
        }

        Object* sr_args = rest(transformer_spec);
        Object* literals = first(sr_args);
        Object* rules = rest(sr_args);

        // Create and install transformer in environment
        SyntaxRulesTransformer* transformer = gc_new<SyntaxRulesTransformer>(literals, rules);
        env_->define(macro_name, transformer);

        // Return a marker that define-syntax executed
        // The VM should treat this as a void result
        gc_root<a_symbol> begin_sym(intern_symbol("begin"));
        return cons(begin_sym.get(), nil);  // (begin) = void
    }

    // Local Macro forms - create local environment and expand body
    if (name == "let-syntax") {
        Object* bindings = first(args);
        Object* body = rest(args);
        return expand_let_syntax(bindings, body);
    }

    if (name == "letrec-syntax") {
        Object* bindings = first(args);
        Object* body = rest(args);
        return expand_letrec_syntax(bindings, body);
    }

    // Core forms that DON'T get desugared (lazy evaluation needed)
    // These pass through but we recursively expand their sub-expressions where appropriate
    if (name == "quote") {
        // Quote: don't expand the quoted expression
        return list;
    }

    if (name == "lambda") {
        // Lambda: expand the body but not the parameters
        Object* params = first(args);
        Object* body = rest(args);

        // Expand each body expression
        Object* expanded_body = nil;
        Object* expanded_tail = nil;
        for (Object* b = body; !is_nil(b); b = rest(b)) {
            Object* expanded_expr = expand(first(b));
            Object* cell = cons(expanded_expr, nil);
            if (expanded_body == nil) {
                expanded_body = cell;
                expanded_tail = cell;
            } else {
                set_rest(expanded_tail, cell);
                expanded_tail = cell;
            }
        }

        return cons(first_elem, cons(params, expanded_body));
    }

    // Desugar and/or to nested if statements
    if (name == "and") {
        return expand_and(args);
    }

    if (name == "or") {
        return expand_or(args);
    }

    // Desugar when/unless to if statements
    if (name == "when") {
        if (is_nil(args) || !args->pair()) {
            throw std::runtime_error("when: malformed syntax - expected (when test body...)");
        }
        Object* test = first(args);
        Object* body = rest(args);
        return expand_when(test, body);
    }

    if (name == "unless") {
        if (is_nil(args) || !args->pair()) {
            throw std::runtime_error("unless: malformed syntax - expected (unless test body...)");
        }
        Object* test = first(args);
        Object* body = rest(args);
        return expand_unless(test, body);
    }

    if (name == "if" || name == "begin" || name == "cond") {
        // These forms need lazy evaluation in the VM
        // But we still need to expand their sub-expressions
        Object* expanded_args = nil;
        Object* expanded_tail = nil;
        for (Object* a = args; !is_nil(a); a = rest(a)) {
            Object* expanded_expr = expand(first(a));
            Object* cell = cons(expanded_expr, nil);
            if (expanded_args == nil) {
                expanded_args = cell;
                expanded_tail = cell;
            } else {
                set_rest(expanded_tail, cell);
                expanded_tail = cell;
            }
        }

        return cons(first_elem, expanded_args);
    }

    if (name == "define") {
        // Define: expand the value expression
        Object* first_arg = first(args);

        // Check if it's shorthand form: (define (name ...) ...)
        if (first_arg->pair() && !first_arg->is_nil()) {
            // Shorthand form - expand the body
            Object* fn_name = first(first_arg);
            Object* params = rest(first_arg);
            Object* body = rest(args);

            // Expand body expressions
            Object* expanded_body = nil;
            Object* expanded_tail = nil;
            for (Object* b = body; !is_nil(b); b = rest(b)) {
                Object* expanded_expr = expand(first(b));
                Object* cell = cons(expanded_expr, nil);
                if (expanded_body == nil) {
                    expanded_body = cell;
                    expanded_tail = cell;
                } else {
                    set_rest(expanded_tail, cell);
                    expanded_tail = cell;
                }
            }

            // Reconstruct: (define (name params...) expanded-body...)
            Object* fn_spec = cons(fn_name, params);
            return cons(first_elem, cons(fn_spec, expanded_body));
        } else {
            // Simple form: (define var val) - expand the value
            Object* var_name = first_arg;
            Object* value_expr = first(rest(args));
            Object* expanded_value = expand(value_expr);
            return cons(first_elem, cons(var_name, cons(expanded_value, nil)));
        }
    }

    if (name == "set!") {
        // Set!: expand the value expression
        Object* var_name = first(args);
        Object* value_expr = first(rest(args));
        Object* expanded_value = expand(value_expr);
        return cons(first_elem, cons(var_name, cons(expanded_value, nil)));
    }

    // Try Macro expansion
    Object* macro_expanded = try_expand_macro(list);
    if (macro_expanded) {
        // Recursively expand the Macro result
        return expand(macro_expanded);
    }

    // Regular function call: expand all sub-expressions
    Object* expanded_list = nil;
    Object* expanded_tail = nil;
    for (Object* l = list; !is_nil(l); l = rest(l)) {
        Object* expanded_expr = expand(first(l));
        Object* cell = cons(expanded_expr, nil);
        if (expanded_list == nil) {
            expanded_list = cell;
            expanded_tail = cell;
        } else {
            set_rest(expanded_tail, cell);
            expanded_tail = cell;
        }
    }

    return expanded_list;
}

// ============================================================================
// Desugaring transformations
// ============================================================================

Object* Expander::expand_let(Object* bindings, Object* body) {
    // Transform: (let ((var val) ...) body...)
    //         → ((lambda (var...) body...) val...)

    // Validate bindings: must be nil or a proper list
    if (!is_nil(bindings) && !bindings->pair()) {
        throw std::runtime_error("let: bindings must be a list");
    }

    // R5RS requires at least one body expression
    if (is_nil(body)) {
        throw std::runtime_error("let: body cannot be empty");
    }

    // Body must be a list, not an atom
    if (!body->pair()) {
        throw std::runtime_error("let: body must be a list of expressions");
    }

    // Build vars and vals lists
    Object* vars_list = nil;
    Object* vals_list = nil;
    Object* vars_tail = nil;
    Object* vals_tail = nil;

    for (Object* b = bindings; !is_nil(b); b = rest(b)) {
        Object* binding = first(b);

        // Each binding must be a pair (var val)
        if (!binding->pair()) {
            throw std::runtime_error("let: each binding must be a list (var val)");
        }

        Object* var = first(binding);
        Object* val_pair = rest(binding);

        // Check that we have a value
        if (is_nil(val_pair) || !val_pair->pair()) {
            throw std::runtime_error("let: each binding must have a value");
        }

        Object* val = first(val_pair);

        // Append to vars list
        Object* var_cell = cons(var, nil);
        if (vars_list == nil) {
            vars_list = var_cell;
            vars_tail = var_cell;
        } else {
            set_rest(vars_tail, var_cell);
            vars_tail = var_cell;
        }

        // Expand and append to vals list
        Object* expanded_val = expand(val);
        Object* val_cell = cons(expanded_val, nil);
        if (vals_list == nil) {
            vals_list = val_cell;
            vals_tail = val_cell;
        } else {
            set_rest(vals_tail, val_cell);
            vals_tail = val_cell;
        }
    }

    // Expand body expressions
    Object* expanded_body = nil;
    Object* expanded_tail = nil;
    for (Object* b = body; !is_nil(b); b = rest(b)) {
        Object* expanded_expr = expand(first(b));
        Object* cell = cons(expanded_expr, nil);
        if (expanded_body == nil) {
            expanded_body = cell;
            expanded_tail = cell;
        } else {
            set_rest(expanded_tail, cell);
            expanded_tail = cell;
        }
    }

    // Build: ((lambda vars body...) vals...)
    // Note: vars_list will be nil for empty bindings, which is correct for (lambda () ...)
    Object* lambda_form = cons(intern_symbol("lambda"),
                              cons(vars_list, expanded_body));

    return cons(lambda_form, vals_list);
}

Object* Expander::expand_let_star(Object* bindings, Object* body) {
    // Transform: (let* ((var val) ...) body...)
    //         → nested let forms

    // Build result from inside out, starting with body
    Object* result = cons(intern_symbol("begin"), body);

    // Work backwards through bindings to build nested structure
    Object* bindings_list = nil;
    for (Object* b = bindings; !is_nil(b); b = rest(b)) {
        bindings_list = cons(first(b), bindings_list);
    }

    // Now bindings_list is reversed, build nested lets
    while (!is_nil(bindings_list)) {
        Object* binding = first(bindings_list);
        // (let ((var val)) result)
        Object* let_form = cons(intern_symbol("let"),
                              cons(cons(binding, nil),
                                   cons(result, nil)));
        result = let_form;
        bindings_list = rest(bindings_list);
    }

    // Recursively expand the nested let structure
    return expand(result);
}

Object* Expander::expand_letrec(Object* bindings, Object* body) {
    // Transform: (letrec ((var val) ...) body...)
    //         → (let ((var nil) ...) (set! var val) ... body...)

    Object* let_bindings = nil;
    Object* let_bindings_tail = nil;
    Object* set_statements = nil;
    Object* set_statements_tail = nil;

    for (Object* b = bindings; !is_nil(b); b = rest(b)) {
        Object* binding = first(b);
        Object* var = first(binding);
        Object* val = first(rest(binding));

        // Add (var #f) to let bindings
        // IMPORTANT: Use false_obj, not nil, because nil represents the empty list
        // and would cause expand_let to generate an empty argument list
        Object* let_binding = cons(var, cons(false_obj, nil));
        Object* let_binding_cell = cons(let_binding, nil);
        if (let_bindings == nil) {
            let_bindings = let_binding_cell;
            let_bindings_tail = let_binding_cell;
        } else {
            set_rest(let_bindings_tail, let_binding_cell);
            let_bindings_tail = let_binding_cell;
        }

        // Add (set! var val) to set statements
        Object* set_form = cons(intern_symbol("set!"),
                              cons(var, cons(val, nil)));
        Object* set_cell = cons(set_form, nil);
        if (set_statements == nil) {
            set_statements = set_cell;
            set_statements_tail = set_cell;
        } else {
            set_rest(set_statements_tail, set_cell);
            set_statements_tail = set_cell;
        }
    }

    // Build the body: set statements + body expressions
    Object* let_body = set_statements;
    if (let_body == nil) {
        let_body = body;
    } else {
        set_rest(set_statements_tail, body);
    }

    // Build: (let ((var nil) ...) (set! ...) ... body...)
    Object* let_form = cons(intern_symbol("let"),
                          cons(let_bindings, let_body));

    // Recursively expand the let form
    return expand(let_form);
}

Object* Expander::expand_case(Object* key_expr, Object* clauses) {
    // Transform: (case key ((datum...) result) ...)
    //         → (cond ((memv key '(datum...)) result) ...)

    // Expand the key expression once
    Object* expanded_key = expand(key_expr);

    // Build cond clauses
    Object* cond_clauses = nil;
    Object* cond_tail = nil;

    for (Object* c = clauses; !is_nil(c); c = rest(c)) {
        Object* clause = first(c);
        Object* datums = first(clause);
        Object* results = rest(clause);

        Object* cond_clause;
        if (is_keyword(datums, "else")) {
            // (else result...)
            cond_clause = cons(intern_symbol("else"), results);
        } else {
            // ((memv key '(datum...)) result...)
            Object* quoted_datums = cons(intern_symbol("quote"),
                                       cons(datums, nil));
            Object* memv_call = cons(intern_symbol("memv"),
                                   cons(expanded_key,
                                       cons(quoted_datums, nil)));
            cond_clause = cons(memv_call, results);
        }

        Object* cond_cell = cons(cond_clause, nil);
        if (cond_clauses == nil) {
            cond_clauses = cond_cell;
            cond_tail = cond_cell;
        } else {
            set_rest(cond_tail, cond_cell);
            cond_tail = cond_cell;
        }
    }

    // Build: (cond ...)
    Object* cond_form = cons(intern_symbol("cond"), cond_clauses);

    // Recursively expand the cond form
    return expand(cond_form);
}

Object* Expander::expand_cond_expand(Object* clauses) {
    // Transform: (cond-expand (feature-req body...) ...)
    //         → body of first matching clause
    // R7RS Section 4.2.1

    // Platform-specific features (must match primitive_system.cpp)
    const char* feature_list[] = {
#ifdef _WIN32
        "windows",
#endif
#ifdef __linux__
        "posix",
        "unix",
#endif
        // Numeric features
        "complex",           // Complex numbers supported
        "ratios",            // Exact rational numbers (a/b)
        "exact-closed",      // Exact arithmetic is closed
        "ieee-float",        // IEEE 754 floating point
        "exact-complex",     // Exact complex numbers
        // Core features
        "r7rs",              // R7RS-small compliant
        "zombie-scheme",     // Implementation name
        nullptr
    };

    // Helper to check if a single feature is available
    auto has_feature = [&](const std::string& feature_name) -> bool {
        for (int i = 0; feature_list[i] != nullptr; i++) {
            if (feature_name == feature_list[i]) {
                return true;
            }
        }
        return false;
    };

    // Forward declaration for recursive checking
    std::function<bool(Object*)> is_feature_satisfied;

    // Check if a feature requirement is satisfied
    is_feature_satisfied = [&](Object* req) -> bool {
        if (is_symbol(req)) {
            // Single feature name
            std::string req_str = ((a_symbol*)req)->value();
            return has_feature(req_str);
        }

        if (!is_nil(req) && !req->is_atom()) {
            // Compound requirement
            Object* op = first(req);
            Object* args = rest(req);

            if (is_keyword(op, "and")) {
                // All requirements must be satisfied
                for (Object* a = args; !is_nil(a); a = rest(a)) {
                    if (!is_feature_satisfied(first(a))) {
                        return false;
                    }
                }
                return true;
            }

            if (is_keyword(op, "or")) {
                // At least one requirement must be satisfied
                for (Object* a = args; !is_nil(a); a = rest(a)) {
                    if (is_feature_satisfied(first(a))) {
                        return true;
                    }
                }
                return false;
            }

            if (is_keyword(op, "not")) {
                // Requirement must not be satisfied
                return !is_feature_satisfied(first(args));
            }
        }

        return false;
    };

    // Iterate through clauses and find first matching one
    for (Object* c = clauses; !is_nil(c); c = rest(c)) {
        Object* clause = first(c);
        Object* feature_req = first(clause);
        Object* clause_body = rest(clause);

        // Check for else clause
        if (is_keyword(feature_req, "else")) {
            // Expand the body and return wrapped in begin
            Object* expanded_body = nil;
            Object* expanded_tail = nil;

            for (Object* b = clause_body; !is_nil(b); b = rest(b)) {
                Object* expanded_expr = expand(first(b));
                Object* cell = cons(expanded_expr, nil);

                if (expanded_body == nil) {
                    expanded_body = cell;
                    expanded_tail = cell;
                } else {
                    set_rest(expanded_tail, cell);
                    expanded_tail = cell;
                }
            }

            return cons(intern_symbol("begin"), expanded_body);
        }

        // Check if feature requirement is satisfied
        if (is_feature_satisfied(feature_req)) {
            // Expand the body and return wrapped in begin
            Object* expanded_body = nil;
            Object* expanded_tail = nil;

            for (Object* b = clause_body; !is_nil(b); b = rest(b)) {
                Object* expanded_expr = expand(first(b));
                Object* cell = cons(expanded_expr, nil);

                if (expanded_body == nil) {
                    expanded_body = cell;
                    expanded_tail = cell;
                } else {
                    set_rest(expanded_tail, cell);
                    expanded_tail = cell;
                }
            }

            return cons(intern_symbol("begin"), expanded_body);
        }
    }

    // No matching clause found - return (begin) which evaluates to unspecified
    // R7RS: "It is an error if none of the feature requirements is satisfied"
    // We'll return empty begin rather than error to be more lenient
    return cons(intern_symbol("begin"), nil);
}

Object* Expander::expand_do(Object* bindings, Object* test_clause, Object* body) {
    // Transform: (do ((var init step) ...) (test result ...) body ...)
    //         → (let loop ((var init) ...)
    //             (if test
    //                 (begin result ...)
    //                 (begin body ... (loop step ...))))

    // Extract variables, inits, and steps from bindings
    Object* vars = nil;
    Object* vars_tail = nil;
    Object* inits = nil;
    Object* inits_tail = nil;
    Object* steps = nil;
    Object* steps_tail = nil;

    for (Object* b = bindings; !is_nil(b); b = rest(b)) {
        Object* binding = first(b);
        Object* var = first(binding);
        Object* init = first(rest(binding));
        Object* step = rest(rest(binding));

        // If no step provided, use var itself
        Object* step_expr = is_nil(step) ? var : first(step);

        // Add var to vars list
        Object* var_cell = cons(var, nil);
        if (vars == nil) {
            vars = var_cell;
            vars_tail = var_cell;
        } else {
            set_rest(vars_tail, var_cell);
            vars_tail = var_cell;
        }

        // Add init to inits list
        Object* init_cell = cons(init, nil);
        if (inits == nil) {
            inits = init_cell;
            inits_tail = init_cell;
        } else {
            set_rest(inits_tail, init_cell);
            inits_tail = init_cell;
        }

        // Add step to steps list
        Object* step_cell = cons(step_expr, nil);
        if (steps == nil) {
            steps = step_cell;
            steps_tail = step_cell;
        } else {
            set_rest(steps_tail, step_cell);
            steps_tail = step_cell;
        }
    }

    // Extract test and results from test_clause
    Object* test = first(test_clause);
    Object* results = rest(test_clause);

    // Build result expression: (begin result ...)
    Object* result_expr;
    if (is_nil(results)) {
        result_expr = nil;
    } else {
        result_expr = cons(intern_symbol("begin"), results);
    }

    // Build loop call: (loop step ...)
    Object* loop_call = cons(intern_symbol("loop"), steps);

    // Build body with loop call: (begin body ... (loop step ...))
    Object* body_with_loop = cons(intern_symbol("begin"), body);
    // Append loop call to body
    Object* body_tail = body_with_loop;
    while (!is_nil(rest(body_tail))) {
        body_tail = rest(body_tail);
    }
    set_rest(body_tail, cons(loop_call, nil));

    // Build if expression: (if test result-expr body-with-loop)
    Object* if_expr = cons(intern_symbol("if"),
                          cons(test,
                              cons(result_expr,
                                  cons(body_with_loop, nil))));

    // Build loop bindings: ((var init) ...)
    Object* loop_bindings = nil;
    Object* loop_bindings_tail = nil;
    Object* v = vars;
    Object* i = inits;
    while (!is_nil(v) && !is_nil(i)) {
        Object* binding = cons(first(v), cons(first(i), nil));
        Object* binding_cell = cons(binding, nil);
        if (loop_bindings == nil) {
            loop_bindings = binding_cell;
            loop_bindings_tail = binding_cell;
        } else {
            set_rest(loop_bindings_tail, binding_cell);
            loop_bindings_tail = binding_cell;
        }
        v = rest(v);
        i = rest(i);
    }

    // Build named let: (let loop ((var init) ...) if-expr)
    Object* named_let = cons(intern_symbol("let"),
                            cons(intern_symbol("loop"),
                                cons(loop_bindings,
                                    cons(if_expr, nil))));

    // Recursively expand the named let
    return expand(named_let);
}

Object* Expander::expand_quasiquote(Object* expr) {
    // Transform: `expr → quasiquote expansion
    // Handles unquote (,) and unquote-splicing (,@)

    // Base case: atoms and nil
    if (is_nil(expr) || expr->is_nil()) {
        return cons(intern_symbol("quote"), cons(expr, nil));
    }

    if (expr->is_atom()) {
        return cons(intern_symbol("quote"), cons(expr, nil));
    }

    // Check for unquote: (unquote expr) or ,expr
    Object* first_elem = first(expr);
    std::string first_name = get_identifier_name(first_elem);

    if (first_name == "unquote") {
        // (unquote expr) → expr (evaluate it)
        Object* unquoted = first(rest(expr));
        return expand(unquoted);
    }

    if (first_name == "unquote-splicing") {
        // Error: unquote-splicing not allowed at top level of quasiquote
        return cons(intern_symbol("quote"), cons(expr, nil));
    }

    // Recursive case: (a . b)
    // Process car and cdr separately

    // Check if car is unquote-splicing
    Object* car_elem = first(expr);
    bool car_is_splicing = false;
    if (!is_nil(car_elem) && !car_elem->is_nil() && car_elem->pair()) {
        Object* car_first = first(car_elem);
        std::string car_first_name = get_identifier_name(car_first);
        if (car_first_name == "unquote-splicing") {
            car_is_splicing = true;
        }
    }

    Object* expanded_car;
    if (car_is_splicing) {
        // ,@expr → (append expr ...)
        Object* spliced = first(rest(car_elem));
        expanded_car = expand(spliced);
    } else {
        // Recursively expand car
        expanded_car = expand_quasiquote(car_elem);
    }

    // Recursively expand cdr
    Object* cdr_elem = rest(expr);
    Object* expanded_cdr;
    if (is_nil(cdr_elem) || cdr_elem->is_nil()) {
        expanded_cdr = cons(intern_symbol("quote"), cons(nil, nil));
    } else {
        expanded_cdr = expand_quasiquote(cdr_elem);
    }

    // Build result using cons or append
    if (car_is_splicing) {
        // (append expanded_car expanded_cdr)
        return cons(intern_symbol("append"),
                   cons(expanded_car,
                       cons(expanded_cdr, nil)));
    } else {
        // (cons expanded_car expanded_cdr)
        return cons(intern_symbol("cons"),
                   cons(expanded_car,
                       cons(expanded_cdr, nil)));
    }
}

// ============================================================================
// Desugar and/or - Short-circuit boolean operators
// ============================================================================

Object* Expander::expand_and(Object* args) {
    // Transform: (and e1 e2 ...) → nested if statements wrapped in let
    // (and) → #t
    // (and e) → e
    // (and e1 e2 e3) → (let ((tmp e1)) (if tmp (and e2 e3) #f))

    if (is_nil(args)) {
        // (and) → #t
        return t;
    }

    Object* first_arg = first(args);
    Object* rest_args = rest(args);

    if (is_nil(rest_args)) {
        // (and e) → e (expand it)
        return expand(first_arg);
    }

    // (and e1 e2 e3) → (let ((tmp e1)) (if tmp (and e2 e3) #f))
    // Generate unique tmp variable
    static int tmp_counter = 0;
    std::string tmp_name = "__and_tmp_" + std::to_string(tmp_counter++);
    Object* tmp_sym = intern_symbol(tmp_name);

    Object* expanded_first = expand(first_arg);
    Object* expanded_rest = expand_and(rest_args);  // Recursive call

    // Build: (let ((tmp expanded_first)) (if tmp expanded_rest #f))
    Object* binding = cons(tmp_sym, cons(expanded_first, nil));
    Object* bindings = cons(binding, nil);
    Object* if_expr = cons(intern_symbol("if"),
                          cons(tmp_sym,
                              cons(expanded_rest,
                                  cons(false_obj, nil))));
    Object* body = cons(if_expr, nil);

    // Use expand_let to desugar the let
    return expand_let(bindings, body);
}

Object* Expander::expand_or(Object* args) {
    // Transform: (or e1 e2 ...) → nested if statements with tmp binding
    // (or) → #f
    // (or e) → e
    // (or e1 e2 e3) → (let ((tmp e1)) (if tmp tmp (or e2 e3)))

    if (is_nil(args)) {
        // (or) → #f
        return false_obj;
    }

    Object* first_arg = first(args);
    Object* rest_args = rest(args);

    if (is_nil(rest_args)) {
        // (or e) → e (expand it)
        return expand(first_arg);
    }

    // (or e1 e2 ...) → (let ((tmp e1)) (if tmp tmp (or e2 e3)))
    // Generate unique tmp variable
    static int tmp_counter = 0;
    std::string tmp_name = "__or_tmp_" + std::to_string(tmp_counter++);
    Object* tmp_sym = intern_symbol(tmp_name);

    Object* expanded_first = expand(first_arg);
    Object* expanded_rest = expand_or(rest_args);  // Recursive call

    // Build: (let ((tmp expanded_first)) (if tmp tmp expanded_rest))
    Object* binding = cons(tmp_sym, cons(expanded_first, nil));
    Object* bindings = cons(binding, nil);
    Object* if_expr = cons(intern_symbol("if"),
                          cons(tmp_sym,
                              cons(tmp_sym,
                                  cons(expanded_rest, nil))));
    Object* body = cons(if_expr, nil);

    // Use expand_let to desugar the let
    return expand_let(bindings, body);
}

// ============================================================================
// Conditional execution - when/unless
// ============================================================================

Object* Expander::expand_when(Object* test, Object* body) {
    // Transform: (when test expr1 expr2 ...) → (if test (begin expr1 expr2 ...))
    // Special cases:
    //   (when test) → (if test (begin))
    //   (when test expr) → (if test expr)
    //   (when test expr1 expr2 ...) → (if test (begin expr1 expr2 ...))

    // Expand the test expression
    Object* expanded_test = expand(test);

    // Expand the body expressions
    Object* expanded_body = nil;
    Object* body_tail = nil;
    for (Object* e = body; !is_nil(e); e = rest(e)) {
        Object* expanded_expr = expand(first(e));
        Object* cell = cons(expanded_expr, nil);
        if (expanded_body == nil) {
            expanded_body = cell;
            body_tail = cell;
        } else {
            set_rest(body_tail, cell);
            body_tail = cell;
        }
    }

    // Build the if expression
    Object* consequent;
    if (is_nil(expanded_body)) {
        // (when test) → (if test (begin))
        consequent = cons(intern_symbol("begin"), nil);
    } else if (is_nil(rest(expanded_body))) {
        // Single body expression: (when test expr) → (if test expr)
        consequent = first(expanded_body);
    } else {
        // Multiple body expressions: (when test expr1 expr2 ...) → (if test (begin expr1 expr2 ...))
        consequent = cons(intern_symbol("begin"), expanded_body);
    }

    // Build: (if expanded_test consequent)
    return cons(intern_symbol("if"),
               cons(expanded_test,
                   cons(consequent, nil)));
}

Object* Expander::expand_unless(Object* test, Object* body) {
    // Transform: (unless test expr1 expr2 ...) → (if (not test) (begin expr1 expr2 ...))
    // Special cases:
    //   (unless test) → (if (not test) (begin))
    //   (unless test expr) → (if (not test) expr)
    //   (unless test expr1 expr2 ...) → (if (not test) (begin expr1 expr2 ...))

    // Expand the test expression
    Object* expanded_test = expand(test);

    // Build (not test)
    Object* negated_test = cons(intern_symbol("not"),
                               cons(expanded_test, nil));

    // Expand the body expressions
    Object* expanded_body = nil;
    Object* body_tail = nil;
    for (Object* e = body; !is_nil(e); e = rest(e)) {
        Object* expanded_expr = expand(first(e));
        Object* cell = cons(expanded_expr, nil);
        if (expanded_body == nil) {
            expanded_body = cell;
            body_tail = cell;
        } else {
            set_rest(body_tail, cell);
            body_tail = cell;
        }
    }

    // Build the if expression
    Object* consequent;
    if (is_nil(expanded_body)) {
        // (unless test) → (if (not test) (begin))
        consequent = cons(intern_symbol("begin"), nil);
    } else if (is_nil(rest(expanded_body))) {
        // Single body expression: (unless test expr) → (if (not test) expr)
        consequent = first(expanded_body);
    } else {
        // Multiple body expressions: (unless test expr1 expr2 ...) → (if (not test) (begin expr1 expr2 ...))
        consequent = cons(intern_symbol("begin"), expanded_body);
    }

    // Build: (if negated_test consequent)
    return cons(intern_symbol("if"),
               cons(negated_test,
                   cons(consequent, nil)));
}

// ============================================================================
// Exception handling - guard
// ============================================================================

Object* Expander::expand_guard(Object* var, Object* clauses, Object* body) {
    /*
     * Transform: (guard (var (test handler) ... (else handler)) body...)
     *
     * R7RS guard syntax for exception handling.
     *
     * NOTE: This is a simplified implementation. Full R7RS guard requires:
     * - Continuation capture (call/cc) to properly implement re-raising
     * - Dynamic exception handlers using with-exception-handler
     * - Proper escape continuations for non-matching handlers
     *
     * For now, we mark this as a special form that the evaluator must handle
     * directly, since we don't have full Continuation support yet.
     *
     * The evaluator will need to:
     * 1. Try to evaluate body
     * 2. On exception, bind exception to var
     * 3. Test each clause in order
     * 4. Execute handler for first matching clause
     * 5. Re-raise if no clause matches
     */

    // For now, return the guard form unchanged - the evaluator will handle it
    // This marks it as a special form that can't be desugared further
    // without Continuation support

    // Build: (guard (var clause...) body...)
    Object* guard_spec = cons(var, clauses);
    return cons(intern_symbol("guard"),
               cons(guard_spec, body));
}

// ============================================================================
// Case-lambda - multi-arity procedures
// ============================================================================

Object* Expander::expand_case_lambda(Object* clauses) {
    /*
     * Transform: (case-lambda ((formals body ...) ...))
     *
     * R7RS case-lambda allows procedures to have different behavior based on
     * argument count:
     *
     * (define plus
     *   (case-lambda
     *     (() 0)
     *     ((x) x)
     *     ((x y) (+ x y))))
     *
     * This cannot be desugared to simpler forms - it needs runtime dispatch
     * based on argument count. We return a special case-lambda expression that
     * the evaluator will recognize and handle directly.
     */

    // Validate clauses
    if (!clauses || is_nil(clauses)) {
        throw std::runtime_error("case-lambda: requires at least one clause");
    }

    // Expand each clause's body expressions
    Object* expanded_clauses = nil;
    Object* tail = nil;

    for (Object* clause_list = clauses; !is_nil(clause_list); clause_list = rest(clause_list)) {
        if (!clause_list->pair()) {
            throw std::runtime_error("case-lambda: clauses must be a proper list");
        }

        Object* clause = first(clause_list);
        if (!clause->pair()) {
            throw std::runtime_error("case-lambda: each clause must be (formals body ...)");
        }

        Object* formals = first(clause);
        Object* body = rest(clause);

        if (is_nil(body)) {
            throw std::runtime_error("case-lambda: clause body cannot be empty");
        }

        // Expand body expressions
        Object* expanded_body = nil;
        Object* body_tail = nil;

        for (Object* expr_list = body; !is_nil(expr_list); expr_list = rest(expr_list)) {
            if (!expr_list->pair()) {
                throw std::runtime_error("case-lambda: clause body must be a proper list");
            }
            Object* expanded_expr = expand(first(expr_list));
            Object* cell = cons(expanded_expr, nil);

            if (is_nil(expanded_body)) {
                expanded_body = cell;
                body_tail = cell;
            } else {
                set_rest(body_tail, cell);
                body_tail = cell;
            }
        }

        // Build expanded clause: (formals expanded-body ...)
        Object* expanded_clause = cons(formals, expanded_body);
        Object* cell = cons(expanded_clause, nil);

        if (is_nil(expanded_clauses)) {
            expanded_clauses = cell;
            tail = cell;
        } else {
            set_rest(tail, cell);
            tail = cell;
        }
    }

    // Return: (case-lambda expanded-clauses...)
    // The evaluator will recognize this and create a case_lambda Procedure
    return cons(intern_symbol("case-lambda"), expanded_clauses);
}

// ============================================================================
// Local Macro handling
// ============================================================================

Object* Expander::expand_let_syntax(Object* bindings, Object* body) {
    // Create a new environment with local Macro definitions
    Environment* local_env = gc_new<Environment>("let-syntax", nil, nil, env_);

    // Define each Macro in the local environment
    for (Object* b = bindings; !is_nil(b); b = rest(b)) {
        Object* binding = first(b);
        Object* name = first(binding);
        Object* transformer_spec = second(binding);

        // Parse (syntax-rules ...)
        if (transformer_spec->is_atom()) {
            throw std::runtime_error("let-syntax: requires syntax-rules form");
        }

        Object* sr_keyword = first(transformer_spec);
        std::string sr_name = get_identifier_name(sr_keyword);

        if (sr_name != "syntax-rules") {
            throw std::runtime_error("let-syntax: requires syntax-rules");
        }

        Object* sr_args = rest(transformer_spec);
        Object* literals = first(sr_args);
        Object* rules = rest(sr_args);

        // Create transformer and define in local environment
        SyntaxRulesTransformer* transformer = gc_new<SyntaxRulesTransformer>(literals, rules);
        local_env->define(name, transformer);
    }

    // Expand body in the local environment
    Expander local_expander(local_env);

    // Expand body expressions
    Object* expanded_body = nil;
    Object* expanded_tail = nil;
    for (Object* b = body; !is_nil(b); b = rest(b)) {
        Object* expanded_expr = local_expander.expand(first(b));
        Object* cell = cons(expanded_expr, nil);
        if (expanded_body == nil) {
            expanded_body = cell;
            expanded_tail = cell;
        } else {
            set_rest(expanded_tail, cell);
            expanded_tail = cell;
        }
    }

    // Return (begin expanded-body...)
    return cons(intern_symbol("begin"), expanded_body);
}

Object* Expander::expand_letrec_syntax(Object* bindings, Object* body) {
    // Same as let-syntax but macros can reference each other
    // In practice, this is the same implementation
    return expand_let_syntax(bindings, body);
}

// ============================================================================
// Macro expansion
// ============================================================================

Object* Expander::try_expand_macro(Object* expr) {
    if (is_nil(expr) || expr->is_nil() || expr->is_atom()) {
        return nullptr;
    }

    Object* first_elem = first(expr);

    // Check if the first element is a symbol bound to a Macro
    if (!is_macro(first_elem)) {
        return nullptr;
    }

    // Get the Macro transformer
    Object* transformer = get_macro_transformer(first_elem);
    if (!transformer) {
        return nullptr;
    }

    // Apply the transformer
    SyntaxRulesTransformer* Macro = dynamic_cast<SyntaxRulesTransformer*>(transformer);
    if (!Macro) {
        return nullptr;
    }

    // Expand the Macro expression
    return Macro->expand(expr, env_);
}

bool Expander::is_macro(Object* symbol) {
    if (!symbol || symbol->is_nil() || !is_symbol(symbol)) {
        return false;
    }

    try {
        Object* binding = env_->lookup(symbol);
        return dynamic_cast<SyntaxRulesTransformer*>(binding) != nullptr;
    } catch (const std::exception&) {
        // lookup throws when the symbol is unbound
        return false;
    }
}

Object* Expander::get_macro_transformer(Object* symbol) {
    if (!is_macro(symbol)) {
        return nullptr;
    }

    try {
        return env_->lookup(symbol);
    } catch (const std::exception&) {
        // lookup throws when the symbol is unbound
        return nullptr;
    }
}

} } // namespace zombie::runtime
