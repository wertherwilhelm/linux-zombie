/**
 * @file Lisp.h
 * @brief Traditional tree-walking Scheme interpreter with tail-call optimization
 * @author chrislk@gmail.com
 * @date 2024-06
 *
 * Implements the classic recursive Scheme interpreter with support for R5RS
 * special forms, macros, and tail-call optimization (TCO). This is the default
 * evaluator used by scheme_engine and the most mature implementation.
 *
 * **Evaluation Strategy:**
 * The Lisp class uses a hybrid evaluation model:
 * - **Recursive evaluation** (new_eval): Simple, direct recursion for most expressions
 * - **Trampolined evaluation** (eval_step): Returns eval_result for TCO in tail position
 *
 * **Special Forms Handled:**
 * - quote, if, cond, case, and, or, begin
 * - lambda, define, set!
 * - let, let*, letrec
 * - do (transformed to letrec for efficiency)
 * - quasiquote/unquote
 * - define-syntax (R5RS macros)
 *
 * **Tail-Call Optimization:**
 * Uses trampolining to avoid C++ stack overflow on deep recursion. Tail calls
 * return eval_result instead of Object*, allowing caller to iterate instead of recurse.
 *
 * @see IEvaluator for evaluator interface
 * @see scheme_engine for high-level interpreter API
 * @see eval_result for TCO return type
 */
#pragma once

#include "Environment.h"
#include "primitive.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "Base.h"
#include "i_evaluator.h"
#include "eval_result.h"

namespace zombie { namespace runtime {

/**
 * @brief Traditional tree-walking Scheme interpreter
 *
 * This is the main evaluator implementation - a classic recursive interpreter
 * that walks the Scheme AST and evaluates it according to R5RS semantics.
 *
 * **Architecture:**
 * - Implements IEvaluator interface (pluggable into scheme_engine)
 * - Uses recursive descent with special form detection
 * - Supports tail-call optimization via trampolining
 * - Handles Macro expansion during evaluation
 *
 * **Evaluation Flow:**
 * 1. **Atoms** (numbers, strings, symbols, booleans):
 *    - Self-evaluating types return themselves
 *    - Symbols look up value in environment
 *    - Handled by eval_atom()
 *
 * 2. **Lists** (function calls and special forms):
 *    - First element determines behavior:
 *      - **Special form** (quote, if, lambda, etc.): Handle specially
 *      - **Macro**: Expand and re-evaluate
 *      - **Function**: Evaluate args and apply
 *    - Handled by eval_list()
 *
 * **Tail-Call Optimization (TCO):**
 * To avoid C++ stack overflow on deep recursion (e.g., tail-recursive factorial
 * with n=10000), the evaluator uses trampolining for tail calls:
 * - eval_step() returns eval_result (either value or Continuation)
 * - Caller loops until final value is produced
 * - Only tail position calls use trampolining (non-tail use new_eval)
 *
 * **Special Form Handlers:**
 * - **quote**: Return argument unevaluated
 * - **if**: Evaluate condition, then one branch
 * - **lambda**: Create Closure capturing current environment
 * - **define**: Create or update binding in current environment
 * - **set!**: Mutate existing binding (searches parent chain)
 * - **let/let * /letrec**: Create extended environment with bindings
 * - **begin**: Evaluate sequence, return last value
 * - **cond**: Multi-way conditional (transformed to nested ifs)
 * - **case**: Pattern matching (transformed to cond)
 * - **and/or**: Short-circuit boolean operators
 * - **quasiquote**: Template expansion with unquote/unquote-splicing
 * - **do**: Iteration (transformed to letrec for efficiency)
 *
 * **Usage Example:**
 * @code
 * Lisp evaluator;
 * Environment* env = evaluator.get_environment();
 *
 * // Evaluate simple expression
 * Object* result = evaluator.new_eval(read_string("(+ 1 2)"), env);  // Returns 3
 *
 * // Define function
 * evaluator.new_eval(read_string("(define (factorial n) "
 *                                 "(if (= n 0) 1 (* n (factorial (- n 1)))))"), env);
 *
 * // Call function (TCO handles deep recursion)
 * Object* fac = evaluator.new_eval(read_string("(factorial 10000)"), env);
 * @endcode
 *
 * @see IEvaluator for interface contract
 * @see eval_result for TCO mechanism
 * @see scheme_engine for high-level wrapper
 */
class Lisp : public Base, public IEvaluator
{
public:
    /**
     * @brief Construct Lisp evaluator with new global environment
     * @note Automatically creates global environment
     * @note Does NOT install primitives (caller must do this)
     */
    Lisp();

    /**
     * @brief Destructor
     * @note Does not delete global environment (managed by GC)
     */
    virtual ~Lisp();

    // ========================================================================
    // IEvaluator Interface Implementation
    // ========================================================================

    /**
     * @brief Evaluate expression in given environment (IEvaluator interface)
     * @param expr Scheme expression to evaluate (atom or list)
     * @param env Environment for variable lookup
     * @return Result of evaluation
     * @note Delegates to new_eval() for actual evaluation
     * @note This is the main entry point when used as IEvaluator
     */
    Object* eval(Object* expr, Environment* env) override {
        return new_eval(expr, env);
    }

    /**
     * @brief Get global environment (IEvaluator interface)
     * @return Pointer to global environment
     */
    Environment* get_environment() override {
        return m_global_env;
    }

    // ========================================================================
    // Main Evaluation Methods (Recursive Strategy)
    // ========================================================================

    /**
     * @brief Evaluate expression in global environment
     * @param x Expression to evaluate
     * @param treat_as_single_arg If true, don't evaluate as function call (internal flag)
     * @return Result of evaluation
     * @note Uses global environment (m_global_env)
     * @note Convenience wrapper for new_eval(x, m_global_env)
     */
    Object* new_eval(Object* x, bool treat_as_single_arg = false);

    /**
     * @brief Evaluate expression in specified environment (main evaluator)
     * @param x Expression to evaluate
     * @param env Environment for variable lookup
     * @param treat_as_single_arg If true, don't evaluate as function call (internal flag)
     * @return Result of evaluation
     * @note This is the main recursive evaluator
     * @note Dispatches to eval_atom() or eval_list() based on expression type
     *
     * Algorithm:
     * 1. If atom: delegate to eval_atom()
     * 2. If list: delegate to eval_list()
     * 3. Return result
     */
    Object* new_eval(Object* x, Environment* env, bool treat_as_single_arg = false);

    /**
     * @brief Evaluate atom (self-evaluating or variable lookup)
     * @param x Atom to evaluate (number, string, symbol, boolean, etc.)
     * @param env Environment for variable lookup
     * @return Value of atom
     * @note Self-evaluating types (numbers, strings, booleans) return themselves
     * @note Symbols look up value in environment (searches parent chain)
     * @note Returns nil if symbol not found
     */
    Object* eval_atom(Object* x, Environment* env);

    /**
     * @brief Evaluate list (special form or function application)
     * @param x List expression to evaluate
     * @param env Environment for evaluation
     * @return Result of evaluation
     * @note First element determines behavior:
     *   - Special form (quote, if, lambda, etc.): Handle specially
     *   - Macro: Expand and re-evaluate
     *   - Function: Evaluate args and apply
     *
     * Special forms handled:
     * - quote, if, cond, case, and, or, begin
     * - lambda, define, set!
     * - let, let*, letrec
     * - do, quasiquote, define-syntax
     */
    Object* eval_list(Object* x, Environment* env);

    // ========================================================================
    // Tail-Call Optimization (TCO) Methods
    // ========================================================================

    /**
     * @brief Evaluate expression with TCO (trampolined evaluation)
     * @param x Expression to evaluate
     * @param env Environment for evaluation
     * @param treat_as_single_arg If true, don't evaluate as function call
     * @return eval_result (either final value or Continuation)
     * @note Returns eval_result instead of Object* to enable trampolining
     * @note Caller must loop until result.is_value() is true
     * @note Used in tail position to avoid C++ stack growth
     *
     * Example trampolining loop:
     * @code
     * eval_result r = evaluator.eval_step(expr, env);
     * while (r.is_continuation()) {
     *     r = evaluator.eval_step(r.get_continuation(), r.get_env());
     * }
     * Object* final_value = r.get_value();
     * @endcode
     */
    eval_result eval_step(Object* x, Environment* env, bool treat_as_single_arg = false);

    /**
     * @brief Evaluate list with TCO (trampolined list evaluation)
     * @param x List expression to evaluate
     * @param env Environment for evaluation
     * @return eval_result (either final value or Continuation)
     * @note Like eval_list but returns eval_result for trampolining
     * @note Used when list is in tail position
     */
    eval_result eval_step_list(Object* x, Environment* env);

    /**
     * @brief Evaluate expression to value without new stack frame
     * @param x Expression to evaluate
     * @param env Environment for evaluation
     * @return Final value (after trampolining if needed)
     * @note Uses local trampolining loop instead of recursion
     * @note Useful for evaluating non-tail expressions without stack growth
     * @note Internally calls eval_step() in a loop until value is produced
     */
    Object* eval_to_value(Object* x, Environment* env);

    // ========================================================================
    // Legacy and Utility Methods
    // ========================================================================

    /**
     * @brief Evaluate list of expressions (legacy method)
     * @param List List of expressions to evaluate
     * @param env Environment for evaluation
     * @return List of results (ConsCell)
     * @note Evaluates each element and returns list of values
     * @note Used by apply and other primitives
     * @deprecated Prefer using new_eval() directly
     */
    ConsCell* eval_args(Object *List, Environment* env);

    /**
     * @brief Set global environment (legacy mutator)
     * @param env New global environment
     * @note Rarely needed; environment set in constructor
     */
    void set_environment(Environment* env){ m_global_env = env;}

    /**
     * @brief Evaluate cond clauses (R5RS cond special form)
     * @param clauses List of cond clauses ((test expr...) ...)
     * @param env Environment for evaluation
     * @return Result of first clause whose test succeeds
     * @note Internal helper for cond special form
     * @note Evaluates test expressions until one is true
     * @note Returns result of corresponding body expressions
     */
    Object * reduce_cond(Object *clauses, Environment* env);

private:
    // ========================================================================
    // Private Members
    // ========================================================================

    OutputPort* m_output_port;    ///< Output port for (display ...), (write ...), etc.
    InputPort* m_input_port;      ///< Input port for (read), (read-char), etc.
    Environment* m_environment;    ///< Legacy environment (typically same as m_global_env)
    Environment* m_global_env;     ///< Global environment (top-level bindings)
    Procedure* m_proc;            ///< Cached Procedure (legacy, may be unused)
    std::string m_editor_command; ///< External editor command (for interactive editing)

    // ========================================================================
    // Private Helper Methods
    // ========================================================================

    /**
     * @brief Transform do loop to letrec (optimization)
     * @param args Arguments to do special form
     * @return Transformed expression (letrec form)
     * @note Reduces stack overhead by converting do to letrec + named let
     * @note Internal optimization for do loops
     *
     * Transforms:
     * @code
     * (do ((var init step) ...) (test expr ...) body ...)
     * @endcode
     * to:
     * @code
     * (letrec ((loop (lambda (var ...) (if test (begin expr ...) (begin body ... (loop step ...)))))) (loop init ...))
     * @endcode
     */
    Object* transform_do_to_letrec(Object* args);

    /**
     * @brief Expand quasiquote template (recursive helper)
     * @param x Template expression to expand
     * @param env Environment for evaluation
     * @return Expanded expression
     * @note Handles unquote and unquote-splicing recursively
     * @note Used by quasiquote special form
     *
     * Example:
     * @code
     * `(a ,b ,@c)  =>  (append (list 'a b) c)
     * @endcode
     */
    Object* qq_expand(Object* x, Environment* env);

    /**
     * @brief Expand quasiquote list (recursive helper)
     * @param x List template to expand
     * @param env Environment for evaluation
     * @return Expanded list expression
     * @note Handles unquote-splicing (,@) in list context
     * @note Called by qq_expand() for list templates
     */
    Object* qq_expand_list(Object* x, Environment* env);

};

/**
 * @brief Global Lisp evaluator pointer (for Value Ledger system)
 * @note Used by Value Ledger primitives to access evaluator
 * @note Set by embedding code or scheme_engine
 * @note Not thread-safe (one global evaluator per process)
 */
extern Lisp* g_lisp;

} } // namespace zombie::runtime
