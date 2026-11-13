/**
 * @file a_case_lambda.cpp
 * @brief Implementation of R7RS case-lambda
 */

#include "pch.h"
#include "a_case_lambda.h"
#include "i_evaluator.h"
#include "Environment.h"
#include "Base.h"
#include "cons_list.h"
#include "atom.h"
#include "constants.h"
#include <sstream>

namespace zombie { namespace runtime {

// ============================================================================
// Constructor
// ============================================================================

a_case_lambda::a_case_lambda(const std::vector<case_lambda_clause>& clauses, Environment* env)
    : Procedure(), m_clauses(clauses), m_env(env) {
}

// ============================================================================
// Arity Matching
// ============================================================================

int a_case_lambda::count_params(Object* formals, bool& has_rest) const {
    has_rest = false;

    // Symbol formals: (lambda args ...) - rest parameter only
    if (formals->is_atom() && formals->get_type() == static_cast<int>(Type::SYMBOL)) {
        has_rest = true;
        return 0;  // No required params, accepts any number
    }

    // Nil formals: (lambda () ...) - no parameters
    if (formals->is_nil()) {
        return 0;
    }

    // List formals: count elements
    int count = 0;
    Object* current = formals;

    while (current && !current->is_nil()) {
        if (!current->pair()) {
            // Improper list: (lambda (x y . rest) ...)
            has_rest = true;
            break;
        }
        count++;
        current = cdr(current);
    }

    return count;
}

bool a_case_lambda::matches_arity(Object* formals, int argc) const {
    bool has_rest = false;
    int required = count_params(formals, has_rest);

    if (has_rest) {
        // Rest parameter: accepts required or more
        return argc >= required;
    } else {
        // Exact match required
        return argc == required;
    }
}

// ============================================================================
// Application
// ============================================================================

Object* a_case_lambda::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Count arguments
    int argc = 0;
    Object* arg_iter = args;
    while (arg_iter && !arg_iter->is_nil()) {
        argc++;
        arg_iter = cdr(arg_iter);
    }

    // Try each clause in order
    for (const auto& clause : m_clauses) {
        if (matches_arity(clause.formals, argc)) {
            // Found matching clause - bind parameters and evaluate body

            // Create new environment by extending captured environment with parameter bindings
            // This uses the same approach as Closure::apply
            Environment* clause_env = m_env->extend(clause.formals, args);

            // Evaluate body (implicit begin)
            Object* result = nil;
            Object* body_exprs = clause.body;

            while (body_exprs && !body_exprs->is_nil()) {
                result = eval->eval(car(body_exprs), clause_env);
                body_exprs = cdr(body_exprs);
            }

            return result;
        }
    }

    // No matching clause - error
    std::ostringstream oss;
    oss << "case-lambda: no clause matches " << argc << " argument";
    if (argc != 1) oss << "s";
    throw std::runtime_error(oss.str());
}

// ============================================================================
// Printing
// ============================================================================

void a_case_lambda::print(std::ostream& os) const {
    os << "#<case-lambda:" << m_clauses.size() << "-clauses>";
}

// ============================================================================
// Type Information
// ============================================================================

int a_case_lambda::get_type() const {
    return static_cast<int>(Type::NOTYPE);  // Procedures don't have specific type enum
}

// ============================================================================
// Cloning
// ============================================================================

Object* a_case_lambda::clone() const {
    return gc_new<a_case_lambda>(m_clauses, m_env);
}

// ============================================================================
// Garbage Collection
// ============================================================================

void a_case_lambda::trace_references(Tracer& tracer) {
    Procedure::trace_references(tracer);

    // Trace captured environment
    if (m_env) {
        Object* env_obj = (Object*)m_env;
        tracer.visit_field(env_obj);
        m_env = (Environment*)env_obj;
    }

    // Trace all clause formals and bodies
    for (auto& clause : m_clauses) {
        tracer.visit_field(clause.formals);
        tracer.visit_field(clause.body);
    }
}

} } // namespace zombie::runtime
