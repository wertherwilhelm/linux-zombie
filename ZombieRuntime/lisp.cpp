#include "zombie_runtime_types.h"
#include <unordered_set>
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
            Object* elem_evaluated = eval_impl(first(current), env);

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

Object* Lisp::eval_impl(Object* x, bool treat_as_single_arg) {
    return eval_impl(x, this->m_global_env, treat_as_single_arg);
}

Object* Lisp::eval_impl(Object* x, Environment*env, bool treat_as_single_arg) {
    // Trampoline loop for Tail Call Optimization (TCO)
    // Instead of recursing, we loop and evaluate tail calls iteratively
    eval_result result = eval_result::make_tailcall(x, env);

    while (result.tag == eval_result::TAILCALL) {
        result = eval_step(result.expr, result.env, treat_as_single_arg);
    }

    return result.value;
}

// TCO: Local trampolining - evaluates expression to a value without creating new stack frame
// This is critical for unlimited TCO: by trampolining locally instead of calling eval_impl(),
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
    } else if (x->get_type() == static_cast<int>(Type::VECTOR) ||
               x->get_type() == static_cast<int>(Type::BYTEVECTOR)) {
        // R7RS: vector and bytevector constants are self-evaluating.
        return x;
    } else {
        throw std::runtime_error("Unexpected atomic type");
    }
}

// we need to pass the current env in.
Object* Lisp::eval_list(Object* x, Environment*env)
{
    // Reached ONLY from eval_step_list, which delegates the two dynamic-extent forms
    // `guard` and `parameterize` here: they need a live C++ RAII frame during body
    // evaluation (handler install / parameter restore) that a trampolined tail call
    // would discard. Every other special form is handled by eval_step_list's trampoline;
    // the full duplicate dispatch that used to live here was dead code and is removed.
    Object* fn = first(x);
    Object* args = rest(x);
    std::string fn_val = get_identifier_name(fn);

    if (fn_val == "guard") {
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
                    result = eval_impl(first(b), env);
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
            Object* test_val = is_else ? nullptr : eval_impl(test, guard_env);
            if (is_else || truth(test_val)) {
                Object* result = is_else ? nil : test_val;
                for (Object* e = rest(clause); !e->is_nil(); e = rest(e)) {
                    result = eval_impl(first(e), guard_env);
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
            Object* param_obj = eval_impl(first(binding), env);
            a_parameter* param = dynamic_cast<a_parameter*>(param_obj);
            if (!param) {
                throw std::runtime_error("parameterize: not a parameter object");
            }
            roots.emplace_back(gc_ptr<Object>(param_obj));
            Object* value = eval_impl(second(binding), env);
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
            result = eval_impl(first(b), env);
        }
        return result;
    }

    // Defensive: eval_step_list only sends guard/parameterize here.
    return eval_to_value(x, env);
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

        // Unify derived-form desugaring through the shared Expander — the single
        // source of truth used by every evaluator. These forms are desugared to
        // core forms (if/let/lambda/begin/...) and tail-called, exactly as a
        // macro use is expanded on encounter further below. This eliminates the
        // tree-walk's parallel reimplementations (the recurring source of
        // cross-evaluator divergences: named let, cond =>, let-values, ...).
        // Pure derived forms desugared to core forms. NOT case-lambda: it is a
        // procedure constructor (like lambda), kept native — delegating it to the
        // Expander's dispatch desugaring is both unnecessary and far slower.
        static const std::unordered_set<std::string> kExpanderForms = {
            "when", "unless", "and", "or", "cond", "case", "do",
            "let", "let*", "letrec", "letrec*", "let-values", "let*-values",
            "quasiquote", "define-record-type", "define-values", "receive",
            "delay", "delay-force"
        };
        if (kExpanderForms.count(fn_val)) {
            Expander expander(env);
            Object* desugared = expander.expand(x);
            // Displacement cache: the tree-walk re-evaluates the SAME AST on
            // every call (e.g. a loop body), so re-expanding per encounter is
            // quadratic. Rewrite this cons cell in place to its desugared form;
            // the desugaring is deterministic, so this is a pure one-time cache
            // and the next encounter dispatches directly on the core form.
            if (!desugared->is_atom() && desugared != x) {
                set_first(x, first(desugared));
                set_rest(x, rest(desugared));
                return eval_result::make_tailcall(x, env);
            }
            return eval_result::make_tailcall(desugared, env);
        }

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

            Object* result = eval_impl(lambda_expr.get(), env);
            env->define(first(first(args)), result);
            // R7RS: the value of a define is unspecified (nil is this codebase's
            // unspecified value, as used by when/unless/import/one-armed if).
            return eval_result::make_value(nil);
        } else {
            Object* result = eval_impl(second(args), env);
            env->define(first(args), result);
            return eval_result::make_value(nil);  // R7RS: define is unspecified
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

            // Bind the imported symbols into the current environment.
            r7rs::resolve_import(import_spec, source_lib,
                [&](const std::string& name, Object* value) {
                    env->define(gc_new<a_string>(name), value);
                });
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
            return this->eval_impl(form, env_arg);
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
                eval_impl(first(current), env);  // Evaluate for side effect
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
        Object* val = eval_impl(second(args), env);
        return eval_result::make_value(env->set(first(args), val));
    }
    else if (fn_val == "if") {
        // Evaluate condition
        Object* cond = eval_impl(first(args), env);

        // Choose branch based on condition (tail call!)
        Object* branch = truth(cond) ? second(args) : third(args);
        return eval_result::make_tailcall(branch, env);
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
    // NOTE: `delay` / `delay-force` are desugared through the shared Expander
    // (see kExpanderForms above) to %make-promise, so the tree-walk uses the same
    // memoizing promise representation as every other evaluator. No native form.
    else if (fn_val == "Macro") {
        Macro* m = gc_new<Macro>(first(args), rest(args), env);
        return eval_result::make_value(m);
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
        Object* proc = eval_impl(fn, env);
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

} } // namespace zombie::runtime
