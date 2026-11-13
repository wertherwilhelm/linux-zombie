/**
 * @file Environment.h
 * @brief Lexical environment for variable bindings and scope management
 * @author chrislk@gmail.com
 * @date 2024-06
 *
 * Implements Scheme's lexical scoping using association lists (A-Lists).
 * Each environment maps variable names to values and chains to a parent
 * environment for nested scope lookup.
 *
 * @note Intentional misspelling: "Environment" throughout codebase
 *       (too pervasive to rename without breaking everything)
 */
#pragma once

#include "Base.h"

namespace zombie { namespace runtime {

// Forward declaration within namespace
class PrimitiveBase;

/**
 * @brief Lexical environment for variable bindings
 *
 * Implements Scheme's lexical scoping using association lists. Each environment
 * stores parallel lists of variable names and values, with a parent pointer for
 * scope chaining.
 *
 * **Structure:**
 * - _vars: List of variable names (symbols or strings)
 * - _vals: List of corresponding values (parallel to _vars)
 * - _parent: Outer scope environment (null for global)
 *
 * **Scope Chain:**
 * When looking up a variable:
 * 1. Search local bindings (_vars/_vals)
 * 2. If not found, recursively search parent
 * 3. If not in any scope, return nil or error
 *
 * **Usage Patterns:**
 * - Global environment: Created with no parent
 * - Function call: extend() creates new environment with parent
 * - Let bindings: Create temporary extended environment
 *
 * Example:
 * @code
 * Environment* global = new Environment("global");
 * global->define("x", make_number(42));
 *
 * // Create nested scope
 * Environment* local = global->extend(params, args);
 * Object* val = local->lookup("x");  // Searches local, then global
 * @endcode
 *
 * @see Closure for captured environments
 * @see define() for creating bindings
 * @see lookup() for variable resolution
 */
class Environment : public Base
{
public:
    /**
     * @brief Construct empty global environment
     * @note Creates environment with no parent and no bindings
     */
    Environment();

    /**
     * @brief Construct named environment
     * @param name Descriptive name for debugging
     * @note Name is for debugging only; not used in lookup
     */
    Environment(std::string name);

    /**
     * @brief Construct environment with bindings and parent
     * @param name Descriptive name for debugging
     * @param vars List of variable names (symbols or strings)
     * @param vals List of values (parallel to vars)
     * @param parent Outer scope environment (null for global)
     * @note vars and vals must have same length
     * @note Common pattern: Closure captures defining environment
     */
    Environment(std::string name, Object* vars, Object* vals, Environment* parent);

    /**
     * @brief Destructor
     */
    virtual ~Environment();

    /**
     * @brief Look up variable by name string
     * @param symbol Variable name as string
     * @return Value bound to symbol, or nil if not found
     * @note Searches local scope, then recursively searches parent chain
     */
    Object* lookup(std::string symbol);

    /**
     * @brief Look up variable by symbol Object
     * @param symbol Variable name as Object (a_symbol or a_string)
     * @return Value bound to symbol, or nil if not found
     * @note Searches local scope, then recursively searches parent chain
     */
    Object* lookup(Object* symbol);

    /**
     * @brief Define new variable binding (or update if exists)
     * @param var Variable name Object
     * @param val Value to bind
     * @return The defined value
     * @note R5RS define: Creates binding in current environment
     * @note If variable exists, updates it; otherwise creates new binding
     */
    Object* define(Object* var, Object* val);

    /**
     * @brief Define variable by string name
     * @param name Variable name as string
     * @param val Value to bind
     * @return The defined value
     * @note Convenience wrapper for define(Object*, Object*)
     */
    Object* define(std::string name, Object* val);

    /**
     * @brief Define Primitive function
     * @param val Primitive to register
     * @return The Primitive
     * @note Extracts name from Primitive and defines it
     */
    Object* define(PrimitiveBase* val);

    /**
     * @brief Set existing variable (R5RS set!)
     * @param var Variable name Object
     * @param val New value
     * @return The new value
     * @note Searches scope chain for existing binding and updates it
     * @note Error if variable not found (unlike define, which creates)
     */
    Object* set(Object* var, Object* val);

    /**
     * @brief Validate argument count matches parameter count
     * @param vars Parameter list (from lambda)
     * @param vals Argument list (from call site)
     * @return true if counts match
     * @note Handles dotted parameter lists for variadic functions
     */
    bool number_args_ok(Object* vars, Object* vals);

    /**
     * @brief Create new environment extending this one
     * @param params Parameter names (from lambda definition)
     * @param args Argument values (from function call)
     * @return New environment with params bound to args, parent is this
     * @note Used when calling functions to create activation frame
     *
     * Example:
     * @code
     * // Lambda: (lambda (x y) body)
     * // Call: (f 1 2)
     * Environment* call_env = closure_env->extend(params, args);
     * // call_env now has x=1, y=2, parent=closure_env
     * @endcode
     */
    Environment* extend(Object* params, Object* args);

    /**
     * @brief Set parent environment (for late binding)
     * @param parent New parent environment
     * @note Used when dynamically adjusting scope chain
     */
    void set_parent(Environment* parent) { m_parent = parent; }

    /**
     * @brief Trace all bindings and parent for garbage collection
     * @param tracer GC tracer
     * @note Visits m_vars, m_vals, and m_parent
     */
    void trace_references(Tracer& tracer) override;

private:
    Object* m_vars;         ///< List of variable names (symbols/strings)
    Object* m_vals;         ///< List of values (parallel to m_vars)
    Environment* m_parent;  ///< Outer scope environment (null for global)
};

} } // namespace zombie::runtime
