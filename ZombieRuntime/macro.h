#pragma once

#include "closure.h"
#include "Environment.h"
#include "Lisp.h"

namespace zombie { namespace runtime {

/**
 * @file Macro.h
 * @brief Macro Procedure type for syntax expansion.
 */

/**
 * @brief Macro Procedure (syntax transformer).
 *
 * Macros expand their input form to a new expression before evaluation.
 */
class Macro : public Closure
{
public:
    /**
     * @brief Default constructor.
     */
    Macro();

    /**
     * @brief Destructor.
     */
    virtual ~Macro();

    /**
     * @brief Construct a Macro with parameters, body, and environment.
     * @param parms Formal parameters.
     * @param body Macro body.
     * @param env Defining environment.
     */
    Macro(Object *parms, Object *body, Environment*env);

    /**
     * @brief Expand a Macro invocation.
     * @param interpreter Lisp interpreter instance.
     * @param oldPair Original invocation form.
     * @param args Macro arguments.
     * @param env Environment for expansion.
     * @return Expanded form.
     */
    ConsCell *expand(Lisp *interpreter, ConsCell *oldPair, Object *args, Environment*env);

    /**
     * @brief Expand a Macro form using the evaluator.
     * @param evaluator Evaluator to use.
     * @param x Form to expand.
     * @return Expanded form.
     */
    static Object* macro_expand(IEvaluator *evaluator, Object *x);

    /**
     * @brief Apply Macro (expand then evaluate).
     * @param evaluator Evaluator to use.
     * @param args Macro arguments.
     * @param env Environment to evaluate in.
     * @return Result of evaluation.
     */
    Object* apply(IEvaluator *evaluator, Object *args, Environment*env) override;

    /**
     * @brief Return runtime instance type code.
     */
    int instance_of() override {return static_cast<int>(Base::atype::MACRO);}
};

} } // namespace zombie::runtime
