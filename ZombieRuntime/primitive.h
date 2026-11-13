/**
 * @file Primitive.h
 * @brief Built-in Primitive functions for Scheme interpreter
 * @author chrislk@gmail.com
 * @date 2024-06
 *
 * Implements ~200 R5RS Scheme Primitive functions (built-in functions written
 * in C++ rather than Scheme). These are the fundamental operations that all
 * Scheme code builds upon: arithmetic (+, -, *, /), list manipulation (car,
 * cdr, cons), I/O (display, read, write), control flow (apply, call/cc), etc.
 *
 * **Architecture:**
 * All primitives are implemented as OOP classes derived from PrimitiveBase:
 * - PrimitiveBooleans, PrimitiveNumbers, PrimitiveLists, PrimitiveStrings, etc.
 * - Each class handles a group of related primitives
 * - install_primitives() registers all OOP primitive classes
 *
 * **Primitive Groups (in primitives/ directory):**
 * - PrimitiveBooleans: not, boolean?
 * - PrimitiveNumbers: +, -, *, /, =, <, >, number?, etc.
 * - PrimitiveLists: cons, car, cdr, append, member, etc.
 * - PrimitiveStrings: string-append, substring, etc.
 * - PrimitiveVectors: vector, make-vector, vector-ref, etc.
 * - PrimitiveIO: read, write, display, open-*, etc.
 * - PrimitiveControl: apply, call/cc, dynamic-wind, map, etc.
 * - And more...
 *
 * @see PrimitiveBase for base class of OOP primitives
 * @see Procedure for callable base class
 * @see Environment for binding primitives
 */
#pragma once

#include "procedure.h"
#include <set>
#include <map>
#include <vector>

namespace zombie { namespace runtime {
// Forward declarations within namespace
class Lisp;
class Environment;

/**
 * @brief Base class for Primitive functions (legacy, use PrimitiveBase instead)
 *
 * This is the legacy base class for primitives. All actual primitive dispatch
 * now goes through OOP classes derived from PrimitiveBase (in primitives/ dir).
 *
 * **Modern Primitives:**
 * Use classes in primitives/ directory:
 * - PrimitiveBooleans, PrimitiveNumbers, PrimitiveLists, etc.
 * - Each class implements apply() with its own dispatch
 *
 * **Creating Custom Primitives:**
 * Option 1: Use TemplatedProcedure (simplest):
 * @code
 * define(env, "my-prim", [](IEvaluator* eval, Object* args, Environment* env) {
 *     return result;
 * });
 * @endcode
 *
 * Option 2: Subclass PrimitiveBase (for multiple related primitives):
 * @code
 * class MyPrimitives : public PrimitiveBase {
 *     Object* apply(IEvaluator* eval, Object* args, Environment* env) override;
 *     static void install(Environment* env);
 * };
 * @endcode
 *
 * @see PrimitiveBase for OOP primitive base class
 * @see TemplatedProcedure for lambda-based primitives
 */
class Primitive : public Procedure
{
public:
    /**
     * @brief Construct empty Primitive
     * @note Rarely used directly; use Primitive(id, minArgs, maxArgs) instead
     */
    Primitive();

    /**
     * @brief Construct Primitive with name only
     * @param name Primitive name (for display/debugging)
     * @note Legacy constructor; prefer Primitive(id, minArgs, maxArgs)
     */
    Primitive(std::string name);

    /**
     * @brief Copy constructor
     * @param other Primitive to copy
     */
    Primitive(const Primitive& other);

    /**
     * @brief Construct Primitive with ID and arity
     * @param id Primitive ID from prims enum
     * @param minArgs Minimum argument count (-1 = no minimum)
     * @param maxArgs Maximum argument count (-1 = unlimited)
     * @note This is the standard way to create primitives
     *
     * Example:
     * @code
     * Primitive* plus = new Primitive(Primitive::PLUS, 0, -1);  // (+ ...) accepts 0+ args
     * Primitive* car_prim = new Primitive(Primitive::CAR, 1, 1);  // (car x) requires exactly 1 arg
     * @endcode
     */
    Primitive(int id, int minArgs, int maxArgs);

    /**
     * @brief Clone this Primitive
     * @return New Primitive with same ID and arity
     */
    virtual Object* clone() const;

    /**
     * @brief Print Primitive to stream
     * @param os Output stream
     * @note Prints as #<Primitive:name>
     */
    virtual void print(std::ostream& os) const;

    /**
     * @brief Destructor
     */
    virtual ~Primitive();

    /**
     * @brief Get Primitive name
     * @return Name string (e.g., "+", "car", "display")
     */
    std::string get_name() {
        return m_name;
    }

    /**
     * @brief Get runtime type identifier
     * @return PRIMITIVE type code
     */
    int instance_of() override {
        return static_cast<int>(Base::atype::PRIMITIVE);
    }

    // ========================================================================
    // Primitive Installation Methods
    // ========================================================================
    //
    // NOTE: Legacy enum prims and enum ext have been REMOVED.
    // All primitive dispatch now goes through OOP classes:
    // - PrimitiveBooleans, PrimitiveNumbers, PrimitiveLists, etc.
    // See primitives/ directory for implementations.
    // ========================================================================

    /**
     * @brief Install all primitives (full R5RS + extensions)
     * @param env Environment to install primitives into
     * @return Updated environment
     * @note Standard installation - includes all ~200 primitives
     * @note This is the default used by scheme_engine
     *
     * Example:
     * @code
     * Environment* env = new Environment("global");
     * Primitive::install_primitives(env);  // Ready for R5RS code
     * @endcode
     */
    static Environment* install_primitives(Environment* env);

    /**
     * @brief Install safe primitives only (for sandboxing)
     * @param env Environment to install primitives into
     * @return Updated environment
     * @note Excludes REFLECTION and INTERPRETERS groups (prevents introspection and sub-interpreters)
     * @note Safe for running untrusted code
     */
    static Environment* install_sandbox_primitives(Environment* env);

    /**
     * @brief Install all primitives plus debug tools
     * @param env Environment to install primitives into
     * @return Updated environment
     * @note Includes all primitives + DEBUG group
     * @note Future: Will add breakpoint, trace, step commands
     */
    static Environment* install_debug_primitives(Environment* env);

    // ========================================================================
    // Core Methods
    // ========================================================================

    /**
     * @brief Apply Primitive to arguments (Procedure::apply override)
     * @param evaluator Evaluator for nested evaluation
     * @param args Argument list (already evaluated by caller)
     * @param env Environment (used by some primitives like eval)
     * @return Error - this legacy method should never be called
     * @note All primitive dispatch now goes through OOP classes
     *       (PrimitiveBooleans, PrimitiveNumbers, PrimitiveLists, etc.)
     * @deprecated Use OOP primitive classes instead of this legacy dispatch
     */
    virtual Object* apply(IEvaluator * evaluator, Object *args, Environment* env);

    /**
     * @brief Trace references for GC
     * @param tracer GC tracer
     * @note Base Primitive has no captured references
     */
    void trace_references(Tracer& tracer) override;

    // ========================================================================
    // Private Members
    // ========================================================================

    int m_minArgs;       ///< Minimum argument count (-1 = no minimum)
    int m_maxArgs;       ///< Maximum argument count (-1 = unlimited)
    int m_id_number;      ///< Primitive ID from prims enum
    std::string m_name;  ///< Primitive name (e.g., "+", "car", "display")
};

/**
 * @brief Template-based Primitive for C++ lambda functions
 * @tparam Func Function type (typically std::function or lambda)
 *
 * Allows creating custom primitives from C++ lambdas without subclassing.
 * Much more convenient than creating a new Primitive subclass for each
 * custom operation.
 *
 * **Usage Example:**
 * @code
 * // Create custom Primitive from C++ lambda
 * auto square = new TemplatedProcedure<std::function<Object*(IEvaluator*, Object*, Environment*)>>(
 *     "square",
 *     [](IEvaluator* eval, Object* args, Environment* env) -> Object* {
 *         Object* arg = car(args);
 *         double n = num(arg);
 *         return make_number(n * n);
 *     });
 *
 * env->define("square", square);
 *
 * // Now can call from Scheme:
 * // (square 5) => 25
 * @endcode
 *
 * **Type-Safe Wrapper:**
 * For even better ergonomics, use embedding_api.h's compile_fn() which
 * provides automatic type conversion:
 * @code
 * auto square = embedding::compile_fn<double(double)>("(lambda (x) (* x x))");
 * double result = square(5.0);  // Type-safe C++ function
 * @endcode
 *
 * @see embedding_api.h for type-safe C++/Scheme interop
 * @see Primitive for Base class
 */
template<typename Func>
class TemplatedProcedure : public Primitive {
public:
    /**
     * @brief Construct templated Procedure from C++ function
     * @param name Procedure name (for display and error messages)
     * @param func C++ function Object (lambda, std::function, function pointer)
     * @note Function signature must be: Object*(IEvaluator*, Object*, Environment*)
     */
    TemplatedProcedure(std::string name, Func func)
        : m_name(name), m_func(func) { }

    /**
     * @brief Clone this Procedure
     * @return New TemplatedProcedure with same function
     */
    Object* clone() const override {
        return new TemplatedProcedure(*this);
    }

    /**
     * @brief Print Procedure name
     * @param os Output stream
     */
    void print(std::ostream& os) const override {
        os << m_name;
    }

    /**
     * @brief Apply Procedure to arguments
     * @param evaluator Evaluator for nested evaluation
     * @param args Argument list (already evaluated)
     * @param env Environment
     * @return Result of calling m_func
     * @note Delegates directly to stored C++ function
     */
    Object* apply(IEvaluator* evaluator, Object* args, Environment* env) override {
        return m_func(evaluator, args, env);
    }

private:
    Func m_func;         ///< C++ function to call
    std::string m_name;  ///< Procedure name
};

// ============================================================================
// Helper Template - Zero-Boilerplate Primitive Registration
// ============================================================================

/**
 * @brief Register a C++ lambda as a Scheme Primitive (zero-boilerplate)
 * @tparam Func Function type (automatically deduced from lambda)
 * @param env Environment to register Primitive into
 * @param name Scheme name for the Primitive
 * @param func C++ lambda implementing the Primitive
 * @return The defined Procedure Object
 *
 * This helper eliminates boilerplate for registering primitives. Just pass
 * a lambda and it's automatically wrapped in a TemplatedProcedure.
 *
 * **Usage Example:**
 * @code
 * // One-liner Primitive registration
 * define(env, "square", [](IEvaluator* eval, Object* args, Environment* env) -> Object* {
 *     Object* arg = car(args);
 *     double n = numeric::to_double(arg);
 *     return numeric::make_number(n * n);
 * });
 *
 * // Now callable from Scheme:
 * // (square 5) => 25
 * @endcode
 *
 * **Pattern:**
 * This is the recommended way to add primitives in Primitive group files.
 * Each group provides an installation function using this pattern.
 *
 * @see TemplatedProcedure for underlying template
 * @see primitive_registry for group registration
 */
template<typename Func>
Object* define(Environment* env, const std::string& name, Func func) {
    TemplatedProcedure<Func>* Procedure = gc_new<TemplatedProcedure<Func>>(name, func);
    a_string* name_obj = gc_new<a_string>(name);
    // Protect both from GC during define operation
    gc_root<TemplatedProcedure<Func>> proc_guard(Procedure);
    gc_root<a_string> name_guard(name_obj);
    return env->define(name_obj, Procedure);
}

// ============================================================================
// Primitive Registry - Explicit Group Registration
// ============================================================================

/**
 * @brief Registry for Primitive group installation functions
 *
 * Allows applications to explicitly register custom Primitive groups and
 * selectively install them into environments. This provides clean separation
 * between core interpreter primitives and application-specific extensions.
 *
 * **Architecture:**
 * - Each Primitive group (core, lists, strings, etc.) provides an installation function
 * - Groups register themselves via `register_group()`
 * - Applications install groups selectively via `install_groups()`
 * - Backward compatibility via `Primitive::install_primitives()`
 *
 * **Usage Example (Embedding):**
 * @code
 * // Register custom primitives
 * primitive_registry::instance().register_group("my-app", [](Environment* env) {
 *     define(env, "custom-op", [](IEvaluator* eval, Object* args, Environment* env) {
 *         // Implementation
 *         return nil;
 *     });
 * });
 *
 * // Create engine and install groups
 * scheme_engine engine;
 * engine.install_group("my-app");  // Add custom primitives
 * @endcode
 *
 * **Usage Example (Group Registration):**
 * @code
 * // In primitives_core.cpp
 * namespace {
 *     struct CoreRegistrar {
 *         CoreRegistrar() {
 *             primitive_registry::instance().register_group("core",
 *                 primitives::install_core_group);
 *         }
 *     };
 *     static CoreRegistrar core_registrar;  // Auto-registers on load
 * }
 * @endcode
 *
 * @see define() for Primitive registration helper
 * @see scheme_engine::install_group() for installing groups
 */
class primitive_registry {
public:
    /// Installation function type: void(*)(Environment*)
    using InstallFunc = void (*)(Environment*);

    /**
     * @brief Get singleton registry instance
     * @return Reference to global Primitive registry
     */
    static primitive_registry& instance();

    /**
     * @brief Register a Primitive group installation function
     * @param name Group name (e.g., "core", "lists", "my-app")
     * @param func Installation function to register
     * @note Overwrites existing registration with same name
     */
    void register_group(const std::string& name, InstallFunc func);

    /**
     * @brief Install specific Primitive groups into environment
     * @param env Environment to install primitives into
     * @param names List of group names to install
     * @note Silently skips groups that aren't registered
     */
    void install_groups(Environment* env, const std::vector<std::string>& names);

    /**
     * @brief Install all registered Primitive groups
     * @param env Environment to install primitives into
     * @note Installs every group that has been registered
     */
    void install_all(Environment* env);

    /**
     * @brief Check if a group is registered
     * @param name Group name to check
     * @return true if group exists in registry
     */
    bool has_group(const std::string& name) const;

private:
    primitive_registry() = default;  ///< Private constructor (singleton)
    std::map<std::string, InstallFunc> groups_;  ///< Registered installation functions
};

} } // namespace zombie::runtime
