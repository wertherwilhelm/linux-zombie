#include "pch.h"
// Lisp.cpp: implementation of the Lisp class.
//
//////////////////////////////////////////////////////////////////////

#include "Lisp.h"

#include "closure.h"
#include "a_case_lambda.h"
#include "Macro.h"
#include "syntax_rules_transformer.h"
#include "constants.h"  // For intern_symbol()
#include "gc_manager.h"
#include "gc_ptr.h"
#include "exception_handlers.h"
#include "a_parameter.h"
#include "a_exception.h"
#include "library.h"
#include "library_registry.h"
#include "library_parser.h"

namespace zombie { namespace runtime {

// Global Lisp evaluator pointer (for Value Ledger system)
Lisp* g_lisp = nullptr;

// Helper function to extract name from symbol or string
static std::string get_identifier_name(Object* obj) {
    if (is_symbol(obj)) {
        return ((a_symbol*)obj)->value();
    } else if (is_string(obj)) {
        return ((a_string*)obj)->value();
    }
    return "";
}

// Helper function for appending two lists (used in letrec implementation)
static Object* append_two_lists(Object* list1, Object* list2) {
    if (list1 == nil || list1->is_nil()) {
        return list2;
    }

    // Protect parameters during loop allocations
    gc_root<Object> guard1(list1);
    gc_root<Object> guard2(list2);

    Object* result = nil;
    Object* tail = nil;

    // Protect result and tail during loop allocations
    gc_root<Object> guard_result(result);
    gc_root<Object> guard_tail(tail);

    // Copy list1
    for (Object* p = list1; !p->is_nil(); p = rest(p)) {
        Object* new_cell = cons(first(p), nil);
        if (result == nil) {
            result = new_cell;
            guard_result = gc_root<Object>(result);  // Update guard
            tail = new_cell;
            guard_tail = gc_root<Object>(tail);  // Update guard
        } else {
            ((ConsCell*)tail)->set_cdr_noclone(new_cell);
            tail = new_cell;
            guard_tail = gc_root<Object>(tail);  // Update guard
        }
    }

    // Append list2
    if (tail != nil) {
        ((ConsCell*)tail)->set_cdr_noclone(list2);
    } else {
        result = list2;
    }

    return result;
}

// ============================================================================
// cond-expand feature checking helpers (static - not part of Lisp class)
// ============================================================================

// Get compile-time feature list
static const char** get_feature_list() {
    static const char* features[] = {
#ifdef _WIN32
        "windows",
#endif
#ifdef __linux__
        "posix",
        "unix",
#endif
        // Numeric features
        "complex", "ratios", "exact-closed", "ieee-float", "exact-complex",
        // Core features
        "r7rs", "zombie-scheme",
        nullptr
    };
    return features;
}

// Check if a single feature is available
static bool has_feature(const std::string& feat) {
    const char** features = get_feature_list();
    for (int i = 0; features[i] != nullptr; i++) {
        if (feat == features[i]) return true;
    }
    return false;
}

// Check if a feature requirement is satisfied (recursive)
static bool is_feature_satisfied(Object* req) {
    if (is_symbol(req)) {
        return has_feature(((a_symbol*)req)->value());
    }
    if (!req->is_nil() && !req->is_atom()) {
        Object* op = first(req);
        Object* op_args = rest(req);
        if (is_symbol(op)) {
            std::string op_name = ((a_symbol*)op)->value();
            if (op_name == "and") {
                for (Object* a = op_args; !a->is_nil(); a = rest(a)) {
                    if (!is_feature_satisfied(first(a))) return false;
                }
                return true;
            } else if (op_name == "or") {
                for (Object* a = op_args; !a->is_nil(); a = rest(a)) {
                    if (is_feature_satisfied(first(a))) return true;
                }
                return false;
            } else if (op_name == "not") {
                return !is_feature_satisfied(first(op_args));
            }
        }
    }
    return false;
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Lisp::Lisp()
: m_global_env(NULL)
{
    Primitive temp;
}

Lisp::~Lisp()
{
}




// Evaluate a list of arguments and return a list of evaluated results
// ITERATIVE VERSION - critical for TCO to work with deep recursion
ConsCell* Lisp::eval_args(Object *List, Environment*env) {
    if (List == NULL || List->is_nil()) {
        // CRITICAL FIX: Return nil, not NULL!
        // NULL (nullptr) and nil are different objects.
        // Returning NULL causes cons cells to have nullptr as cdr instead of nil.
        return (ConsCell*)nil;
    } else if (!List->pair()) {
        // If it's an atom, just return it as a single-element list
        return (ConsCell*)List;
    } else {
        // ITERATIVELY evaluate each element (avoids C++ stack overflow)
        // Build list in reverse, then reverse it
        Object* result = nil;
        Object* current = List;

        // Protect result during loop allocations
        gc_root<Object> guard_result(result);

        while (current != nil && !current->is_nil() && current->pair()) {
            Object* elem_evaluated = new_eval(first(current), env);

            // Protect elem_evaluated during cons allocation
            gc_root<Object> guard_elem(elem_evaluated);

            result = cons(elem_evaluated, result);
            guard_result = gc_root<Object>(result);  // Update guard

            current = rest(current);
        }

        // Reverse to restore original order
        return (ConsCell*)reverse(result);
    }
}

Object * Lisp::reduce_cond(Object *clauses, Environment*env) {
    Object *result = NULL;
    for (;;) {
        if (clauses == NULL) {
            return gc_new<a_bool>(false);
        }
        Object *clause = first(clauses); clauses = rest(clauses);
        if (get_identifier_name(first(clause)) == "else" || truth(result = new_eval(first(clause), env))) {
            if (rest(clause) == NULL) {
                return list(gc_new<a_string>("quote"), result);
            }
            else if (get_identifier_name(second(clause)) == "=>") {
                return list(third(clause), list(gc_new<a_string>("quote"), result));
            } else {
                //char[] tag = "begin";
                return cons(gc_new<a_string>("begin"), rest(clause));
            }
        }
    }
}

Object* Lisp::new_eval(Object* x, bool treat_as_single_arg) {
    return new_eval(x, this->m_global_env, treat_as_single_arg);
}

Object* Lisp::new_eval(Object* x, Environment*env, bool treat_as_single_arg) {
    // Trampoline loop for Tail Call Optimization (TCO)
    // Instead of recursing, we loop and evaluate tail calls iteratively
    eval_result result = eval_result::make_tailcall(x, env);

    while (result.tag == eval_result::TAILCALL) {
        result = eval_step(result.expr, result.env, treat_as_single_arg);
    }

    return result.value;
}

// TCO: Local trampolining - evaluates expression to a value without creating new stack frame
// This is critical for unlimited TCO: by trampolining locally instead of calling new_eval(),
// we avoid nesting trampoline loops on the C++ stack
Object* Lisp::eval_to_value(Object* x, Environment* env) {
    eval_result result = eval_step(x, env, false);

    while (result.tag == eval_result::TAILCALL) {
        result = eval_step(result.expr, result.env, false);
    }

    return result.value;
}

Object* Lisp::eval_atom(Object* x, Environment*env) {
    // R5RS: empty_list, #t, and #f are self-evaluating
    if (x->is_nil() || x == t || x == false_obj) {
        return x;
    }
    // Numbers are self-evaluating (includes rationals and complex)
    if (is_double(x) || x->get_type() == static_cast<int>(Type::INT) || x->get_type() == static_cast<int>(Type::LONG) ||
        x->get_type() == static_cast<int>(Type::RATIONAL) || x->get_type() == static_cast<int>(Type::COMPLEX)) {
        return x;
    } else if (is_char(x)) {
        // Characters are self-evaluating like numbers
        return x;
    } else if (is_symbol(x)) {
        // Symbols are looked up in the environment
        try {
            Object* prim_fn = env->lookup(x);
            return prim_fn;
        } catch (const std::runtime_error&) {
            // Unbound variable error
            throw std::runtime_error("Unbound variable: " + ((a_symbol*)x)->getName());
        }
    } else if (is_string(x)) {
        // Strings are self-evaluating literals
        return x;
    } else {
        throw std::runtime_error("Unexpected atomic type");
    }
}

// we need to pass the current env in.
Object* Lisp::eval_list(Object* x, Environment*env)
{
    ConsCell* expr_as_pair = dynamic_cast<ConsCell*>(x);
    if (expr_as_pair == nullptr) {
        throw std::runtime_error("Invalid expression type");
    }

    Object* fn = first(x);
    Object* args = rest(x);

    std::string fn_val = get_identifier_name(fn);
    if (fn_val == "quote") {
        return first(args);
    } else if (fn_val == "define") {
        if (!first(args)->is_atom()) {
            return env->define(first(first(args)), new_eval(cons(gc_new<a_string>("lambda"), cons(rest(first(args)), rest(args))), env));
        } else {
            return env->define(first(args), new_eval(second(args), env));
        }
    }
    else if (fn_val == "define-syntax") {
        // (define-syntax name (syntax-rules (literals) (pattern template) ...))
        Object* name = first(args);
        Object* transformer_spec = second(args);

        // Parse (syntax-rules ...)
        if (transformer_spec->is_atom()) {
            return gc_new<a_string>("ERROR: define-syntax requires syntax-rules form");
        }

        Object* sr_keyword = first(transformer_spec);
        std::string sr_name = get_identifier_name(sr_keyword);

        if (sr_name != "syntax-rules") {
            return gc_new<a_string>("ERROR: define-syntax requires syntax-rules");
        }

        Object* sr_args = rest(transformer_spec);
        Object* literals = first(sr_args);
        Object* rules = rest(sr_args);

        // Create transformer
        SyntaxRulesTransformer* transformer = gc_new<SyntaxRulesTransformer>(literals, rules);

        // Define in environment
        return env->define(name, transformer);
    }
    else if (fn_val == "import") {
        // (import <import-set> ...)
        // Import symbols from libraries into current environment
        // Supports: simple, only, except, prefix, rename

        r7rs::LibraryParser parser;

        for (Object* import_set_list = args; !import_set_list->is_nil(); import_set_list = rest(import_set_list)) {
            Object* import_set_expr = first(import_set_list);

            // Parse the import set (handles all import types)
            r7rs::ImportSpec import_spec = parser.parse_import_set(import_set_expr);

            // Get the library from registry
            r7rs::Library* source_lib = r7rs::LibraryRegistry::instance().get_library(import_spec.library);
            if (!source_lib) {
                throw std::runtime_error("import: library not found: " + r7rs::library_name_to_string(import_spec.library));
            }

            // Get all exported symbols from the source library
            const std::vector<r7rs::ExportSpec>& exports = source_lib->exports();

            // Process based on import type
            switch (import_spec.type) {
            case r7rs::ImportSpec::SIMPLE: {
                // Import all exported symbols
                for (const auto& exp : exports) {
                    Object* value = source_lib->get_symbol(exp.internal_name);
                    if (value) {
                        env->define(gc_new<a_string>(exp.external_name), value);
                    }
                }
                break;
            }

            case r7rs::ImportSpec::ONLY: {
                // Import only specified symbols
                for (const auto& exp : exports) {
                    if (import_spec.symbols.find(exp.external_name) != import_spec.symbols.end()) {
                        Object* value = source_lib->get_symbol(exp.internal_name);
                        if (value) {
                            env->define(gc_new<a_string>(exp.external_name), value);
                        }
                    }
                }
                break;
            }

            case r7rs::ImportSpec::EXCEPT: {
                // Import all except specified symbols
                for (const auto& exp : exports) {
                    if (import_spec.symbols.find(exp.external_name) == import_spec.symbols.end()) {
                        Object* value = source_lib->get_symbol(exp.internal_name);
                        if (value) {
                            env->define(gc_new<a_string>(exp.external_name), value);
                        }
                    }
                }
                break;
            }

            case r7rs::ImportSpec::PREFIX: {
                // Import all with prefix
                for (const auto& exp : exports) {
                    Object* value = source_lib->get_symbol(exp.internal_name);
                    if (value) {
                        std::string prefixed_name = import_spec.prefix + exp.external_name;
                        env->define(gc_new<a_string>(prefixed_name), value);
                    }
                }
                break;
            }

            case r7rs::ImportSpec::RENAME: {
                // Import with renames
                for (const auto& exp : exports) {
                    Object* value = source_lib->get_symbol(exp.internal_name);
                    if (value) {
                        // Check if this symbol should be renamed
                        auto rename_it = import_spec.renames.find(exp.external_name);
                        if (rename_it != import_spec.renames.end()) {
                            // Use renamed name
                            env->define(gc_new<a_string>(rename_it->second), value);
                        } else {
                            // Use original name
                            env->define(gc_new<a_string>(exp.external_name), value);
                        }
                    }
                }
                break;
            }
            }
        }
        return nil;  // import returns unspecified value
    }
    else if (fn_val == "define-library") {
        // (define-library <library name> <library declaration> ...)

        // Parse the library definition
        r7rs::LibraryParser parser;
        auto lib_ptr = parser.parse_library_definition(x);
        r7rs::Library* lib = lib_ptr.get();

        // Initialize the library (process imports)
        std::string error;
        if (!lib->initialize(env, error)) {
            throw std::runtime_error("define-library: initialization failed: " + error);
        }

        // Evaluate begin blocks
        auto eval_func = [this](Object* form, Environment* env_arg) -> Object* {
            return this->new_eval(form, env_arg);
        };

        if (!lib->evaluate_begin_blocks(eval_func, error)) {
            throw std::runtime_error("define-library: evaluation failed: " + error);
        }

        // Library is automatically registered by LibraryParser, so we don't need to keep lib_ptr
        return nil;  // define-library returns unspecified value
    }
    else if (fn_val == "begin") {
        Object* last_eval = nullptr;
        for (; args != nullptr; args = rest(args)) {
            ConsCell* cons_args = dynamic_cast<ConsCell*>(args);
            if (cons_args != nullptr) {
                last_eval = new_eval(first(args), env);
            } else {
                // Here you could either throw an exception or handle this case in a different way
                //throw std::runtime_error("Expected a cons cell in the argument list");
                return last_eval;
            }
        }
        return last_eval;
    } else if (fn_val == "set!") {
        return env->set(first(args), new_eval(second(args), env));
    } else if (fn_val == "if") {
        x = (truth(new_eval(first(args), env))) ? second(args) : third(args);
        return new_eval(x, env);  // Evaluate and return the chosen branch
    } else if (fn_val == "when") {
        // (when test expr1 expr2 ...) → (if test (begin expr1 expr2 ...))
        Object* test = first(args);
        Object* body = rest(args);
        if (truth(new_eval(test, env))) {
            // Execute body expressions in sequence
            Object* result = nil;
            for (Object* expr_list = body; !expr_list->is_nil(); expr_list = rest(expr_list)) {
                result = new_eval(first(expr_list), env);
            }
            return result;
        } else {
            return nil;  // when returns unspecified (nil) when test is false
        }
    } else if (fn_val == "unless") {
        // (unless test expr1 expr2 ...) → (if (not test) (begin expr1 expr2 ...))
        Object* test = first(args);
        Object* body = rest(args);
        if (!truth(new_eval(test, env))) {
            // Execute body expressions in sequence
            Object* result = nil;
            for (Object* expr_list = body; !expr_list->is_nil(); expr_list = rest(expr_list)) {
                result = new_eval(first(expr_list), env);
            }
            return result;
        } else {
            return nil;  // unless returns unspecified (nil) when test is true
        }
    } else if (fn_val == "and") {
        // R5RS 4.2.1: (and) => #t. Evaluate left to right; on the first false
        // value, short-circuit and return it; otherwise return the last value.
        Object* result = (Object*)t;
        for (Object* a = args; !a->is_nil(); a = rest(a)) {
            result = new_eval(first(a), env);
            if (!truth(result)) return result;
        }
        return result;
    } else if (fn_val == "or") {
        // R5RS 4.2.1: (or) => #f. Return the first true value; else #f.
        for (Object* a = args; !a->is_nil(); a = rest(a)) {
            Object* result = new_eval(first(a), env);
            if (truth(result)) return result;
        }
        return (Object*)false_obj;
    } else if (fn_val == "cond") {
        x = reduce_cond(args, env);
        return new_eval(x, env);  // Evaluate and return the chosen branch
    } else if (fn_val == "lambda") {
        return gc_new<Closure>(first(args), rest(args), env);
    } else if (fn_val == "case-lambda") {
        // (case-lambda ((formals body ...) ...))
        // Parse clauses and create case_lambda Procedure
        std::vector<case_lambda_clause> clauses;

        for (Object* clause_list = args; !clause_list->is_nil(); clause_list = rest(clause_list)) {
            if (!clause_list->pair()) {
                throw std::runtime_error("case-lambda: clauses must be a proper list");
            }

            Object* clause = first(clause_list);
            if (!clause->pair()) {
                throw std::runtime_error("case-lambda: each clause must be (formals body ...)");
            }

            Object* formals = first(clause);
            Object* body = rest(clause);

            if (body->is_nil()) {
                throw std::runtime_error("case-lambda: clause body cannot be empty");
            }

            clauses.push_back(case_lambda_clause(formals, body));
        }

        if (clauses.empty()) {
            throw std::runtime_error("case-lambda: requires at least one clause");
        }

        return gc_new<a_case_lambda>(clauses, env);
    } else if (fn_val == "delay") {
        // Transform (delay expr) to (lambda () expr) - create a promise/thunk
        Object* expr = first(args);
        Object* lambda_body = cons(expr, nil);
        return gc_new<Closure>(nil, lambda_body, env);  // nil = no parameters
    } else if (fn_val == "Macro") {
        Macro* m = gc_new<Macro>(first(args), rest(args), env);
        return m;
    } else if (fn_val == "let") {
        // Validate args before processing
        if (args->is_nil() || !args->pair()) {
            throw std::runtime_error("let: malformed syntax - expected bindings and body");
        }

        // Check if this is a named let: (let name ((var val) ...) body...)
        // Note: nil is an atom, but (let () body) is regular let, not named let
        if (first(args)->is_atom() && !first(args)->is_nil()) {
            // Named let: (let name ((x 1) (y 2)) body...)
            // => (letrec ((name (lambda (x y) body...))) (name 1 2))
            Object* name = first(args);
            Object* bindings = second(args);
            Object* body = rest(rest(args));

            // Validate bindings (must be nil or a list)
            if (!bindings->is_nil() && !bindings->pair()) {
                throw std::runtime_error("named let: bindings must be a list");
            }

            // Extract variables and values from bindings
            Object* vars = nil;
            Object* vals = nil;
            for (Object* b = bindings; !b->is_nil(); b = rest(b)) {
                // Validate each element is a pair
                if (!b->pair()) {
                    throw std::runtime_error("named let: malformed bindings list");
                }
                Object* binding = first(b);
                // Validate binding is a pair (var val)
                if (!binding->pair()) {
                    throw std::runtime_error("named let: each binding must be (var val)");
                }
                vars = cons(first(binding), vars);
                vals = cons(second(binding), vals);
            }
            // Reverse to maintain order
            vars = reverse(vars);
            vals = reverse(vals);

            // Build: (letrec ((name (lambda (vars...) body...))) (name vals...))
            Object* lambda_expr = cons(gc_new<a_string>("lambda"), cons(vars, body));
            Object* letrec_binding = cons(cons(name, cons(lambda_expr, nil)), nil);
            Object* call_expr = cons(name, vals);
            Object* letrec_expr = cons(gc_new<a_string>("letrec"), cons(letrec_binding, cons(call_expr, nil)));

            return new_eval(letrec_expr, env);
        } else {
            // Regular let: (let ((x 1) (y 2)) body...)
            // => ((lambda (x y) body...) 1 2)
            Object* bindings = first(args);
            Object* body = rest(args);

            // Validate bindings (must be nil or a list)
            if (!bindings->is_nil() && !bindings->pair()) {
                throw std::runtime_error("let: bindings must be a list");
            }

            // Extract variables and values from bindings
            Object* vars = nil;
            Object* vals = nil;
            for (Object* b = bindings; !b->is_nil(); b = rest(b)) {
                // Validate each binding is a pair
                if (!b->pair()) {
                    throw std::runtime_error("let: malformed bindings list");
                }
                Object* binding = first(b);
                // Validate binding is a pair (var val)
                if (!binding->pair()) {
                    throw std::runtime_error("let: each binding must be (var val)");
                }
                vars = cons(first(binding), vars);
                vals = cons(second(binding), vals);
            }
            // Reverse to maintain order
            vars = reverse(vars);
            vals = reverse(vals);

            // Build and evaluate: ((lambda (vars...) body...) vals...)
            Object* lambda_expr = cons(gc_new<a_string>("lambda"), cons(vars, body));
            Object* call_expr = cons(lambda_expr, vals);

            return new_eval(call_expr, env);
        }
    } else if (fn_val == "let*") {
        // let*: (let* ((x 1) (y (+ x 1))) body...)
        // => (let ((x 1)) (let* ((y (+ x 1))) body...))
        Object* bindings = first(args);
        Object* body = rest(args);

        if (bindings->is_nil()) {
            // No bindings, just evaluate body in sequence
            return new_eval(cons(gc_new<a_string>("begin"), body), env);
        } else {
            // Transform into nested let
            Object* first_binding = first(bindings);
            Object* rest_bindings = rest(bindings);

            if (rest_bindings->is_nil()) {
                // Last binding: (let ((var val)) body...)
                Object* let_expr = cons(gc_new<a_string>("let"), cons(cons(first_binding, nil), body));
                return new_eval(let_expr, env);
            } else {
                // Multiple bindings: (let ((var val)) (let* (rest...) body...))
                Object* inner_let_star = cons(gc_new<a_string>("let*"), cons(rest_bindings, body));
                Object* let_expr = cons(gc_new<a_string>("let"), cons(cons(first_binding, nil), cons(inner_let_star, nil)));
                return new_eval(let_expr, env);
            }
        }
    } else if (fn_val == "letrec") {
        // letrec: (letrec ((x init-x) (y init-y)) body...)
        // => (let ((x nil) (y nil)) (set! x init-x) (set! y init-y) body...)
        Object* bindings = first(args);
        Object* body = rest(args);

        // Build initial bindings with nil values
        Object* init_bindings = nil;
        Object* set_exprs = nil;

        for (Object* b = bindings; !b->is_nil(); b = rest(b)) {
            Object* binding = first(b);
            Object* var = first(binding);
            Object* init = second(binding);

            // Add (var nil) to initial bindings
            init_bindings = cons(cons(var, cons(nil, nil)), init_bindings);
            // Add (set! var init) to assignments
            set_exprs = cons(cons(gc_new<a_string>("set!"), cons(var, cons(init, nil))), set_exprs);
        }

        // Reverse to maintain order
        init_bindings = reverse(init_bindings);
        set_exprs = reverse(set_exprs);

        // Build: (let ((vars nil)...) (set! vars inits)... body...)
        Object* let_body = append_two_lists(set_exprs, body);
        Object* let_expr = cons(gc_new<a_string>("let"), cons(init_bindings, let_body));

        return new_eval(let_expr, env);
    } else if (fn_val == "let-syntax") {
        // let-syntax: (let-syntax ((name1 (syntax-rules ...)) ...) body...)
        // Create a new environment with local Macro definitions
        Object* bindings = first(args);
        Object* body = rest(args);

        // Create new environment extending current one
        Environment* local_env = gc_new<Environment>("let-syntax", nil, nil, env);

        // Define each Macro in the local environment
        for (Object* b = bindings; !b->is_nil(); b = rest(b)) {
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

        // Evaluate body in local environment with macros defined
        Object* result = nil;
        for (Object* b = body; !b->is_nil(); b = rest(b)) {
            result = new_eval(first(b), local_env);
        }
        return result;

    } else if (fn_val == "letrec-syntax") {
        // letrec-syntax: (letrec-syntax ((name1 (syntax-rules ...)) ...) body...)
        // Like let-syntax but macros can reference each other
        // Implementation is the same as let-syntax since syntax-rules transformers
        // are defined before expansion anyway
        Object* bindings = first(args);
        Object* body = rest(args);

        // Create new environment extending current one
        Environment* local_env = gc_new<Environment>("letrec-syntax", nil, nil, env);

        // Define each Macro in the local environment
        // All macros are defined before any are used, allowing mutual recursion
        for (Object* b = bindings; !b->is_nil(); b = rest(b)) {
            Object* binding = first(b);
            Object* name = first(binding);
            Object* transformer_spec = second(binding);

            // Parse (syntax-rules ...)
            if (transformer_spec->is_atom()) {
                throw std::runtime_error("letrec-syntax: requires syntax-rules form");
            }

            Object* sr_keyword = first(transformer_spec);
            std::string sr_name = get_identifier_name(sr_keyword);

            if (sr_name != "syntax-rules") {
                throw std::runtime_error("letrec-syntax: requires syntax-rules");
            }

            Object* sr_args = rest(transformer_spec);
            Object* literals = first(sr_args);
            Object* rules = rest(sr_args);

            // Create transformer and define in local environment
            SyntaxRulesTransformer* transformer = gc_new<SyntaxRulesTransformer>(literals, rules);
            local_env->define(name, transformer);
        }

        // Evaluate body in local environment with macros defined
        Object* result = nil;
        for (Object* b = body; !b->is_nil(); b = rest(b)) {
            result = new_eval(first(b), local_env);
        }
        return result;

    } else if (fn_val == "case") {
        // case: (case key ((datum1 ...) result1) ... (else result-else))
        // => (cond ((memv key '(datum1 ...)) result1) ... (else result-else))
        Object* key_expr = first(args);
        Object* clauses = rest(args);

        // Build cond clauses
        Object* cond_clauses = nil;
        for (Object* c = clauses; !c->is_nil(); c = rest(c)) {
            Object* clause = first(c);
            Object* datums = first(clause);
            Object* results = rest(clause);

            Object* cond_clause;
            // Check for else clause
            if (is_symbol(datums) && ((a_symbol*)datums)->value() == "else") {
                // (else result...)
                cond_clause = cons(intern_symbol("else"), results);
            } else {
                // ((memv key '(datum...)) result...)
                Object* quoted_datums = cons(intern_symbol("quote"), cons(datums, nil));
                Object* memv_call = cons(intern_symbol("memv"),
                                        cons(key_expr, cons(quoted_datums, nil)));
                cond_clause = cons(memv_call, results);
            }
            cond_clauses = cons(cond_clause, cond_clauses);
        }

        // Reverse to maintain order
        cond_clauses = reverse(cond_clauses);

        // Build and evaluate (cond ...)
        Object* cond_expr = cons(intern_symbol("cond"), cond_clauses);
        return new_eval(cond_expr, env);
    } else if (fn_val == "cond-expand") {
        // cond-expand: (cond-expand (feature-requirement body ...) ...)
        // R7RS conditional compilation based on available features
        // Uses static helper functions to avoid stack overhead

        // Find first matching clause
        for (Object* c = args; !c->is_nil(); c = rest(c)) {
            Object* clause = first(c);
            Object* feature_req = first(clause);
            Object* clause_body = rest(clause);

            // Check for else clause
            bool is_else = is_symbol(feature_req) && ((a_symbol*)feature_req)->value() == "else";

            if (is_else || is_feature_satisfied(feature_req)) {
                // Evaluate body expressions in sequence
                Object* result = nil;
                for (Object* b = clause_body; !b->is_nil(); b = rest(b)) {
                    result = new_eval(first(b), env);
                }
                return result;
            }
        }

        // No matching clause - return unspecified value
        return nil;
    } else if (fn_val == "do") {
        // Desugar do to letrec with named loop (helper function reduces stack overhead)
        Object* letrec_expr = transform_do_to_letrec(args);
        return new_eval(letrec_expr, env);
    } else if (fn_val == "quasiquote") {
        // Expand quasiquote into cons/append/quote operations and evaluate
        Object* template_expr = first(args);
        Object* expanded = qq_expand(template_expr, env);
        return new_eval(expanded, env);
    } else if (fn_val == "guard") {
        // (guard (var clause ...) body ...)  [R7RS 4.2.7]
        // Evaluates body with a handler that escapes back here; on a raise
        // the payload is bound to var and the clauses run cond-style.
        if (args->is_nil() || !args->pair() || !first(args)->pair()) {
            throw std::runtime_error("guard: malformed syntax - expected (guard (var clause ...) body ...)");
        }
        Object* spec = first(args);
        Object* var = first(spec);
        Object* clauses = rest(spec);
        Object* body = rest(args);

        zombie::runtime::exc::GuardHandler* handler =
            gc_new<zombie::runtime::exc::GuardHandler>();
        gc_root<Object> guard_handler_root(handler);

        Object* payload = nullptr;
        {
            zombie::runtime::exc::handler_scope hscope(handler);
            try {
                Object* result = nil;
                for (Object* b = body; !b->is_nil(); b = rest(b)) {
                    result = new_eval(first(b), env);
                }
                return result;
            } catch (const zombie::runtime::exc::GuardEscape& ge) {
                if (ge.token != static_cast<const void*>(handler)) {
                    throw;  // belongs to an outer guard
                }
                payload = ge.payload;
            } catch (const scheme_error& se) {
                // C++-side scheme error (raise with no inner handler, or a
                // primitive error): recover the payload object
                payload = se.exception()
                    ? static_cast<Object*>(se.exception())
                    : static_cast<Object*>(gc_new<a_exception>(se.message(), nil, a_exception::GENERAL));
            } catch (const std::runtime_error& re) {
                // Plain C++ runtime errors (e.g. division by zero) become
                // error objects so clauses can inspect them
                payload = gc_new<a_exception>(re.what(), nil, a_exception::GENERAL);
            }
        }

        // A clause is chosen with var bound to the raised object
        gc_root<Object> guard_payload(payload);
        Object* var_list = cons(var, nil);
        gc_root<Object> guard_vars(var_list);
        Object* val_list = cons(payload, nil);
        gc_root<Object> guard_vals(val_list);
        Environment* guard_env = gc_new<Environment>("guard", var_list, val_list, env);
        gc_root<Object> guard_env_root(guard_env);

        for (Object* c = clauses; !c->is_nil(); c = rest(c)) {
            Object* clause = first(c);
            if (!clause->pair()) {
                throw std::runtime_error("guard: each clause must be (test expr ...)");
            }
            Object* test = first(clause);
            bool is_else = get_identifier_name(test) == "else";
            Object* test_val = is_else ? nullptr : new_eval(test, guard_env);
            if (is_else || truth(test_val)) {
                Object* result = is_else ? nil : test_val;
                for (Object* e = rest(clause); !e->is_nil(); e = rest(e)) {
                    result = new_eval(first(e), guard_env);
                }
                return result;
            }
        }

        // No clause matched: re-raise to the outer handlers
        return zombie::runtime::exc::raise_object(this, env, payload, false);
    } else if (fn_val == "parameterize") {
        // (parameterize ((param value) ...) body ...)  [R7RS 4.2.6]
        // Converters apply to the new values; old values are restored on
        // exit without re-applying converters, even on non-local exit.
        if (args->is_nil() || !args->pair()) {
            throw std::runtime_error("parameterize: malformed syntax - expected bindings and body");
        }
        Object* bindings = first(args);
        Object* body = rest(args);
        if (!bindings->is_nil() && !bindings->pair()) {
            throw std::runtime_error("parameterize: bindings must be a list");
        }

        std::vector<a_parameter*> params;
        std::vector<Object*> new_values;
        std::vector<Object*> old_values;
        std::vector<gc_ptr<Object>> roots;  // keep everything alive across body eval

        // Evaluate all params and values before any assignment (R7RS)
        for (Object* b = bindings; !b->is_nil(); b = rest(b)) {
            Object* binding = first(b);
            if (!binding->pair() || rest(binding)->is_nil()) {
                throw std::runtime_error("parameterize: each binding must be (param value)");
            }
            Object* param_obj = new_eval(first(binding), env);
            a_parameter* param = dynamic_cast<a_parameter*>(param_obj);
            if (!param) {
                throw std::runtime_error("parameterize: not a parameter object");
            }
            roots.emplace_back(gc_ptr<Object>(param_obj));
            Object* value = new_eval(second(binding), env);
            roots.emplace_back(gc_ptr<Object>(value));
            params.push_back(param);
            new_values.push_back(value);
        }

        // Save old values, then set new ones (converter applies)
        for (size_t i = 0; i < params.size(); i++) {
            old_values.push_back(params[i]->get_value());
            roots.emplace_back(gc_ptr<Object>(old_values[i]));
            params[i]->set_value(this, env, new_values[i]);
        }

        // RAII restore so old values come back even when body raises
        struct param_restore {
            std::vector<a_parameter*>& params;
            std::vector<Object*>& olds;
            ~param_restore() {
                for (size_t i = 0; i < params.size(); i++) {
                    params[i]->set_value(olds[i]);  // raw set: no converter
                }
            }
        } restore{ params, old_values };

        Object* result = nil;
        for (Object* b = body; !b->is_nil(); b = rest(b)) {
            result = new_eval(first(b), env);
        }
        return result;
    } else if (fn_val == "***EXIT_SYS!") {
        exit(0);
    } else if (fn_val == "***SYS_MESSAGE!") {
        std::string message = ((a_string*)args)->value();
        //AfxMessageBox(CString(message.c_str()));
        return nil;  // Was falling off the end of the function (C4715/UB)
    } else {
        Object* proc_obj = new_eval(car(expr_as_pair), env);
        // GC SAFETY: Protect proc_obj during subsequent allocations (eval_args, expand, etc.)
        gc_root<Object> guard_proc(proc_obj);

        if (proc_obj->get_type() != 0) {
            //throw std::runtime_error("list element must be a Procedure");
            return proc_obj;
        }

        // Check if it's a syntax-rules Macro FIRST (before Procedure check)
        SyntaxRulesTransformer* transformer = dynamic_cast<SyntaxRulesTransformer*>(proc_obj);
        if (transformer) {
            // Pass the FULL form to the expander (pattern includes the Macro name)
            Object* expansion = transformer->expand(expr_as_pair, env);
            gc_root<Object> guard_expansion(expansion);  // GC SAFETY
            Object* result = new_eval(expansion, env);
            return result;
        }

        // Check if it's a Macro - if so, expand and evaluate
        if (((Base*)proc_obj)->instance_of() == static_cast<int>(Base::atype::MACRO)) {
            ConsCell* expanded = ((Macro*)proc_obj)->expand(this, expr_as_pair, cdr(expr_as_pair), env);
            gc_root<Object> guard_expanded((Object*)expanded);  // GC SAFETY
            Object* result = new_eval(expanded, env);
            return result;
        }

        // Now check if it's a Procedure
        Procedure* proc = dynamic_cast<Procedure*>(proc_obj);
        if (proc == NULL) {
            return expr_as_pair;
        }

        // Evaluate arguments before passing to Procedure (primitives expect evaluated args)
        Object* evaluated_args = eval_args(cdr(expr_as_pair), env);
        return proc->apply(this, evaluated_args, env);
    }
}
// TCO: eval_step - one step of evaluation returning eval_result
eval_result Lisp::eval_step(Object* x, Environment* env, bool treat_as_single_arg) {
    if (treat_as_single_arg) {
        return eval_result::make_value(x);
    }

    if (x->is_atom()) {
        // Atoms evaluate to values (no tail calls from atoms)
        return eval_result::make_value(eval_atom(x, env));
    } else {
        // Lists may contain tail calls
        return eval_step_list(x, env);
    }
}

// TCO: eval_step_list - evaluate list forms, detecting tail position
eval_result Lisp::eval_step_list(Object* x, Environment* env) {
    ConsCell* expr_as_pair = dynamic_cast<ConsCell*>(x);
    if (expr_as_pair == nullptr) {
        throw std::runtime_error("Invalid expression type");
    }

    Object* fn = first(x);
    Object* args = rest(x);

    // Check if fn is a symbol or string (special form name) before attempting special form dispatch
    // If fn is neither (e.g., a lambda expression), skip to Procedure application
    if (fn->is_atom() && (is_string(fn) || is_symbol(fn))) {
        std::string fn_val = get_identifier_name(fn);

        // Special forms - some can be tail calls
        if (fn_val == "quote") {
        return eval_result::make_value(first(args));
    }
    else if (fn_val == "define") {
        if (!first(args)->is_atom()) {
            // (define (name args...) body...)
            // Transform to: (define name (lambda (args...) body...))

            // Build lambda expression with guards
            gc_root<Object> lambda_sym(gc_new<a_string>("lambda"));
            gc_root<Object> lambda_args(rest(first(args)));
            gc_root<Object> lambda_body(rest(args));
            gc_root<Object> inner_cons(cons(lambda_args.get(), lambda_body.get()));
            gc_root<Object> lambda_expr(cons(lambda_sym.get(), inner_cons.get()));

            Object* result = new_eval(lambda_expr.get(), env);
            return eval_result::make_value(env->define(first(first(args)), result));
        } else {
            Object* result = new_eval(second(args), env);
            return eval_result::make_value(env->define(first(args), result));
        }
    }
    else if (fn_val == "define-syntax") {
        // (define-syntax name (syntax-rules (literals) (pattern template) ...))
        Object* name = first(args);
        Object* transformer_spec = second(args);

        // Parse (syntax-rules ...)
        if (transformer_spec->is_atom()) {
            return eval_result::make_value(gc_new<a_string>("ERROR: define-syntax requires syntax-rules form"));
        }

        Object* sr_keyword = first(transformer_spec);
        std::string sr_name = get_identifier_name(sr_keyword);

        if (sr_name != "syntax-rules") {
            return eval_result::make_value(gc_new<a_string>("ERROR: define-syntax requires syntax-rules"));
        }

        Object* sr_args = rest(transformer_spec);
        Object* literals = first(sr_args);
        Object* rules = rest(sr_args);

        // Create transformer
        SyntaxRulesTransformer* transformer = gc_new<SyntaxRulesTransformer>(literals, rules);

        // Define in environment
        return eval_result::make_value(env->define(name, transformer));
    }
    else if (fn_val == "import") {
        // (import <import-set> ...)
        // Import symbols from libraries into current environment
        // Supports: simple, only, except, prefix, rename

        r7rs::LibraryParser parser;

        for (Object* import_set_list = args; !import_set_list->is_nil(); import_set_list = rest(import_set_list)) {
            Object* import_set_expr = first(import_set_list);

            // Parse the import set (handles all import types)
            r7rs::ImportSpec import_spec = parser.parse_import_set(import_set_expr);

            // Get the library from registry
            r7rs::Library* source_lib = r7rs::LibraryRegistry::instance().get_library(import_spec.library);
            if (!source_lib) {
                throw std::runtime_error("import: library not found: " + r7rs::library_name_to_string(import_spec.library));
            }

            // Get all exported symbols from the source library
            const std::vector<r7rs::ExportSpec>& exports = source_lib->exports();

            // Process based on import type
            switch (import_spec.type) {
            case r7rs::ImportSpec::SIMPLE: {
                // Import all exported symbols
                for (const auto& exp : exports) {
                    Object* value = source_lib->get_symbol(exp.internal_name);
                    if (value) {
                        env->define(gc_new<a_string>(exp.external_name), value);
                    }
                }
                break;
            }

            case r7rs::ImportSpec::ONLY: {
                // Import only specified symbols
                for (const auto& exp : exports) {
                    if (import_spec.symbols.find(exp.external_name) != import_spec.symbols.end()) {
                        Object* value = source_lib->get_symbol(exp.internal_name);
                        if (value) {
                            env->define(gc_new<a_string>(exp.external_name), value);
                        }
                    }
                }
                break;
            }

            case r7rs::ImportSpec::EXCEPT: {
                // Import all except specified symbols
                for (const auto& exp : exports) {
                    if (import_spec.symbols.find(exp.external_name) == import_spec.symbols.end()) {
                        Object* value = source_lib->get_symbol(exp.internal_name);
                        if (value) {
                            env->define(gc_new<a_string>(exp.external_name), value);
                        }
                    }
                }
                break;
            }

            case r7rs::ImportSpec::PREFIX: {
                // Import all with prefix
                for (const auto& exp : exports) {
                    Object* value = source_lib->get_symbol(exp.internal_name);
                    if (value) {
                        std::string prefixed_name = import_spec.prefix + exp.external_name;
                        env->define(gc_new<a_string>(prefixed_name), value);
                    }
                }
                break;
            }

            case r7rs::ImportSpec::RENAME: {
                // Import with renames
                for (const auto& exp : exports) {
                    Object* value = source_lib->get_symbol(exp.internal_name);
                    if (value) {
                        // Check if this symbol should be renamed
                        auto rename_it = import_spec.renames.find(exp.external_name);
                        if (rename_it != import_spec.renames.end()) {
                            // Use renamed name
                            env->define(gc_new<a_string>(rename_it->second), value);
                        } else {
                            // Use original name
                            env->define(gc_new<a_string>(exp.external_name), value);
                        }
                    }
                }
                break;
            }
            }
        }
        return eval_result::make_value(nil);  // import returns unspecified value
    }
    else if (fn_val == "define-library") {
        // (define-library <library name> <library declaration> ...)

        // Parse the library definition
        r7rs::LibraryParser parser;
        auto lib_ptr = parser.parse_library_definition(x);
        r7rs::Library* lib = lib_ptr.get();

        // Initialize the library (process imports)
        std::string error;
        if (!lib->initialize(env, error)) {
            throw std::runtime_error("define-library: initialization failed: " + error);
        }

        // Evaluate begin blocks
        auto eval_func = [this](Object* form, Environment* env_arg) -> Object* {
            return this->new_eval(form, env_arg);
        };

        if (!lib->evaluate_begin_blocks(eval_func, error)) {
            throw std::runtime_error("define-library: evaluation failed: " + error);
        }

        // Register the library in the global registry
        r7rs::LibraryRegistry::instance().register_library(std::move(lib_ptr));

        return eval_result::make_value(nil);  // define-library returns unspecified value
    }
    else if (fn_val == "begin") {
        // Evaluate all but last expression for side effects
        // Return TAILCALL for the last expression (tail position!)
        Object* current = args;
        while (current != nullptr && !current->is_nil() && rest(current) != nullptr && !rest(current)->is_nil()) {
            ConsCell* cons_current = dynamic_cast<ConsCell*>(current);
            if (cons_current != nullptr) {
                new_eval(first(current), env);  // Evaluate for side effect
                current = rest(current);
            } else {
                break;
            }
        }

        // Last expression is in tail position - return TAILCALL
        if (current != nullptr && !current->is_nil()) {
            return eval_result::make_tailcall(first(current), env);
        } else {
            return eval_result::make_value(nil);
        }
    }
    else if (fn_val == "set!") {
        Object* val = new_eval(second(args), env);
        return eval_result::make_value(env->set(first(args), val));
    }
    else if (fn_val == "if") {
        // Evaluate condition
        Object* cond = new_eval(first(args), env);

        // Choose branch based on condition (tail call!)
        Object* branch = truth(cond) ? second(args) : third(args);
        return eval_result::make_tailcall(branch, env);
    }
    else if (fn_val == "when") {
        // (when test expr1 expr2 ...) → (if test (begin expr1 expr2 ...))
        Object* test = first(args);
        Object* body = rest(args);
        Object* test_result = new_eval(test, env);
        if (truth(test_result)) {
            // Execute body expressions in sequence, last one is tail call
            Object* result = nil;
            while (!body->is_nil()) {
                Object* expr = first(body);
                Object* remaining = rest(body);
                if (remaining->is_nil()) {
                    // Last expression - tail call
                    return eval_result::make_tailcall(expr, env);
                } else {
                    // Not last - evaluate for side effects
                    result = new_eval(expr, env);
                    body = remaining;
                }
            }
            return eval_result::make_value(result);
        } else {
            return eval_result::make_value(nil);  // when returns unspecified (nil) when test is false
        }
    }
    else if (fn_val == "unless") {
        // (unless test expr1 expr2 ...) → (if (not test) (begin expr1 expr2 ...))
        Object* test = first(args);
        Object* body = rest(args);
        Object* test_result = new_eval(test, env);
        if (!truth(test_result)) {
            // Execute body expressions in sequence, last one is tail call
            Object* result = nil;
            while (!body->is_nil()) {
                Object* expr = first(body);
                Object* remaining = rest(body);
                if (remaining->is_nil()) {
                    // Last expression - tail call
                    return eval_result::make_tailcall(expr, env);
                } else {
                    // Not last - evaluate for side effects
                    result = new_eval(expr, env);
                    body = remaining;
                }
            }
            return eval_result::make_value(result);
        } else {
            return eval_result::make_value(nil);  // unless returns unspecified (nil) when test is true
        }
    }
    else if (fn_val == "and") {
        // R5RS 4.2.1: (and) => #t. Short-circuit on the first false value; the
        // last subexpression is evaluated in tail position for TCO.
        if (args->is_nil()) return eval_result::make_value((Object*)t);
        while (!args->is_nil()) {
            Object* expr = first(args);
            Object* remaining = rest(args);
            if (remaining->is_nil()) {
                return eval_result::make_tailcall(expr, env);  // value is the result
            }
            Object* v = new_eval(expr, env);
            if (!truth(v)) return eval_result::make_value(v);  // short-circuit false
            args = remaining;
        }
        return eval_result::make_value((Object*)t);  // unreachable
    }
    else if (fn_val == "or") {
        // R5RS 4.2.1: (or) => #f. Short-circuit on the first true value; the
        // last subexpression is evaluated in tail position for TCO.
        while (!args->is_nil()) {
            Object* expr = first(args);
            Object* remaining = rest(args);
            if (remaining->is_nil()) {
                return eval_result::make_tailcall(expr, env);  // value is the result
            }
            Object* v = new_eval(expr, env);
            if (truth(v)) return eval_result::make_value(v);  // short-circuit true
            args = remaining;
        }
        return eval_result::make_value((Object*)false_obj);  // (or) => #f
    }
    else if (fn_val == "cond") {
        // reduce_cond returns the expression to evaluate
        Object* result_expr = reduce_cond(args, env);
        return eval_result::make_tailcall(result_expr, env);
    }
    else if (fn_val == "case") {
        // Desugar case to cond with memv checks
        Object* key_expr = first(args);
        Object* clauses = rest(args);
        Object* cond_clauses = nil;
        for (Object* c = clauses; !c->is_nil(); c = rest(c)) {
            Object* clause = first(c);
            Object* datums = first(clause);
            Object* results = rest(clause);
            Object* cond_clause;
            if (is_symbol(datums) && ((a_symbol*)datums)->value() == "else") {
                cond_clause = cons(intern_symbol("else"), results);
            } else {
                Object* quoted_datums = cons(intern_symbol("quote"), cons(datums, nil));
                Object* memv_call = cons(intern_symbol("memv"),
                                        cons(key_expr, cons(quoted_datums, nil)));
                cond_clause = cons(memv_call, results);
            }
            cond_clauses = cons(cond_clause, cond_clauses);
        }
        cond_clauses = reverse(cond_clauses);
        Object* cond_expr = cons(intern_symbol("cond"), cond_clauses);
        return eval_result::make_tailcall(cond_expr, env);
    }
    else if (fn_val == "cond-expand") {
        // cond-expand: (cond-expand (feature-requirement body ...) ...)
        // R7RS conditional compilation based on available features
        // Uses static helper functions to avoid stack overhead

        // Find first matching clause
        for (Object* c = args; !c->is_nil(); c = rest(c)) {
            Object* clause = first(c);
            Object* feature_req = first(clause);
            Object* clause_body = rest(clause);

            // Check for else clause
            bool is_else = is_symbol(feature_req) && ((a_symbol*)feature_req)->value() == "else";

            if (is_else || is_feature_satisfied(feature_req)) {
                // Build (begin ...) form for the body
                Object* begin_form = cons(intern_symbol("begin"), clause_body);
                return eval_result::make_tailcall(begin_form, env);
            }
        }

        // No matching clause - return unspecified value
        return eval_result::make_value(nil);
    }
    else if (fn_val == "do") {
        // Desugar do to letrec with named loop (helper function reduces stack overhead)
        Object* letrec_expr = transform_do_to_letrec(args);
        return eval_result::make_tailcall(letrec_expr, env);
    }
    else if (fn_val == "quasiquote") {
        // Expand quasiquote into cons/append/quote operations and evaluate
        Object* template_expr = first(args);
        Object* expanded = qq_expand(template_expr, env);
        return eval_result::make_tailcall(expanded, env);
    }
    else if (fn_val == "lambda") {
        return eval_result::make_value(gc_new<Closure>(first(args), rest(args), env));
    }
    else if (fn_val == "case-lambda") {
        // (case-lambda ((formals body ...) ...))
        // Parse clauses and create case_lambda Procedure
        std::vector<case_lambda_clause> clauses;

        for (Object* clause_list = args; !clause_list->is_nil(); clause_list = rest(clause_list)) {
            if (!clause_list->pair()) {
                throw std::runtime_error("case-lambda: clauses must be a proper list");
            }

            Object* clause = first(clause_list);
            if (!clause->pair()) {
                throw std::runtime_error("case-lambda: each clause must be (formals body ...)");
            }

            Object* formals = first(clause);
            Object* body = rest(clause);

            if (body->is_nil()) {
                throw std::runtime_error("case-lambda: clause body cannot be empty");
            }

            clauses.push_back(case_lambda_clause(formals, body));
        }

        if (clauses.empty()) {
            throw std::runtime_error("case-lambda: requires at least one clause");
        }

        return eval_result::make_value(gc_new<a_case_lambda>(clauses, env));
    }
    else if (fn_val == "delay") {
        // Transform (delay expr) to (lambda () expr) - create a promise/thunk
        Object* expr = first(args);
        Object* lambda_body = cons(expr, nil);
        return eval_result::make_value(gc_new<Closure>(nil, lambda_body, env));  // nil = no parameters
    }
    else if (fn_val == "Macro") {
        Macro* m = gc_new<Macro>(first(args), rest(args), env);
        return eval_result::make_value(m);
    }
    else if (fn_val == "let") {
        // Validate args before processing
        if (args->is_nil() || !args->pair()) {
            throw std::runtime_error("let: malformed syntax - expected bindings and body");
        }

        // Named let
        // Note: nil is an atom, but (let () body) is regular let, not named let
        if (first(args)->is_atom() && !first(args)->is_nil()) {
            Object* name = first(args);
            Object* bindings = second(args);
            Object* body = rest(rest(args));

            // Validate bindings (must be nil or a list)
            if (!bindings->is_nil() && !bindings->pair()) {
                throw std::runtime_error("named let: bindings must be a list");
            }

            Object* vars = nil;
            Object* vals = nil;
            for (Object* b = bindings; !b->is_nil(); b = rest(b)) {
                // Validate each element is a pair
                if (!b->pair()) {
                    throw std::runtime_error("named let: malformed bindings list");
                }
                Object* binding = first(b);
                // Validate binding is a pair (var val)
                if (!binding->pair()) {
                    throw std::runtime_error("named let: each binding must be (var val)");
                }
                vars = cons(first(binding), vars);
                vals = cons(second(binding), vals);
            }
            vars = reverse(vars);
            vals = reverse(vals);

            Object* lambda_expr = cons(gc_new<a_string>("lambda"), cons(vars, body));
            Object* letrec_binding = cons(cons(name, cons(lambda_expr, nil)), nil);
            Object* call_expr = cons(name, vals);
            Object* letrec_expr = cons(gc_new<a_string>("letrec"), cons(letrec_binding, cons(call_expr, nil)));

            return eval_result::make_tailcall(letrec_expr, env);
        } else {
            // Regular let
            Object* bindings = first(args);
            Object* body = rest(args);

            // Validate bindings (must be nil or a list)
            if (!bindings->is_nil() && !bindings->pair()) {
                throw std::runtime_error("let: bindings must be a list");
            }

            Object* vars = nil;
            Object* vals = nil;

            for (Object* b = bindings; !b->is_nil(); b = rest(b)) {
                // Validate each binding is a pair
                if (!b->pair()) {
                    throw std::runtime_error("let: malformed bindings list");
                }
                Object* binding = first(b);
                // Validate binding is a pair (var val)
                if (!binding->pair()) {
                    throw std::runtime_error("let: each binding must be (var val)");
                }
                vars = cons(first(binding), vars);
                vals = cons(second(binding), vals);
            }

            vars = reverse(vars);
            vals = reverse(vals);

            Object* lambda_expr = cons(gc_new<a_string>("lambda"), cons(vars, body));
            Object* call_expr = cons(lambda_expr, vals);

            return eval_result::make_tailcall(call_expr, env);
        }
    }
    else if (fn_val == "let*") {
        Object* bindings = first(args);
        Object* body = rest(args);

        if (bindings->is_nil()) {
            Object* begin_expr = cons(gc_new<a_string>("begin"), body);
            return eval_result::make_tailcall(begin_expr, env);
        } else {
            Object* first_binding = first(bindings);
            Object* var = first(first_binding);
            Object* val_expr = second(first_binding);
            Object* rest_bindings = rest(bindings);

            Object* val = new_eval(val_expr, env);

            Object* lambda_expr = cons(gc_new<a_string>("lambda"),
                cons(cons(var, nil),
                    cons(cons(gc_new<a_string>("let*"),
                        cons(rest_bindings, body)), nil)));

            Object* call_expr = cons(lambda_expr, cons(val, nil));

            return eval_result::make_tailcall(call_expr, env);
        }
    }
    else if (fn_val == "letrec") {
        Object* bindings = first(args);
        Object* body = rest(args);

        Object* vars = nil;
        for (Object* b = bindings; !b->is_nil(); b = rest(b)) {
            vars = cons(first(first(b)), vars);
        }
        vars = reverse(vars);

        Object* nil_vals = nil;
        for (Object* v = vars; !v->is_nil(); v = rest(v)) {
            nil_vals = cons(nil, nil_vals);
        }
        nil_vals = reverse(nil_vals);

        Object* lambda_expr = cons(gc_new<a_string>("lambda"), cons(vars, body));
        Object* outer_call = cons(lambda_expr, nil_vals);

        Environment* letrec_env = gc_new<Environment>("letrec-env", vars, nil_vals, env);

        for (Object* b = bindings; !b->is_nil(); b = rest(b)) {
            Object* binding = first(b);
            Object* var = first(binding);
            Object* val_expr = second(binding);
            Object* val = new_eval(val_expr, letrec_env);
            letrec_env->set(var, val);
        }

        Object* begin_expr = cons(gc_new<a_string>("begin"), body);
        return eval_result::make_tailcall(begin_expr, letrec_env);
    }
    else if (fn_val == "let-syntax") {
        // let-syntax: (let-syntax ((name1 (syntax-rules ...)) ...) body...)
        Object* bindings = first(args);
        Object* body = rest(args);

        // Create new environment extending current one
        Environment* local_env = gc_new<Environment>("let-syntax", nil, nil, env);

        // Define each Macro in the local environment
        for (Object* b = bindings; !b->is_nil(); b = rest(b)) {
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

        // Body is in tail position - evaluate as begin
        Object* begin_expr = cons(gc_new<a_string>("begin"), body);
        return eval_result::make_tailcall(begin_expr, local_env);
    }
    else if (fn_val == "letrec-syntax") {
        // letrec-syntax: (letrec-syntax ((name1 (syntax-rules ...)) ...) body...)
        Object* bindings = first(args);
        Object* body = rest(args);

        // Create new environment extending current one
        Environment* local_env = gc_new<Environment>("letrec-syntax", nil, nil, env);

        // Define each Macro in the local environment
        for (Object* b = bindings; !b->is_nil(); b = rest(b)) {
            Object* binding = first(b);
            Object* name = first(binding);
            Object* transformer_spec = second(binding);

            // Parse (syntax-rules ...)
            if (transformer_spec->is_atom()) {
                throw std::runtime_error("letrec-syntax: requires syntax-rules form");
            }

            Object* sr_keyword = first(transformer_spec);
            std::string sr_name = get_identifier_name(sr_keyword);

            if (sr_name != "syntax-rules") {
                throw std::runtime_error("letrec-syntax: requires syntax-rules");
            }

            Object* sr_args = rest(transformer_spec);
            Object* literals = first(sr_args);
            Object* rules = rest(sr_args);

            // Create transformer and define in local environment
            SyntaxRulesTransformer* transformer = gc_new<SyntaxRulesTransformer>(literals, rules);
            local_env->define(name, transformer);
        }

        // Body is in tail position - evaluate as begin
        Object* begin_expr = cons(gc_new<a_string>("begin"), body);
        return eval_result::make_tailcall(begin_expr, local_env);
    }
    else if (fn_val == "guard" || fn_val == "parameterize") {
        // Dynamic-extent forms need C++ RAII frames (handler install /
        // parameter restore), which a trampolined tail call would discard.
        // Delegate to the recursive evaluator; their bodies lose TCO.
        return eval_result::make_value(eval_list(x, env));
    }
    } // End of special form string checks

    // Function application (reached if fn is not a string special form, or if it didn't match any special form)
    {
        Object* proc = new_eval(fn, env);
        // GC SAFETY: Protect proc during subsequent allocations
        gc_root<Object> guard_proc(proc);

        // Handle syntax-rules Macro expansion FIRST
        SyntaxRulesTransformer* transformer = dynamic_cast<SyntaxRulesTransformer*>(proc);
        if (transformer) {
            // Pass full form to expander (pattern includes Macro name)
            Object* expansion = transformer->expand(x, env);
            return eval_result::make_tailcall(expansion, env);
        }

        // Handle Macro expansion
        Macro* m = dynamic_cast<Macro*>(proc);
        if (m) {
            // Use args that was already extracted at the top of the function
            Object* expanded = m->apply(this, args, env);
            return eval_result::make_tailcall(expanded, env);
        }

        // Regular Procedure application
        Procedure* p = dynamic_cast<Procedure*>(proc);
        if (!p) {
            // Not a Procedure - check if it's a value that should be returned
            if (proc->get_type() != 0) {
                // It's a value (non-Procedure), just return it
                return eval_result::make_value(proc);
            }
            throw std::runtime_error("Not a Procedure");
        }

        // Evaluate arguments (use args that was already extracted)
        if (!args) {
            args = nil;
        }
        Object* evaluated_args = eval_args(args, env);
        gc_root<Object> guard_args(evaluated_args);  // GC SAFETY

        // TCO: Check if this is a Closure - if so, we can optimize as a tail call!
        Closure* c = dynamic_cast<Closure*>(proc);
        if (c) {
            // Tail call optimization for closures
            // Instead of calling apply() which uses C++ stack, we return a TAILCALL
            // Create new environment with parameters bound to evaluated arguments
            Environment* call_env = c->m_env->extend(c->m_parms, evaluated_args);

            // If this is a Macro, we need to evaluate the result again in the original env
            // But we'll handle that by checking after evaluation
            // For now, just tail-call to the body

            // Return a tail call to evaluate the body in the new environment
            return eval_result::make_tailcall(c->m_body, call_env);
        }

        // For other procedures (primitives), we must call apply() directly
        // Primitives are written in C++ and don't have a Scheme body to tail-call to
        Object* result = p->apply(this, evaluated_args, env);
        return eval_result::make_value(result);
    }
}

// Helper function to expand quasiquote recursively
// Processes quasiquote, unquote, and unquote-splicing
Object* Lisp::qq_expand(Object* x, Environment* env) {
    // Base case: atom or nil
    if (x == nil) {
        return cons(gc_new<a_string>("quote"), cons(nil, nil));
    }

    if (x->is_atom()) {
        // Return it quoted
        return cons(gc_new<a_string>("quote"), cons(x, nil));
    }

    // x is a list - check for special forms
    Object* first_elem = first(x);

    // Check if it's (unquote expr)
    if (is_string(first_elem) || is_symbol(first_elem)) {
        std::string name = is_string(first_elem) ?
            ((a_string*)first_elem)->value() :
            ((a_symbol*)first_elem)->value();

        if (name == "unquote") {
            // (unquote expr) - return expr to be evaluated
            return second(x);
        }

        if (name == "unquote-splicing") {
            // unquote-splicing at top level is an error
            return error("unquote-splicing: not in list context");
        }
    }

    // x is a regular list - process each element and build result
    return qq_expand_list(x, env);
}

// Helper to expand a list in quasiquote context
Object* Lisp::qq_expand_list(Object* x, Environment* env) {
    if (x == nil) {
        return cons(intern_symbol("quote"), cons(nil, nil));
    }

    Object* elem = first(x);
    Object* rest_list = rest(x);

    // Check if elem is (unquote-splicing expr)
    if (!elem->is_atom()) {
        Object* elem_first = first(elem);
        if (is_string(elem_first) || is_symbol(elem_first)) {
            std::string elem_name = is_string(elem_first) ?
                ((a_string*)elem_first)->value() :
                ((a_symbol*)elem_first)->value();

            if (elem_name == "unquote-splicing") {
                // Build (append splice-expr (qq-expand rest))
                Object* splice_expr = second(elem);
                Object* rest_expanded = qq_expand_list(rest_list, env);
                return cons(intern_symbol("append"),
                    cons(splice_expr, cons(rest_expanded, nil)));
            }
        }
    }

    // Regular element - build (cons (qq-expand elem) (qq-expand rest))
    Object* elem_expanded = qq_expand(elem, env);
    Object* rest_expanded = qq_expand_list(rest_list, env);
    return cons(intern_symbol("cons"),
        cons(elem_expanded, cons(rest_expanded, nil)));
}

// Helper function to transform do to letrec (extracted to reduce stack overhead in eval functions)
// (do ((var init step) ...) (test result ...) body ...)
// => (letrec ((loop (lambda (var ...) (if test (begin result ...) (begin body ... (loop step ...)))))) (loop init ...))
Object* Lisp::transform_do_to_letrec(Object* args) {
    Object* bindings = first(args);
    Object* test_clause = second(args);
    Object* body = rest(rest(args));

    Object* vars = nil;
    Object* inits = nil;
    Object* steps = nil;

    // Extract vars, inits, and steps from bindings
    for (Object* b = bindings; !b->is_nil(); b = rest(b)) {
        Object* binding = first(b);
        vars = cons(first(binding), vars);
        inits = cons(second(binding), inits);
        // step defaults to var if not provided
        Object* step = rest(rest(binding));
        if (step->is_nil()) {
            steps = cons(first(binding), steps);
        } else {
            steps = cons(first(step), steps);
        }
    }
    vars = reverse(vars);
    inits = reverse(inits);
    steps = reverse(steps);

    // Build the loop body: (if test (begin result ...) (begin body ... (loop step ...)))
    Object* test_expr = first(test_clause);
    Object* result_exprs = rest(test_clause);
    Object* result_begin = result_exprs->is_nil() ? nil :
        (rest(result_exprs)->is_nil() ? first(result_exprs) : cons(intern_symbol("begin"), result_exprs));

    Object* loop_call = cons(intern_symbol("loop"), steps);
    Object* body_with_loop = append_two_lists(body, cons(loop_call, nil));
    Object* body_begin = body_with_loop->is_nil() ? loop_call :
        (rest(body_with_loop)->is_nil() ? loop_call : cons(intern_symbol("begin"), body_with_loop));

    Object* if_expr = cons(intern_symbol("if"), cons(test_expr, cons(result_begin, cons(body_begin, nil))));

    // Build lambda: (lambda (var ...) if_expr)
    Object* lambda_expr = cons(intern_symbol("lambda"), cons(vars, cons(if_expr, nil)));

    // Build letrec: (letrec ((loop lambda_expr)) (loop init ...))
    Object* loop_binding = cons(cons(intern_symbol("loop"), cons(lambda_expr, nil)), nil);
    Object* init_call = cons(intern_symbol("loop"), inits);
    return cons(intern_symbol("letrec"), cons(loop_binding, cons(init_call, nil)));
}

} } // namespace zombie::runtime
