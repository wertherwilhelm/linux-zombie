#pragma once

/**
 * @file primitive_exceptions.h
 * @brief R7RS Exception handling primitives (Section 6.11)
 */

#include "../primitive_base.h"
#include "../Environment.h"

/**
 * @brief R7RS Exception handling primitives
 *
 * Implements R7RS exception handling:
 * - Raising: raise, raise-continuable, error
 * - Predicates: error-Object?, file-error?, read-error?
 * - Accessors: error-Object-message, error-Object-irritants
 */
class PrimitiveExceptions : public PrimitiveBase {
public:
    enum ID {
        RAISE,                   ///< raise
        RAISECONTINUABLE,        ///< raise-continuable
        ERROR,                   ///< error
        ERROROBJECTQ,            ///< error-Object?
        ERROROBJECTMESSAGE,      ///< error-Object-message
        ERROROBJECTIRRITANTS,    ///< error-Object-irritants
        FILEERRORQ,              ///< file-error?
        READERRORQ,              ///< read-error?
        WITHEXCEPTIONHANDLER,    ///< with-exception-handler
        GUARDIMPL                ///< %guard (runtime support for compiled guard)
    };

    PrimitiveExceptions(ID id, int minArgs, int maxArgs);

    /**
     * @brief Apply the Primitive operation
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all exception handling primitives into environment
     * @param env Environment to install into
     */
    static void install(Environment* env);

private:
    ID m_id;
    std::string m_name;
};
