#include "pch.h"
#include "toy_vm.h"
#include "continuation_support.h"
#include "expander.h"
#include "primitive.h"
#include "closure.h"
#include "continuation.h"
#include "cons_cell.h"  // for cons operations
#include "gc_manager.h"  // for trace_references
#include "syntax_rules_transformer.h"  // for let-syntax/letrec-syntax

namespace toy {

// ============================================================================
// VM Constructor
// ============================================================================

VM::VM(Environment* env, bool debug, bool enable_continuations)
    : accumulator(nil)
    , global_env(env)
    , debug_mode(debug)
    , step_count(0)
    , cont_support(nullptr) {

    // Create Continuation support if enabled
    if (enable_continuations) {
        cont_support = std::make_unique<ContinuationSupport>(this);
    }
}

// Destructor must be defined here where ContinuationSupport is a complete type
VM::~VM() = default;

// ============================================================================
// eval() - IEvaluator interface implementation
// ============================================================================

Object* VM::eval(Object* expr, Environment* env) {
    // Clear any leftover state from previous evaluations (e.g., from exceptions)
    stack.clear();
    if (cont_support) {
        cont_support->clear_continuation_flag();
    }

    // Expand macros and desugar BEFORE evaluation
    // This transforms let, let*, letrec, case into core forms
    // and expands all macros (syntax-rules)
    Expander expander(env);
    Object* expanded = expander.expand(expr);

    stack.push_back(Frame(EVAL_ATOM, expanded, env));
    run();
    return accumulator;
}

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

void VM::trace_references(Tracer& tracer) {
    tracer.visit_field(accumulator);

    // Trace all frames (iterate by reference to update in-place)
    for (auto& frame : stack) {
        tracer.visit_field(frame.expr);
        tracer.visit_field(frame.proc);
        tracer.visit_field(frame.args);
        tracer.visit_field(frame.evaled_args);
        // Trace frame environment (need to handle cast)
        if (frame.env) {
            Object* env_obj = (Object*)frame.env;
            tracer.visit_field(env_obj);
            frame.env = (Environment*)env_obj;  // Update after potential move
        }
    }
}

// ============================================================================
// Main step() dispatcher - delegates to per-frame handlers
// ============================================================================

void VM::step() {
    step_count++;

    if (stack.empty()) {
        return;
    }

    Frame& frame = stack.back();

    if (debug_mode) {
        std::cout << "\n--- Step " << step_count << " ---" << std::endl;
        std::cout << "Frame: " << frame_name(frame.type) << std::endl;
        std::cout << "Expr: ";
        if (frame.expr) {
            std::cout << frame.expr;
        } else {
            std::cout << "null";
        }
        std::cout << std::endl;
    }

    // Dispatch to appropriate handler based on frame type
    switch (frame.type) {
        case EVAL_ATOM:   step_eval_atom(frame);   break;
        case EVAL_CALL:   step_eval_call(frame);   break;
        case EVAL_ARGS:   step_eval_args(frame);   break;
        case APPLY_PROC:  step_apply_proc(frame);  break;
        case EVAL_IF:     step_eval_if(frame);     break;
        case EVAL_AND:    step_eval_and(frame);    break;
        case EVAL_OR:     step_eval_or(frame);     break;
        case EVAL_BEGIN:  step_eval_begin(frame);  break;
        case EVAL_DEFINE: step_eval_define(frame); break;
        case EVAL_SET:    step_eval_set(frame);    break;
        case EVAL_CALLCC: step_eval_callcc(frame); break;
        case EVAL_COND:   step_eval_cond(frame);   break;
        case EVAL_DYNAMIC_WIND: step_eval_dynamic_wind(frame); break;
        case RETURN:      stack.pop_back();        break;
    }

    if (debug_mode && step_count % 5 == 0) {
        print_stack();
    }
}

// ============================================================================
// Per-Frame Handler Functions
// ============================================================================

void VM::step_eval_atom(Frame& frame) {
    // Is it an atom?
    if (frame.expr->is_atom()) {
        // Self-evaluating: numbers, strings, chars, nil, t
        if (::is_nil(frame.expr) || frame.expr == t ||
            is_double(frame.expr) || is_char(frame.expr)) {
            accumulator = frame.expr;
        } else if (is_string(frame.expr) || is_symbol(frame.expr)) {
            // Variable lookup for both strings and symbols
            try {
                accumulator = frame.env->lookup(frame.expr);
            } catch (const std::exception&) {
                // Not found, treat as literal
                accumulator = frame.expr;
            }
        } else {
            // Other atom types (bools, etc.) - self-evaluating
            accumulator = frame.expr;
        }

        if (debug_mode) {
            std::cout << "  -> Atom evaluates to: ";
            std::cout << accumulator;
            std::cout << std::endl;
        }

        stack.pop_back();
    } else {
        // It's a list - switch to EVAL_CALL
        frame.type = EVAL_CALL;
    }
}

void VM::step_eval_call(Frame& frame) {
    // Evaluate a function call: (fn arg1 arg2 ...)
    Object* fn_expr = first(frame.expr);
    Object* args = rest(frame.expr);

    // Try special form dispatch
    if (dispatch_special_form(frame, fn_expr, args)) {
        return;  // Handled as special form
    }

    // Normal function call: evaluate function, then args, then apply
    frame.type = EVAL_ARGS;
    frame.proc = nullptr;  // Will be filled in
    frame.args = frame.expr;  // The whole call expression
    frame.evaled_args = nil;

    // First, evaluate the function itself
    stack.push_back(Frame(EVAL_ATOM, fn_expr, frame.env));
}

void VM::step_eval_args(Frame& frame) {
    // We just got back a result in accumulator

    // If a Continuation was just invoked, treat accumulator as final argument value
    if (cont_support && cont_support->was_continuation_invoked()) {
        if (debug_mode) {
            std::cout << "  -> EVAL_ARGS: Continuation invoked, using accumulator as result" << std::endl;
        }
        cont_support->clear_continuation_flag();
        // Treat the accumulator as the evaluated argument and skip remaining arguments
        frame.evaled_args = append(frame.evaled_args, cons(accumulator, nil));
        frame.args = nil;  // No more arguments to evaluate
        frame.type = APPLY_PROC;  // Proceed to apply
        return;
    }

    if (frame.proc == nullptr) {
        // This is the function we just evaluated
        frame.proc = accumulator;
        frame.args = rest(frame.args);  // Move to actual arguments

        if (debug_mode) {
            std::cout << "  -> Function evaluated to: ";
            std::cout << frame.proc;
            std::cout << std::endl;
        }
    } else {
        // This is an argument we just evaluated
        // Wrap accumulator in a cons cell before appending
        frame.evaled_args = append(frame.evaled_args, cons(accumulator, nil));

        if (debug_mode) {
            std::cout << "  -> Arg evaluated to: ";
            std::cout << accumulator;
            std::cout << std::endl;
        }
    }

    // Are there more arguments?
    if (frame.args != nil && !frame.args->is_nil()) {
        Object* next_arg = first(frame.args);
        frame.args = rest(frame.args);
        // Push a frame to evaluate next argument
        stack.push_back(Frame(EVAL_ATOM, next_arg, frame.env));
    } else {
        // All arguments evaluated! Time to apply
        frame.type = APPLY_PROC;
    }
}

void VM::step_apply_proc(Frame& frame) {
    if (debug_mode) {
        std::cout << "  -> Applying proc to args: ";
        std::cout << frame.evaled_args;
        std::cout << std::endl;
    }

    // Check if it's a Continuation FIRST (before checking for Procedure/Primitive)
    // Continuations inherit from Procedure but have special handling
    if (cont_support && cont_support->try_invoke_continuation(frame.proc, frame.evaled_args)) {
        // Continuation was invoked successfully
        return;
    }

    // Check if it's a Closure
    Closure* clos = dynamic_cast<Closure*>(frame.proc);
    if (clos) {
        // Create new environment for the Closure
        Environment* new_env = clos->m_env->extend(clos->m_parms,
                                                  frame.evaled_args);

        if (debug_mode) {
            std::cout << "  -> Closure call with params: ";
            std::cout << clos->m_parms;
            std::cout << std::endl;
            std::cout << "     bound to: ";
            std::cout << frame.evaled_args;
            std::cout << std::endl;
        }

        // Replace current frame with evaluation of body
        // (This is tail call optimization!)
        stack.pop_back();
        stack.push_back(Frame(EVAL_ATOM, clos->m_body, new_env));
        return;
    }

    // Check if it's a Primitive (Procedure but not Closure or Continuation)
    Procedure* proc = dynamic_cast<Procedure*>(frame.proc);
    if (proc) {
        // It's a Primitive (either old Primitive class or new PrimitiveBase hierarchy)
        accumulator = proc->apply(this, frame.evaled_args, frame.env);
        if (debug_mode) {
            std::cout << "  -> Primitive returned: ";
            std::cout << accumulator;
            std::cout << std::endl;
        }
        stack.pop_back();
        return;
    }

    // Unknown Procedure type - just pop and continue
    // (This can happen if variable lookup returns a non-Procedure value)
    stack.pop_back();
}

void VM::step_eval_if(Frame& frame) {
    // We just evaluated the condition, result is in accumulator
    // frame.args contains (condition then-expr else-expr)

    // R5RS: only #f is false, everything else is true
    bool condition_is_true = truth(accumulator);

    if (debug_mode) {
        std::cout << "  -> IF condition evaluated to: ";
        std::cout << accumulator;
        std::cout << " (" << (condition_is_true ? "TRUE" : "FALSE") << ")" << std::endl;
    }

    Object* then_expr = first(rest(frame.args));
    Object* else_expr = first(rest(rest(frame.args)));

    // Choose which branch to evaluate (lazy evaluation!)
    Object* branch_to_eval = condition_is_true ? then_expr : else_expr;

    if (debug_mode) {
        std::cout << "  -> Taking " << (condition_is_true ? "THEN" : "ELSE")
                  << " branch: ";
        std::cout << branch_to_eval;
        std::cout << std::endl;
    }

    // Replace current frame with evaluation of the chosen branch
    stack.pop_back();
    stack.push_back(Frame(EVAL_ATOM, branch_to_eval, frame.env));
}

void VM::step_eval_and(Frame& frame) {
    // We just evaluated an expression, result is in accumulator
    // Short-circuit: if false, return false immediately
    // If true, continue to next expression

    // R5RS: only #f is false, everything else is true
    bool is_true = truth(accumulator);

    if (debug_mode) {
        std::cout << "  -> AND: evaluated to ";
        std::cout << accumulator;
        std::cout << " (" << (is_true ? "TRUE" : "FALSE") << ")" << std::endl;
    }

    if (!is_true) {
        // Short-circuit! Return false immediately
        if (debug_mode) {
            std::cout << "  -> AND short-circuits: returning FALSE" << std::endl;
        }
        // accumulator already has the false value
        stack.pop_back();
        return;
    }

    // Move to next expression
    frame.args = rest(frame.args);

    if (frame.args != nil && !frame.args->is_nil()) {
        // More expressions to evaluate
        Object* next_expr = first(frame.args);
        stack.push_back(Frame(EVAL_ATOM, next_expr, frame.env));
    } else {
        // No more expressions - return last value (which is true)
        if (debug_mode) {
            std::cout << "  -> AND complete: all true, returning last value" << std::endl;
        }
        // accumulator already has the last value
        stack.pop_back();
    }
}

void VM::step_eval_or(Frame& frame) {
    // We just evaluated an expression, result is in accumulator
    // Short-circuit: if true, return true immediately
    // If false, continue to next expression

    // R5RS: only #f is false, everything else is true
    bool is_true = truth(accumulator);

    if (debug_mode) {
        std::cout << "  -> OR: evaluated to ";
        std::cout << accumulator;
        std::cout << " (" << (is_true ? "TRUE" : "FALSE") << ")" << std::endl;
    }

    if (is_true) {
        // Short-circuit! Return true immediately
        if (debug_mode) {
            std::cout << "  -> OR short-circuits: returning TRUE" << std::endl;
        }
        // accumulator already has the true value
        stack.pop_back();
        return;
    }

    // Move to next expression
    frame.args = rest(frame.args);

    if (frame.args != nil && !frame.args->is_nil()) {
        // More expressions to evaluate
        Object* next_expr = first(frame.args);
        stack.push_back(Frame(EVAL_ATOM, next_expr, frame.env));
    } else {
        // No more expressions - return last value (which is false)
        if (debug_mode) {
            std::cout << "  -> OR complete: all false, returning last value" << std::endl;
        }
        // accumulator already has the last value (nil)
        stack.pop_back();
    }
}

void VM::step_eval_begin(Frame& frame) {
    // We just evaluated an expression, result is in accumulator
    // Move to next expression, or return if done

    if (debug_mode) {
        std::cout << "  -> BEGIN: evaluated to ";
        std::cout << accumulator;
        std::cout << std::endl;
    }

    // If a Continuation was just invoked, don't continue with remaining expressions
    if (cont_support && cont_support->was_continuation_invoked()) {
        if (debug_mode) {
            std::cout << "  -> BEGIN: Continuation invoked, stopping sequence" << std::endl;
        }
        cont_support->clear_continuation_flag();
        stack.pop_back();
        return;
    }

    // Move to next expression
    frame.args = rest(frame.args);

    if (frame.args != nil && !frame.args->is_nil()) {
        // More expressions to evaluate
        Object* next_expr = first(frame.args);
        stack.push_back(Frame(EVAL_ATOM, next_expr, frame.env));
    } else {
        // No more expressions - return last value
        if (debug_mode) {
            std::cout << "  -> BEGIN complete: returning last value" << std::endl;
        }
        // accumulator already has the last value
        stack.pop_back();
    }
}

void VM::step_eval_define(Frame& frame) {
    // We just evaluated the value expression
    // Bind it to the variable name (stored in frame.proc)
    Object* var_name = frame.proc;

    if (debug_mode) {
        std::cout << "  -> DEFINE: binding ";
        std::cout << var_name;
        std::cout << " to ";
        std::cout << accumulator;
        std::cout << std::endl;
    }

    // Define the variable in the current environment
    frame.env->define(var_name, accumulator);

    // define returns the value
    // accumulator already has it
    stack.pop_back();
}

void VM::step_eval_set(Frame& frame) {
    // We just evaluated the value expression
    // Update the variable's binding (stored in frame.proc)
    Object* var_name = frame.proc;

    if (debug_mode) {
        std::cout << "  -> SET!: updating ";
        std::cout << var_name;
        std::cout << " to ";
        std::cout << accumulator;
        std::cout << std::endl;
    }

    // Update the variable in the environment
    // set! searches up the environment chain to find and update the binding
    frame.env->set(var_name, accumulator);

    // set! returns the value
    // accumulator already has it
    stack.pop_back();
}

void VM::step_eval_callcc(Frame& frame) {
    // We just evaluated the function to call with the Continuation
    // The function is in accumulator
    // Now we need to capture the Continuation and call the function with it

    if (!cont_support) {
        throw std::runtime_error("call/cc: Continuation support not enabled");
    }

    if (debug_mode) {
        std::cout << "  -> CALL/CC: function evaluated to ";
        std::cout << accumulator;
        std::cout << std::endl;
        std::cout << "     Creating Continuation..." << std::endl;
    }

    // Capture the current Continuation (stack below this frame)
    Object* cont = cont_support->capture_continuation(frame.env);

    if (debug_mode) {
        std::cout << "     Calling function with Continuation..." << std::endl;
    }

    // Now call the function with the Continuation as its argument
    // Replace this frame with APPLY_PROC
    frame.type = APPLY_PROC;
    frame.proc = accumulator;  // The function to call
    frame.evaled_args = cons(cont, nil);  // The Continuation as argument

    // Don't push a new frame - just transition this frame to APPLY_PROC
    // The step() function will handle it on the next iteration
}

void VM::step_eval_cond(Frame& frame) {
    // We just evaluated a test, result is in accumulator
    // Check if the test is true (not nil and not false)

    if (debug_mode) {
        std::cout << "  -> COND: test evaluated to ";
        std::cout << accumulator;
        std::cout << std::endl;
    }

    // R5RS: only #f is false, everything else is true
    if (truth(accumulator)) {
        // Test succeeded! Evaluate the result expression from this clause
        Object* current_clause = first(frame.args);
        Object* result_expr = first(rest(current_clause));

        if (debug_mode) {
            std::cout << "     Test passed! Evaluating result expression" << std::endl;
        }

        // Replace this frame with evaluation of the result
        stack.pop_back();
        stack.push_back(Frame(EVAL_ATOM, result_expr, frame.env));
    } else {
        // Test failed - try the next clause
        frame.args = rest(frame.args);

        if (debug_mode) {
            std::cout << "     Test failed, trying next clause" << std::endl;
        }

        if (frame.args != nil && !frame.args->is_nil()) {
            // More clauses to try
            Object* next_clause = first(frame.args);
            Object* next_test = first(next_clause);

            // Check for 'else' clause
            if (is_string(next_test) && ((a_string*)next_test)->value() == "else") {
                // Else clause - always succeeds
                if (debug_mode) {
                    std::cout << "     Found 'else' clause" << std::endl;
                }
                Object* else_result = first(rest(next_clause));
                stack.pop_back();
                stack.push_back(Frame(EVAL_ATOM, else_result, frame.env));
            } else {
                // Evaluate the next test
                stack.push_back(Frame(EVAL_ATOM, next_test, frame.env));
            }
        } else {
            // No more clauses - return nil
            if (debug_mode) {
                std::cout << "     No more clauses, returning nil" << std::endl;
            }
            accumulator = nil;
            stack.pop_back();
        }
    }
}

// ============================================================================
// Special Form Dispatch
// ============================================================================

bool VM::dispatch_special_form(Frame& frame, Object* fn_expr, Object* args) {
    std::string name = get_identifier_name(fn_expr);

    // Core language
    if (name == "quote")  { sf_quote(frame, args); return true; }
    if (name == "lambda") { sf_lambda(frame, args); return true; }
    if (name == "if")     { sf_if(frame, args); return true; }
    if (name == "begin")  { sf_begin(frame, args); return true; }
    if (name == "define") { sf_define(frame, args); return true; }
    if (name == "set!")   { sf_set(frame, args); return true; }

    // Short-circuit evaluation
    if (name == "and")    { sf_and(frame, args); return true; }
    if (name == "or")     { sf_or(frame, args); return true; }

    // Conditionals
    if (name == "cond")   { sf_cond(frame, args); return true; }

    // NOTE: Desugaring forms (let, let*, letrec, case) and Macro forms
    // (let-syntax, letrec-syntax) are now handled by the Expander module
    // before evaluation. They no longer appear in VM dispatch.

    // Dynamic-wind (requires Continuation support)
    if (name == "dynamic-wind") {
        if (!cont_support) {
            throw std::runtime_error("dynamic-wind: Continuation support not enabled");
        }
        sf_dynamic_wind(frame, args);
        return true;
    }

    // Continuation support (optional)
    if (cont_support) {
        if (name == "call/cc" || name == "call-with-current-Continuation") {
            cont_support->handle_callcc(frame, args);
            return true;
        }
    }

    return false;  // Not a special form
}

// ============================================================================
// Individual Special Form Handlers - Core Language
// ============================================================================

void VM::sf_quote(Frame& frame, Object* args) {
    accumulator = first(args);
    if (debug_mode) {
        std::cout << "  -> Quote returns: " << accumulator << std::endl;
    }
    stack.pop_back();
}

void VM::sf_lambda(Frame& frame, Object* args) {
    Object* params = first(args);
    Object* body = rest(args);
    accumulator = gc_new<Closure>(params, body, frame.env);
    if (debug_mode) {
        std::cout << "  -> Lambda created" << std::endl;
    }
    stack.pop_back();
}

void VM::sf_if(Frame& frame, Object* args) {
    Object* condition = first(args);

    frame.type = EVAL_IF;
    frame.args = args;  // Save the full if expression args

    if (debug_mode) {
        std::cout << "  -> IF form: evaluating condition" << std::endl;
    }

    // Push frame to evaluate the condition
    stack.push_back(Frame(EVAL_ATOM, condition, frame.env));
}

void VM::sf_begin(Frame& frame, Object* args) {
    frame.type = EVAL_BEGIN;
    frame.args = args;  // The expressions to evaluate

    if (debug_mode) {
        std::cout << "  -> BEGIN form: evaluating sequence" << std::endl;
    }

    // Start by evaluating first expression
    if (args != nil && !args->is_nil()) {
        Object* first_expr = first(args);
        stack.push_back(Frame(EVAL_ATOM, first_expr, frame.env));
    } else {
        // (begin) with no arguments returns nil
        accumulator = nil;
        stack.pop_back();
    }
}

void VM::sf_define(Frame& frame, Object* args) {
    // Two forms:
    // 1. (define var expr)
    // 2. (define (name arg1 arg2) body...) - shorthand for (define name (lambda (arg1 arg2) body...))

    Object* first_arg = first(args);

    // Check if it's the shorthand function definition form
    if (first_arg->pair() && !first_arg->is_nil()) {
        // Shorthand form: (define (name arg1 arg2) body...)
        Object* name = first(first_arg);      // Function name
        Object* params = rest(first_arg);     // Parameters
        Object* body = rest(args);            // Body expressions

        if (debug_mode) {
            std::cout << "  -> DEFINE shorthand: defining function " << name << std::endl;
            std::cout << "     Params: " << params << std::endl;
            std::cout << "     Body before Closure: " << body << std::endl;
        }

        // Transform to: (define name (lambda params body...))
        // Create the lambda directly
        Object* lambda_expr = gc_new<Closure>(params, body, frame.env);

        // Protect from GC while we inspect and define
        gc_root<Object> lambda_guard(lambda_expr);

        if (debug_mode) {
            Closure* clos = dynamic_cast<Closure*>(lambda_expr);
            if (clos) {
                std::cout << "     Closure m_body: " << clos->m_body << std::endl;
            }
        }

        // Define it in the environment
        frame.env->define(name, lambda_expr);
        accumulator = lambda_expr;

        stack.pop_back();
    } else {
        // Simple form: (define var expr)
        Object* var_name = first_arg;
        Object* value_expr = first(rest(args));

        frame.type = EVAL_DEFINE;
        frame.proc = var_name;  // Store var name in proc field

        if (debug_mode) {
            std::cout << "  -> DEFINE form: binding " << var_name << std::endl;
        }

        // Evaluate the value expression
        stack.push_back(Frame(EVAL_ATOM, value_expr, frame.env));
    }
}

void VM::sf_set(Frame& frame, Object* args) {
    // (set! var expr)
    // Evaluate expr, update var's binding
    Object* var_name = first(args);
    Object* value_expr = first(rest(args));

    frame.type = EVAL_SET;
    frame.proc = var_name;  // Store var name in proc field

    if (debug_mode) {
        std::cout << "  -> SET! form: updating " << var_name << std::endl;
    }

    // Evaluate the value expression
    stack.push_back(Frame(EVAL_ATOM, value_expr, frame.env));
}

// ============================================================================
// Individual Special Form Handlers - Short-Circuit Evaluation
// ============================================================================

void VM::sf_and(Frame& frame, Object* args) {
    // (and expr1 expr2 ...)
    // Evaluate left to right, stop at first false
    frame.type = EVAL_AND;
    frame.args = args;  // The expressions to AND together

    if (debug_mode) {
        std::cout << "  -> AND form: short-circuit evaluation" << std::endl;
    }

    // Start by evaluating the first expression
    if (args != nil && !args->is_nil()) {
        Object* first_expr = first(args);
        stack.push_back(Frame(EVAL_ATOM, first_expr, frame.env));
    } else {
        // (and) with no arguments returns t
        accumulator = t;
        stack.pop_back();
    }
}

void VM::sf_or(Frame& frame, Object* args) {
    // (or expr1 expr2 ...)
    // Evaluate left to right, stop at first true
    frame.type = EVAL_OR;
    frame.args = args;  // The expressions to OR together

    if (debug_mode) {
        std::cout << "  -> OR form: short-circuit evaluation" << std::endl;
    }

    // Start by evaluating the first expression
    if (args != nil && !args->is_nil()) {
        Object* first_expr = first(args);
        stack.push_back(Frame(EVAL_ATOM, first_expr, frame.env));
    } else {
        // (or) with no arguments returns #f
        accumulator = false_obj;
        stack.pop_back();
    }
}

// ============================================================================
// Individual Special Form Handlers - Conditionals
// ============================================================================

void VM::sf_cond(Frame& frame, Object* args) {
    // (cond (test1 result1) (test2 result2) ... (else default))
    // Evaluate tests in order until one is true, then return its result

    frame.type = EVAL_COND;
    frame.args = args;  // List of clauses

    if (debug_mode) {
        std::cout << "  -> COND form: multi-way conditional" << std::endl;
    }

    // Start by evaluating the test of the first clause
    if (args != nil && !args->is_nil()) {
        Object* first_clause = first(args);
        Object* test_expr = first(first_clause);

        // Check for 'else' clause (always true)
        if (is_keyword(test_expr, "else")) {
            // Else clause - just evaluate the result
            Object* result_expr = first(rest(first_clause));
            stack.pop_back();
            stack.push_back(Frame(EVAL_ATOM, result_expr, frame.env));
        } else {
            // Normal clause - evaluate the test
            stack.push_back(Frame(EVAL_ATOM, test_expr, frame.env));
        }
    } else {
        // Empty cond returns nil
        accumulator = nil;
        stack.pop_back();
    }
}

void VM::sf_dynamic_wind(Frame& frame, Object* args) {
    // (dynamic-wind before thunk after)
    // Call before, then thunk (with before/after on the wind stack), then after
    // Ensures after is called even if Continuation escapes

    if (debug_mode) {
        std::cout << "  -> DYNAMIC-WIND form" << std::endl;
    }

    // Set up EVAL_DYNAMIC_WIND frame to manage the evaluation
    frame.type = EVAL_DYNAMIC_WIND;
    frame.args = args;  // Save the args (before, thunk, after)
    frame.proc = nullptr;        // Clear - used to track if before_proc is saved
    frame.evaled_args = nullptr; // Clear - used to track if thunk_proc is saved

    // Start by evaluating the before thunk
    Object* before_expr = first(args);
    stack.push_back(Frame(EVAL_ATOM, before_expr, frame.env));
}

// ============================================================================
// step_eval_dynamic_wind - Handle dynamic-wind evaluation
// ============================================================================

void VM::step_eval_dynamic_wind(Frame& frame) {
    // We're in a multi-step process:
    // Step 1: Evaluate before thunk (accumulator has the before Procedure)
    // Step 2: Evaluate thunk expr (accumulator has the thunk Procedure)
    // Step 3: Evaluate after thunk (accumulator has the after Procedure)
    // Step 4: Enter wind, call thunk, exit wind (when thunk returns)

    // Check if the thunk has completed (marker check)
    if (debug_mode) {
        std::cout << "  -> DYNAMIC-WIND: checking marker at frame addr=" << (void*)&frame
                  << " frame.expr=" << (frame.expr == t ? "#t" : "other") << std::endl;
    }
    if (frame.expr == t) {
        // The thunk has completed, we need to exit the wind
        if (debug_mode) {
            std::cout << "  -> DYNAMIC-WIND: thunk completed, calling exit_wind" << std::endl;
        }

        cont_support->exit_wind();

        // The accumulator has the result from the thunk, keep it
        // Pop this frame
        stack.pop_back();
        return;
    }

    if (debug_mode) {
        std::cout << "  -> DYNAMIC-WIND state: proc=" << (frame.proc ? "set" : "null")
                  << " evaled_args=" << (frame.evaled_args ? "set" : "null") << std::endl;
    }

    if (!frame.proc) {
        // Step 1: We just evaluated the before expr, now we have the before Procedure
        frame.proc = accumulator;  // Save before Procedure

        if (debug_mode) {
            std::cout << "  -> DYNAMIC-WIND: before evaluated, now evaluating thunk" << std::endl;
        }

        // Now evaluate the thunk expression
        Object* thunk_expr = first(rest(frame.args));
        stack.push_back(Frame(EVAL_ATOM, thunk_expr, frame.env));
        return;
    }

    if (!frame.evaled_args) {
        // Step 2: We just evaluated the thunk expr, now we have the thunk Procedure
        frame.evaled_args = accumulator;  // Save thunk Procedure

        if (debug_mode) {
            std::cout << "  -> DYNAMIC-WIND: thunk evaluated, now evaluating after" << std::endl;
        }

        // Now evaluate the after expression
        Object* after_expr = first(rest(rest(frame.args)));
        stack.push_back(Frame(EVAL_ATOM, after_expr, frame.env));
        return;
    }

    // Step 3: We have all three procedures
    Object* before_proc = frame.proc;
    Object* thunk_proc = frame.evaled_args;
    Object* after_proc = accumulator;

    if (debug_mode) {
        std::cout << "  -> DYNAMIC-WIND: all procedures evaluated, executing" << std::endl;
        std::cout << "     before_proc type: " << (before_proc ? before_proc->get_type() : -1) << std::endl;
        std::cout << "     thunk_proc type: " << (thunk_proc ? thunk_proc->get_type() : -1) << std::endl;
        std::cout << "     after_proc type: " << (after_proc ? after_proc->get_type() : -1) << std::endl;
    }

    // Now we need to:
    // 1. Call the thunk
    // 2. When it returns, call exit_wind
    //
    // Strategy: We'll keep this frame around with a marker that we've entered the wind
    // When the thunk returns, we check this marker and call exit_wind

    // Mark that we've entered the wind (use expr field as a marker - hacky but works)
    // IMPORTANT: Must set the marker BEFORE enter_wind which may cause reallocation
    // and BEFORE any other operations that might invalidate the frame reference
    frame.expr = t;  // Non-null marker means "wind has been entered"

    // Save values we need before any potential reallocation
    size_t frame_index = stack.size() - 1;  // Remember our position
    Environment* saved_env = frame.env;

    if (debug_mode) {
        std::cout << "  -> DYNAMIC-WIND: marker set, frame_index=" << frame_index << std::endl;
    }

    // Enter the dynamic-wind (calls before thunk and pushes wind frame)
    // NOTE: enter_wind calls call_thunk which may cause stack reallocation!
    cont_support->enter_wind(before_proc, after_proc);

    // After enter_wind, the frame reference may be invalid due to reallocation.
    // Access the frame through the stack index instead.
    if (debug_mode) {
        std::cout << "  -> DYNAMIC-WIND: after enter_wind, checking marker at index " << frame_index
                  << " = " << (stack[frame_index].expr == t ? "#t" : "other") << std::endl;
    }

    // Push a frame to call the thunk
    stack.push_back(Frame(APPLY_PROC, nullptr, saved_env));
    stack.back().proc = thunk_proc;
    stack.back().evaled_args = nil;
}

// NOTE: Desugaring special form handlers (sf_let, sf_let_star, sf_letrec, sf_case)
// and Macro form handlers (sf_let_syntax, sf_letrec_syntax) have been moved to the
// Expander module in expander.cpp. They are no longer part of the VM.

} // namespace toy
