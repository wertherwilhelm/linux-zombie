/**
 * @file Procedure.h
 * @brief Abstract Base class for all callable objects (functions) in Scheme
 * @author chrislk@gmail.com
 * @date 2024-06
 *
 * Provides the common interface for all Scheme procedures (functions).
 * This includes both built-in primitives and user-defined closures.
 *
 * **Hierarchy:**
 * - Procedure (abstract Base)
 *   - Primitive (built-in C++ functions like +, car, cdr)
 *   - Closure (user-defined lambda functions)
 *   - Macro (syntax transformers, subclass of Closure)
 *   - Continuation (first-class continuations from call/cc)
 *
 * **Design Pattern:**
 * This is the **Strategy Pattern** for function application. All callable
 * objects implement the apply() method, allowing polymorphic function calls.
 */
#pragma once

#include "Base.h"
#include "Environment.h"

namespace zombie { namespace runtime {

// Forward declare IEvaluator (in namespace)
class IEvaluator;

/**
 * @brief Abstract Base class for all callable Scheme objects
 *
 * All Scheme procedures (functions) inherit from this class and implement
 * the apply() method. This enables polymorphic function application -
 * the evaluator doesn't need to know if it's calling a built-in Primitive
 * or a user-defined Closure.
 *
 * **Calling Convention:**
 * When a Scheme expression like `(f arg1 arg2)` is evaluated:
 * 1. Evaluator evaluates `f` to get a Procedure Object
 * 2. Evaluator evaluates arguments to get values
 * 3. Evaluator calls `Procedure->apply(evaluator, args, env)`
 * 4. Procedure executes and returns result
 *
 * **Dependency Injection:**
 * The apply() method receives an IEvaluator* parameter, allowing procedures
 * to call back into the evaluator if needed (e.g., for argument evaluation,
 * special forms, or nested evaluation).
 *
 * **Subclasses:**
 * - **Primitive**: Built-in C++ functions (see Primitive.h)
 * - **Closure**: User-defined lambda functions (see Closure.h)
 * - **Macro**: Syntax transformers (subclass of Closure)
 * - **Continuation**: First-class continuations from call/cc
 *
 * **Usage Example:**
 * @code
 * // Define a custom Procedure
 * class my_procedure : public Procedure {
 * public:
 *     Object* apply(IEvaluator* eval, Object* args, Environment* env) override {
 *         Object* arg1 = car(args);  // Get first argument
 *         Object* arg2 = car(cdr(args));  // Get second argument
 *         // ... perform operation ...
 *         return result;
 *     }
 * };
 *
 * // Use it
 * Procedure* proc = new my_procedure();
 * Object* result = proc->apply(evaluator, arg_list, env);
 * @endcode
 *
 * @see Primitive for built-in functions
 * @see Closure for lambda closures
 * @see IEvaluator for evaluator interface
 */
class Procedure : public Base
{
public:
    /**
     * @brief Construct empty Procedure
     * @note Base constructor; subclasses should override apply()
     */
    Procedure();

    /**
     * @brief Virtual destructor
     * @note Ensures proper cleanup of derived classes
     */
    virtual ~Procedure();

    /**
     * @brief Apply Procedure to arguments (abstract method)
     * @param evaluator Evaluator for nested evaluation (dependency injection)
     * @param args Argument list (typically unevaluated for special forms, evaluated for functions)
     * @param env Environment for evaluation
     * @return Result of applying Procedure
     * @note Base implementation returns NULL - subclasses MUST override
     * @note Primitives and closures have different calling conventions:
     *   - **Primitives**: Arguments already evaluated by caller
     *   - **Closures**: Arguments already evaluated, bind to parameters
     *   - **Special forms**: Arguments NOT evaluated (special handling)
     *
     * Typical implementation pattern:
     * @code
     * Object* apply(IEvaluator* eval, Object* args, Environment* env) override {
     *     // Extract arguments
     *     Object* arg1 = car(args);
     *     Object* arg2 = car(cdr(args));
     *
     *     // Perform operation
     *     Object* result = do_something(arg1, arg2);
     *
     *     return result;
     * }
     * @endcode
     */
    virtual Object* apply(IEvaluator *evaluator, Object *args, Environment*env)
    {
        return NULL;
    }

    /**
     * @brief Cast Object to Procedure with type checking
     * @param x Object to cast
     * @return Procedure pointer
     * @note TODO: Currently returns dummy Procedure on type mismatch instead of error
     * @note Future: Should throw exception or return error Object
     *
     * Usage:
     * @code
     * Object* obj = env->lookup("my-function");
     * Procedure* proc = Procedure::proc(obj);
     * if (proc) {
     *     Object* result = proc->apply(eval, args, env);
     * }
     * @endcode
     */
    static Procedure* proc(Object *x)
    {
        if (!x) {
            return nullptr;  // Null input
        }

        // Check if it's any callable type (PROCEDURE, PRIMITIVE, CLOSURE, MACRO)
        int type = x->instance_of();
        if (type == static_cast<int>(Base::atype::PROCEDURE) ||
            type == static_cast<int>(Base::atype::PRIMITIVE) ||
            type == static_cast<int>(Base::atype::CLOSURE) ||
            type == static_cast<int>(Base::atype::MACRO)) {
            return static_cast<Procedure*>(x);
        }

        // Not a Procedure - return nullptr, let caller handle the error
        // This prevents silent type errors and memory leaks
        return nullptr;
    }

    /**
     * @brief Get runtime type identifier
     * @return PROCEDURE type code
     * @note Overridden by subclasses (PRIMITIVE, CLOSURE, MACRO, etc.)
     */
    int instance_of() override {
        return static_cast<int>(Base::atype::PROCEDURE);
    }

    /**
     * @brief Trace references for garbage collection
     * @param tracer GC tracer
     * @note Base implementation traces nothing; subclasses override to trace captured objects
     */
    void trace_references(Tracer& tracer) override;
};

} } // namespace zombie::runtime

