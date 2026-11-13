#pragma once

/**
 * @file primitive_parameters.h
 * @brief R7RS Parameter primitives (Section 4.2.6)
 */

#include "../primitive_base.h"
#include "../Environment.h"

/**
 * @brief R7RS Parameter primitives
 *
 * Implements R7RS parameter operations:
 * - make-parameter: Create a new parameter Object
 * - parameter?: Check if Object is a parameter
 * - %parameterize: runtime support the bytecode compiler rewrites
 *   parameterize forms into (the recursive evaluator handles parameterize
 *   natively in lisp.cpp)
 */
class PrimitiveParameters : public PrimitiveBase {
public:
    enum ID {
        MAKEPARAMETER,    // make-parameter
        PARAMETERQ,       // parameter?
        PARAMETERIZEIMPL  // %parameterize (runtime support for compiled parameterize)
    };

    PrimitiveParameters(ID id, int minArgs, int maxArgs);

    /**
     * @brief Apply the Primitive operation
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all parameter primitives into environment
     * @param env Environment to install into
     */
    static void install(Environment* env);

private:
    ID m_id;
    std::string m_name;
};
