#pragma once

#include "Object.h"
#include "Environment.h"
#include "i_evaluator.h"
#include "closure.h"
#include "expander.h"
#include "Base.h"
#include "atom.h"
#include "procedure.h"
#include <vector>
#include <memory>
#include <iostream>

/**
 * @file toy_vm.h
 * @brief Experimental stack-based VM with Continuation support.
 */

// Forward declare GC manager
class gc_manager;

// A dadaist VM experiment - making the invisible visible

namespace toy {

// Bring types from zombie::runtime into this namespace
using zombie::runtime::Object;
using zombie::runtime::Environment;
using zombie::runtime::IEvaluator;
using zombie::runtime::nil;
using zombie::runtime::t;
using zombie::runtime::is_nil;
using zombie::runtime::is_string;
using zombie::runtime::is_symbol;
using zombie::runtime::a_string;
using zombie::runtime::a_symbol;
using zombie::runtime::Closure;
using zombie::runtime::Procedure;
using zombie::runtime::Expander;
using zombie::runtime::car;
using zombie::runtime::cdr;
using zombie::runtime::first;
using zombie::runtime::rest;
using zombie::runtime::cons;

// Forward declare ContinuationSupport
class ContinuationSupport;

// What kind of frame are we in?
enum FrameType {
    EVAL_ATOM,          // Evaluating an atom (variable lookup)
    EVAL_CALL,          // Evaluating a function call
    APPLY_PROC,         // Applying a Procedure
    EVAL_ARGS,          // Evaluating arguments one by one
    EVAL_IF,            // Evaluating IF special form (lazy branches)
    EVAL_AND,           // Evaluating AND special form (short-circuit)
    EVAL_OR,            // Evaluating OR special form (short-circuit)
    EVAL_BEGIN,         // Evaluating BEGIN (sequence of expressions)
    EVAL_DEFINE,        // Evaluating DEFINE (bind variable)
    EVAL_SET,           // Evaluating SET! (mutate variable)
    EVAL_CALLCC,        // Evaluating CALL/CC (capture Continuation)
    EVAL_COND,          // Evaluating COND (multi-way conditional)
    EVAL_DYNAMIC_WIND,  // Evaluating DYNAMIC-WIND (before/thunk/after)
    RETURN              // Returning a value
};

inline const char* frame_name(FrameType t) {
    switch(t) {
        case EVAL_ATOM: return "EVAL_ATOM";
        case EVAL_CALL: return "EVAL_CALL";
        case APPLY_PROC: return "APPLY_PROC";
        case EVAL_ARGS: return "EVAL_ARGS";
        case EVAL_IF: return "EVAL_IF";
        case EVAL_AND: return "EVAL_AND";
        case EVAL_OR: return "EVAL_OR";
        case EVAL_BEGIN: return "EVAL_BEGIN";
        case EVAL_DEFINE: return "EVAL_DEFINE";
        case EVAL_SET: return "EVAL_SET";
        case EVAL_CALLCC: return "EVAL_CALLCC";
        case EVAL_COND: return "EVAL_COND";
        case EVAL_DYNAMIC_WIND: return "EVAL_DYNAMIC_WIND";
        case RETURN: return "RETURN";
        default: return "???";
    }
}

// A frame is "what we're doing right now"
struct Frame {
    FrameType type;
    Object* expr;           // What expression are we working on?
    Environment* env;        // What environment are we in?

    // Working state for multi-step operations
    Object* proc;           // For APPLY_PROC: the Procedure
    Object* args;           // For EVAL_ARGS: remaining args to evaluate
    Object* evaled_args;    // For EVAL_ARGS: args we've already evaluated

    Frame(FrameType t, Object* e, Environment* env_)
        : type(t), expr(e), env(env_), proc(nullptr), args(nullptr), evaled_args(nil) {}
};

/**
 * @brief Experimental stack-based VM with call/cc and dynamic-wind support.
 */
class VM : public IEvaluator {
public:
    std::vector<Frame> stack;
    Object* accumulator;    // The "result" register
    Environment* global_env;
    bool debug_mode;
    int step_count;

    // Continuation support (optional module)
    std::unique_ptr<ContinuationSupport> cont_support;

    VM(Environment* env, bool debug = false, bool enable_continuations = true);
    ~VM();  // Defined in .cpp where ContinuationSupport is complete

    // VM traces its references for GC (implemented in .cpp)
    void trace_references(Tracer& tracer);

    // IEvaluator interface implementation
    Object* eval(Object* expr, Environment* env) override;

    Environment* get_environment() override {
        return global_env;
    }

    // Run until we're done
    void run() {
        while (!stack.empty()) {
            step();
        }
    }

    // Execute one step
    void step();

    // Per-frame handler functions (extracted from step())
    void step_eval_atom(Frame& frame);
    void step_eval_call(Frame& frame);
    void step_eval_args(Frame& frame);
    void step_apply_proc(Frame& frame);
    void step_eval_if(Frame& frame);
    void step_eval_and(Frame& frame);
    void step_eval_or(Frame& frame);
    void step_eval_begin(Frame& frame);
    void step_eval_define(Frame& frame);
    void step_eval_set(Frame& frame);

    /**
     * @brief Handle call/cc evaluation (capture Continuation).
     */
    void step_eval_callcc(Frame& frame);
    void step_eval_cond(Frame& frame);

    /**
     * @brief Handle dynamic-wind evaluation.
     */
    void step_eval_dynamic_wind(Frame& frame);

    // Special form dispatch
    bool dispatch_special_form(Frame& frame, Object* fn_expr, Object* args);

    // Individual special form handlers
    // Core language
    void sf_quote(Frame& frame, Object* args);
    void sf_lambda(Frame& frame, Object* args);
    void sf_if(Frame& frame, Object* args);
    void sf_begin(Frame& frame, Object* args);
    void sf_define(Frame& frame, Object* args);
    void sf_set(Frame& frame, Object* args);

    // Short-circuit evaluation
    void sf_and(Frame& frame, Object* args);
    void sf_or(Frame& frame, Object* args);

    // Conditionals
    void sf_cond(Frame& frame, Object* args);

    // Dynamic-wind (requires Continuation support)
    /**
     * @brief Special form handler for dynamic-wind.
     */
    void sf_dynamic_wind(Frame& frame, Object* args);

    // NOTE: Desugaring forms (let, let*, letrec, case) and Macro forms
    // (let-syntax, letrec-syntax) are now handled by the Expander module.
    // Their implementations remain in toy_vm.cpp as legacy code but are unused.

    // Debug: show the stack
    void print_stack() {
        std::cout << "\n=== VM Stack (depth=" << stack.size() << ") ===" << std::endl;
        for (auto i = static_cast<std::ptrdiff_t>(stack.size()) - 1; i >= 0; i--) {
            auto& f = stack[i];
            std::cout << "  [" << i << "] " << frame_name(f.type) << " expr=";
            if (f.expr) {
                std::cout << f.expr;
            } else {
                std::cout << "null";
            }
            std::cout << std::endl;
        }
        std::cout << "  ACC: " << accumulator << std::endl;
        std::cout << "========================" << std::endl;
    }
};

} // namespace toy
