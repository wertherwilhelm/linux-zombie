#pragma once

#include "../primitive_base.h"

/**
 * @file primitive_system.h
 * @brief System interface primitives (R7RS Section 6.14)
 * @date 2026-01-11
 *
 * Implements system interface operations including:
 * - exit, emergency-exit
 * - command-line
 * - get-environment-variable, get-environment-variables
 */

/**
 * @brief System interface primitives (R7RS Section 6.14)
 *
 * Handles all system interface operations.
 *
 * **Example Usage:**
 * @code
 * (exit)                          ; Normal exit with code 0
 * (exit 1)                        ; Exit with code 1
 * (command-line)                  ; Get command line arguments
 * (get-environment-variable "PATH")  ; Get environment variable
 * @endcode
 */
class PrimitiveSystem : public PrimitiveBase {
public:
    /**
     * @brief Primitive IDs for system operations
     */
    enum ID {
        EXIT = 0,             ///< exit
        EMERGENCYEXIT = 1,    ///< emergency-exit
        COMMANDLINE = 2,      ///< command-line
        FEATURES = 3,         ///< features
        VOID = 4              ///< void - return unspecified value
    };

    /**
     * @brief Construct system Primitive
     * @param id Which system Primitive
     * @param min_args Minimum argument count
     * @param max_args Maximum argument count (-1 for unlimited)
     */
    PrimitiveSystem(ID id, int min_args, int max_args);

    /**
     * @brief Apply the system Primitive
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all system primitives into environment
     */
    static void install(Environment* env);

private:
    ID m_id;  ///< Which system Primitive this is
};
