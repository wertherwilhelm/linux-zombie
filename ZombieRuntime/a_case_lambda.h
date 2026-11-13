/**
 * @file a_case_lambda.h
 * @brief R7RS case-lambda implementation (Section 4.2.9)
 *
 * Case-lambda allows procedures to have different behavior based on argument count:
 *
 * (define plus
 *   (case-lambda
 *     (() 0)
 *     ((x) x)
 *     ((x y) (+ x y))))
 *
 * (plus)      => 0
 * (plus 5)    => 5
 * (plus 1 2)  => 3
 */

#pragma once

#include "procedure.h"
#include "Object.h"
#include "Environment.h"
#include <vector>

namespace zombie { namespace runtime {

// IEvaluator forward declaration now in i_evaluator.h

/**
 * @brief Represents one clause in a case-lambda: (formals body)
 */
struct case_lambda_clause {
    Object* formals;  // Parameter list (can be symbol for rest args)
    Object* body;     // Expression sequence to evaluate

    case_lambda_clause(Object* f, Object* b) : formals(f), body(b) {}
};

/**
 * @brief Case-lambda Procedure with multiple arity clauses
 *
 * When applied, tries each clause in order and uses the first one
 * that matches the argument count.
 */
class a_case_lambda : public Procedure {
public:
    /**
     * @brief Construct case-lambda with list of clauses
     * @param clauses List of (formals body ...) forms
     * @param env Lexical environment for Closure capture
     */
    a_case_lambda(const std::vector<case_lambda_clause>& clauses, Environment* env);

    /**
     * @brief Apply case-lambda by dispatching to matching clause
     * @param eval Evaluator for executing selected clause body
     * @param args Arguments to match against clauses
     * @param env Environment (unused - uses captured environment)
     * @return Result of evaluating matched clause body
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Print case-lambda representation
     */
    void print(std::ostream& os) const override;

    /**
     * @brief Type discriminator
     */
    int get_type() const override;

    /**
     * @brief Clone case-lambda (deep copy)
     */
    Object* clone() const override;

    /**
     * @brief GC integration - trace all clauses and captured environment
     */
    void trace_references(Tracer& tracer) override;

    /**
     * @brief Get number of clauses
     */
    size_t clause_count() const { return m_clauses.size(); }

private:
    /**
     * @brief Check if formals match argument count
     * @param formals Parameter list (proper list, symbol, or improper list)
     * @param argc Number of arguments provided
     * @return true if this clause can handle argc arguments
     */
    bool matches_arity(Object* formals, int argc) const;

    /**
     * @brief Count parameters in formal parameter list
     * @param formals Parameter list
     * @param has_rest OUT: true if formals has rest parameter
     * @return Number of required parameters
     */
    int count_params(Object* formals, bool& has_rest) const;

    std::vector<case_lambda_clause> m_clauses;
    Environment* m_env;  // Captured lexical environment
};

/**
 * @brief Type predicate for case-lambda
 */
inline bool is_case_lambda(const Object* obj) {
    return obj && dynamic_cast<const a_case_lambda*>(obj) != nullptr;
}

} } // namespace zombie::runtime
