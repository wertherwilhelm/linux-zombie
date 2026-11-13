/**
 * @file IEvaluator.h
 * @brief Abstract evaluator interface for pluggable evaluation strategies
 * @author chrislk@gmail.com
 * @date 2024-06
 *
 * Defines the interface that all evaluators must implement. This enables
 * dependency injection and allows switching between different evaluation
 * strategies (tree-walking, bytecode VM, Continuation-based, etc.) without
 * changing client code.
 *
 * **Design Pattern:** Strategy Pattern + Dependency Inversion
 *
 * **Implementors:**
 * - Lisp: Traditional recursive tree-walking interpreter
 * - BytecodeVM: Bytecode compiler and virtual machine
 * - toy_vm: Experimental stack-based VM with explicit continuations
 * - CoreIR evaluator: Optimized intermediate representation evaluator
 */
#pragma once

#include "Object.h"

namespace zombie { namespace runtime {

// Forward declarations
class Environment;

/**
 * @brief Abstract interface for Scheme expression evaluation
 *
 * This interface decouples high-level code (primitives, closures, macros) from
 * specific evaluator implementations. Any evaluator can be used interchangeably
 * as long as it implements eval() and get_environment().
 *
 * **Design Benefits:**
 * - **Pluggable evaluators**: Switch between interpreters without code changes
 * - **Dependency injection**: Closures/primitives receive IEvaluator*, not concrete type
 * - **Testing**: Easy to mock evaluators for unit tests
 * - **Experimentation**: Try new evaluation strategies (bytecode, JIT, etc.)
 *
 * **Usage Pattern:**
 * @code
 * // Primitives and closures receive IEvaluator*
 * Object* Primitive::prim_apply(IEvaluator* eval, Object* args, Environment* env) {
 *     // Call back to evaluator to eval arguments
 *     Object* fn = eval->eval(first(args), env);
 *     // ...
 * }
 *
 * // Can use any evaluator implementation
 * IEvaluator* eval1 = new Lisp();           // Tree-walking
 * IEvaluator* eval2 = new BytecodeVM(env);  // Bytecode
 * IEvaluator* eval3 = new toy_vm(&env);     // Stack machine
 * @endcode
 *
 * **Concrete Implementations:**
 * - **Lisp**: Traditional recursive interpreter with TCO support
 * - **BytecodeVM**: Compiles to bytecode, executes on stack VM
 * - **toy_vm**: Experimental VM with explicit continuations
 * - **CoreIR evaluator**: Uses optimized intermediate representation
 *
 * @see Lisp for tree-walking implementation
 * @see BytecodeVM for bytecode implementation
 * @see toy_vm for Continuation-based implementation
 * @see scheme_engine for unified wrapper
 */
class IEvaluator {
public:
    /**
     * @brief Virtual destructor (required for interface)
     */
    virtual ~IEvaluator() = default;

    /**
     * @brief Evaluate a Scheme expression in the given environment
     * @param expr Expression to evaluate (can be atom, list, or special form)
     * @param env Lexical environment for variable lookup
     * @return Result of evaluation
     *
     * **Evaluation Rules:**
     * - Self-evaluating: Numbers, strings, booleans return themselves
     * - Symbols: Look up value in environment
     * - Lists: First element determines behavior:
     *   - Special form (quote, if, lambda, etc.): Handle specially
     *   - Function: Evaluate args and apply function
     *
     * **Special Forms** (handled internally):
     * - quote: Return argument unevaluated
     * - if: Conditional evaluation
     * - lambda: Create Closure
     * - define: Create binding
     * - set!: Mutate binding
     * - begin: Sequence evaluation
     * - and, or: Short-circuit logic
     *
     * Example:
     * @code
     * Object* expr = read_string("(+ 1 2)");
     * Object* result = eval->eval(expr, global_env);  // Returns 3
     * @endcode
     */
    virtual Object* eval(Object* expr, Environment* env) = 0;

    /**
     * @brief Get the evaluator's global environment
     * @return Pointer to global environment
     *
     * @note Each evaluator instance has its own global environment
     *       containing Primitive bindings and user definitions
     *
     * Use cases:
     * - Accessing global bindings from primitives
     * - Creating new top-level definitions
     * - Resetting interpreter state
     */
    virtual Environment* get_environment() = 0;
};

} } // namespace zombie::runtime
