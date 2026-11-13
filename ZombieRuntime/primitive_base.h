#pragma once

#include "procedure.h"
#include "atom.h"
#include "gc_ptr.h"
#include <string>
#include <map>
#include <vector>

namespace zombie { namespace runtime {

/**
 * @file PrimitiveBase.h
 * @brief Abstract Base class for all Primitive groups
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 *
 * Provides common functionality for derived Primitive classes.
 * Each Primitive group (Booleans, Lists, Numbers, etc.) inherits from this
 * Base and implements its own apply() method with its own switch statement.
 *
 * **Design Pattern:**
 * This is the **Template Method Pattern** - the Base class provides common
 * functionality (arity checking, error handling) while derived classes
 * implement specific behavior (Primitive dispatch).
 *
 * **Hierarchy:**
 * - PrimitiveBase (abstract Base)
 *   - PrimitiveBooleans (not, boolean?)
 *   - PrimitiveEquivalence (eq?, eqv?, equal?)
 *   - PrimitiveLists (car, cdr, cons, append, ...)
 *   - PrimitiveNumbers (+, -, *, /, ...)
 *   - ... (more groups)
 *
 * **Benefits:**
 * - Modularity: Each group is independently testable
 * - Encapsulation: Each group owns its primitives
 * - Maintainability: Small focused switch statements instead of 2,337-line monolith
 */

/**
 * @brief Abstract Base class for all Primitive groups
 *
 * Provides common functionality for derived Primitive classes:
 * - Arity checking (min/max arguments)
 * - Error handling helpers
 * - Common members (name, arg counts)
 *
 * Each derived class (PrimitiveLists, PrimitiveNumbers, etc.) inherits from
 * this Base and implements its own apply() method with its own switch statement.
 *
 * **Usage Example:**
 * @code
 * class PrimitiveBooleans : public PrimitiveBase {
 * public:
 *     enum ID { NOT = 0, BOOLEANQ = 1 };
 *
 *     PrimitiveBooleans(ID id) : PrimitiveBase(1, 1), m_id(id) {
 *         switch (id) {
 *             case NOT:      m_name = "not"; break;
 *             case BOOLEANQ: m_name = "boolean?"; break;
 *         }
 *     }
 *
 *     Object* apply(IEvaluator* eval, Object* args, Environment* env) override {
 *         if (!check_arity(args)) {
 *             return arity_error("");
 *         }
 *         Object* x = first(args);
 *         switch (m_id) {
 *             case NOT:      return truth(x == false_obj);
 *             case BOOLEANQ: return truth(is_boolean(x));
 *         }
 *     }
 *
 *     static void install(Environment* env) {
 *         env->define(new a_string("not"), new PrimitiveBooleans(NOT));
 *         env->define(new a_string("boolean?"), new PrimitiveBooleans(BOOLEANQ));
 *     }
 *
 * private:
 *     ID m_id;
 * };
 * @endcode
 *
 * @see Procedure for abstract Base class
 * @see PrimitiveBooleans for example derived class
 */
class PrimitiveBase : public Procedure {
public:
    /**
     * @brief Construct Primitive with arity constraints
     * @param minArgs Minimum number of arguments required
     * @param maxArgs Maximum number of arguments allowed
     * @note Use 0 for minArgs if Primitive accepts zero arguments
     * @note Use 999999 for maxArgs if Primitive accepts unlimited arguments
     */
    PrimitiveBase(int minArgs, int maxArgs);

    /**
     * @brief Virtual destructor
     * @note Ensures proper cleanup of derived classes
     */
    virtual ~PrimitiveBase() = default;

    /**
     * @brief Apply Primitive to arguments (abstract method)
     * @param eval Evaluator for nested evaluation (dependency injection)
     * @param args Argument list (already evaluated by caller)
     * @param env Environment for evaluation
     * @return Result of applying Primitive
     * @note Derived classes MUST override this method
     * @note Each derived class implements its own switch statement for dispatch
     *
     * Typical implementation pattern:
     * @code
     * Object* apply(IEvaluator* eval, Object* args, Environment* env) override {
     *     // Arity checking
     *     if (!check_arity(args)) {
     *         return arity_error("");
     *     }
     *
     *     // Extract arguments
     *     Object* x = first(args);
     *     Object* y = (args && rest(args)) ? second(args) : nil;
     *
     *     // Dispatch based on Primitive ID
     *     switch (m_id) {
     *         case MY_PRIMITIVE_1: return do_something(x);
     *         case MY_PRIMITIVE_2: return do_something_else(x, y);
     *         default: return error("Unknown Primitive");
     *     }
     * }
     * @endcode
     */
    virtual Object* apply(IEvaluator* eval, Object* args, Environment* env) override = 0;

    /**
     * @brief Get runtime type identifier
     * @return PRIMITIVE type code (for CoreIR compatibility)
     * @note Overrides Procedure::instance_of() to return PRIMITIVE instead of PROCEDURE
     */
    int instance_of() override {
        return static_cast<int>(Base::atype::PRIMITIVE);
    }

    /**
     * @brief Get primitive name
     * @return Name of the primitive (e.g., "+", "car", "display")
     */
    std::string get_name() const {
        return m_name;
    }

protected:
    /**
     * @brief Check if argument count is within valid range
     * @param args Argument list to check
     * @return true if argument count is valid, false otherwise
     * @note Uses m_minArgs and m_maxArgs for validation
     * @note Handles NULL and nil arguments correctly
     */
    bool check_arity(Object* args);

    /**
     * @brief Generate arity error message
     * @param context Optional context string for error message
     * @return Error Object with arity error message
     * @note Includes Primitive name in error message
     *
     * Usage:
     * @code
     * if (!check_arity(args)) {
     *     return arity_error("");
     * }
     * @endcode
     */
    Object* arity_error(const std::string& context);

    /**
     * @brief Generate type error message
     * @param expected Description of expected type (e.g., "number", "list")
     * @param got Object that caused the error
     * @return Error Object with type error message
     * @note Includes Primitive name in error message
     *
     * Usage:
     * @code
     * if (!is_number(x)) {
     *     return type_error("number", x);
     * }
     * @endcode
     */
    Object* type_error(const std::string& expected, Object* got);

    // Common members available to derived classes
    int m_minArgs;       ///< Minimum number of arguments required
    int m_maxArgs;       ///< Maximum number of arguments allowed (999999 = unlimited)
    std::string m_name;  ///< Primitive name (for error messages)
};

} } // namespace zombie::runtime

// Bring PrimitiveBase into global namespace for backward compatibility with
// code that uses PrimitiveBase without namespace qualification
using zombie::runtime::PrimitiveBase;

// ============================================================================
// Note: TemplatedProcedure, define(), and primitive_registry
// ============================================================================
// These classes are defined in primitive.h in namespace zombie::runtime.
// Files that need them should include primitive.h directly.
