#pragma once

#include "Object.h"
#include <vector>

namespace zombie { namespace runtime {

class Environment;

/**
 * @file eval_result.h
 * @brief Evaluation result and work item types for stackless evaluation.
 */

/**
 * @brief Result of an evaluation step: VALUE or TAILCALL.
 *
 * Enables tail call optimization via trampolining:
 * - VALUE: evaluation complete, return this Object
 * - TAILCALL: continue evaluating expr in env (tail position)
 */
struct eval_result {
    enum tag_type {
        VALUE,      ///< Final result
        TAILCALL    ///< Tail call - needs further evaluation
    };

    tag_type tag;
    Object* value;        ///< Used when tag == VALUE
    Object* expr;         ///< Used when tag == TAILCALL
    Environment* env;      ///< Used when tag == TAILCALL

    /**
     * @brief Construct a VALUE result.
     * @param val Final value.
     * @return eval_result tagged as VALUE.
     */
    static eval_result make_value(Object* val) {
        eval_result r;
        r.tag = VALUE;
        r.value = val;
        r.expr = nullptr;
        r.env = nullptr;
        return r;
    }

    /**
     * @brief Construct a TAILCALL result.
     * @param e Expression to evaluate next.
     * @param env Environment for the tail call.
     * @return eval_result tagged as TAILCALL.
     */
    static eval_result make_tailcall(Object* e, Environment* env) {
        eval_result r;
        r.tag = TAILCALL;
        r.value = nullptr;
        r.expr = e;
        r.env = env;
        return r;
    }
};

/**
 * @brief Work item for stackless evaluation (explicit work stack).
 *
 * Enables truly unlimited TCO by using an explicit work stack instead of the
 * C++ call stack.
 */
struct work_item {
    enum work_type {
        EVAL_EXPR,          ///< Evaluate expression, push result to value stack
        EVAL_LIST,          ///< Evaluate list of expressions
        APPLY_PROC,         ///< Apply Procedure to arguments
        IF_BRANCH,          ///< Evaluate if condition and choose branch
        DEFINE_BIND,        ///< Bind result to name in environment
        SET_BIND,           ///< Set existing binding to result
        BEGIN_SEQ,          ///< Evaluate sequence of expressions
        BUILD_LIST          ///< Build a list from N values on stack
    };

    work_type type;
    Object* expr;
    Environment* env;
    Object* aux_data;  ///< Extra data (e.g., name for DEFINE_BIND, else branch for IF_BRANCH)
    int count;         ///< For BUILD_LIST: how many items to collect

    /**
     * @brief Create an EVAL_EXPR work item.
     */
    static work_item make_eval(Object* e, Environment* env) {
        return work_item{EVAL_EXPR, e, env, nullptr, 0};
    }

    /**
     * @brief Create an EVAL_LIST work item.
     */
    static work_item make_eval_list(Object* lst, Environment* env) {
        return work_item{EVAL_LIST, lst, env, nullptr, 0};
    }

    /**
     * @brief Create an APPLY_PROC work item.
     */
    static work_item make_apply(Object* proc, Environment* env) {
        return work_item{APPLY_PROC, proc, env, nullptr, 0};
    }

    /**
     * @brief Create an IF_BRANCH work item.
     */
    static work_item make_if_branch(Object* then_expr, Object* else_expr, Environment* env) {
        return work_item{IF_BRANCH, then_expr, env, else_expr, 0};
    }

    /**
     * @brief Create a DEFINE_BIND work item.
     */
    static work_item make_define(Object* name, Environment* env) {
        return work_item{DEFINE_BIND, nullptr, env, name, 0};
    }

    /**
     * @brief Create a SET_BIND work item.
     */
    static work_item make_set(Object* name, Environment* env) {
        return work_item{SET_BIND, nullptr, env, name, 0};
    }

    /**
     * @brief Create a BUILD_LIST work item.
     * @param n Number of values to collect into a list.
     */
    static work_item make_build_list(int n) {
        return work_item{BUILD_LIST, nullptr, nullptr, nullptr, n};
    }
};

} } // namespace zombie::runtime
