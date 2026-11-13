/**
 * @file Closure.h
 * @brief Lambda Closure implementation - user-defined functions with captured environment
 * @author chrislk@gmail.com
 * @date 2024-06
 *
 * Implements R5RS lambda closures with lexical scoping. A Closure captures its
 * defining environment and can be called later with arguments.
 *
 * Supports multiple internal representations:
 * - AST body (traditional tree-walking evaluation)
 * - Bytecode (compiled for bytecode VM)
 * - CoreIR (intermediate representation for optimization)
 */
#pragma once
#include "procedure.h"
#include "Lisp.h"

#include "Environment.h"
#include <vector>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <string>

// Forward declaration for CoreIR support
namespace coreir {
    struct Stmt;
    using Value = zombie::runtime::Object*;
    using CoreEnv = std::unordered_map<std::string, Value>;
}

namespace zombie { namespace runtime {

/**
 * @brief Lambda Closure - user-defined function with captured environment
 *
 * Implements R5RS lambda closures with proper lexical scoping. A Closure is
 * created by evaluating (lambda (params...) body...) and captures the lexical
 * environment where it was defined.
 *
 * **Components:**
 * - _parms: Parameter list (symbols or dotted list for variadic)
 * - _body: Function body (expression or sequence)
 * - _env: Captured lexical environment (defines Closure)
 *
 * **Calling Convention:**
 * 1. Extend captured environment with parameter bindings
 * 2. Evaluate body in extended environment
 * 3. Return result of last expression in body
 *
 * **Multiple Representations:**
 * A Closure can store its body in three forms:
 * - **AST** (_body): Scheme expression tree (default)
 * - **Bytecode** (_bytecode): Compiled instructions for bytecode VM
 * - **CoreIR** (_coreir_body): Intermediate representation for optimization
 *
 * Example:
 * @code
 * // Scheme: (define make-adder (lambda (x) (lambda (y) (+ x y))))
 * // Creates Closure capturing x in environment
 * Closure* inner = new Closure(params_y, body, env_with_x);
 * @endcode
 *
 * @see Environment for lexical scoping
 * @see Procedure for Base calling interface
 * @see Macro for syntax transformers (derived from Closure)
 */
class Closure : public Procedure
{
public:
    /**
     * @brief Construct empty Closure
     * @note Rarely used; typically closures created with params/body/env
     */
    Closure();

    /**
     * @brief Construct Closure from parameters, body, and environment
     * @param parms Parameter list (symbols or dotted for variadic)
     * @param body Function body (expression or sequence)
     * @param env Captured lexical environment
     * @note This is the standard way to create closures from (lambda ...)
     *
     * Parameter forms:
     * - (x y z): Fixed arity, binds x, y, z
     * - (x y . rest): Variadic, binds x, y, and rest as list
     * - args: Variadic, binds all arguments to args
     */
    Closure(Object* parms, Object* body, Environment* env);

    /**
     * @brief Destructor
     * @note Cleans up CoreIR body if present
     */
    virtual ~Closure();

    /**
     * @brief Apply Closure to arguments (Procedure::apply override)
     * @param evaluator Evaluator to use for body evaluation
     * @param args Unevaluated argument list
     * @param env Calling environment (typically ignored, uses _env)
     * @return Result of evaluating body with args bound to parms
     * @note Arguments are evaluated before binding (applicative order)
     *
     * Algorithm:
     * 1. Evaluate arguments in calling environment
     * 2. Extend captured environment (_env) with param bindings
     * 3. Evaluate body in extended environment
     * 4. Return result
     */
    Object* apply(IEvaluator* evaluator, Object* args, Environment* env) override;

    /**
     * @brief Print Closure representation
     * @param os Output stream
     * @note Prints as #<Closure> or #<Macro>
     */
    virtual void print(std::ostream& os) const;

    /**
     * @brief Get runtime type identifier
     * @return MACRO if Macro, CLOSURE otherwise
     * @note Macros are implemented as closures with _is_macro flag
     */
    int instance_of() override { return m_is_macro ? static_cast<int>(Base::atype::MACRO) : static_cast<int>(Base::atype::CLOSURE); }

    /**
     * @brief Clone this Closure
     * @return Shallow copy (shares parms, body, env with original)
     */
    virtual Object* clone() const;

    /**
     * @brief Check if this is a Macro (direct check without vtable)
     * @return true if this is a Macro transformer
     * @note Faster than instance_of() check
     */
    bool is_macro_type() const { return m_is_macro; }

    /**
     * @brief Trace references for garbage collection
     * @param tracer GC tracer
     * @note Visits _parms, _body, _env, and _constants
     */
    void trace_references(Tracer& tracer) override;

    // ========================================================================
    // Bytecode Support
    // ========================================================================

    /**
     * @brief Check if Closure has compiled bytecode
     * @return true if bytecode representation available
     * @note Bytecode closures evaluated by bytecode VM for performance
     */
    bool has_bytecode() const { return !m_bytecode.empty(); }

    /**
     * @brief Store compiled bytecode for this Closure
     * @param bytecode Compiled instruction stream
     * @param constants Constant pool (referenced by bytecode)
     * @note Allows bytecode VM to execute Closure without interpreting AST
     */
    void set_bytecode(const std::vector<uint8_t>& bytecode, const std::vector<Object*>& constants) {
        m_bytecode = bytecode;
        m_constants = constants;
    }

    /**
     * @brief Get bytecode instruction stream
     * @return Vector of bytecode instructions
     */
    const std::vector<uint8_t>& bytecode() const { return m_bytecode; }

    /**
     * @brief Get constant pool
     * @return Vector of objects referenced by bytecode
     */
    const std::vector<Object*>& constants() const { return m_constants; }

    // ========================================================================
    // CoreIR Support (Intermediate Representation)
    // ========================================================================

    /**
     * @brief Check if Closure has CoreIR representation
     * @return true if CoreIR body available
     * @note CoreIR is intermediate form for optimization and analysis
     */
    bool has_coreir() const { return m_coreir_body != nullptr; }

    /**
     * @brief Store CoreIR representation of body
     * @param body CoreIR statement tree (takes ownership)
     * @note Defined in .cpp to avoid incomplete type issues
     */
    void set_coreir(coreir::Stmt* body);

    /**
     * @brief Get CoreIR body
     * @return Pointer to CoreIR statement tree
     */
    coreir::Stmt* coreir_body() const { return m_coreir_body; }

    /**
     * @brief Get CoreIR parameter names
     * @return Vector of parameter name strings
     */
    const std::vector<std::string>& coreir_params() const { return m_coreir_params; }

    /**
     * @brief Set CoreIR parameter names
     * @param params Vector of parameter names
     */
    void set_coreir_params(const std::vector<std::string>& params) {
        m_coreir_params = params;
    }

    /**
     * @brief Store captured CoreIR environment
     * @param env CoreIR environment map
     */
    void set_coreir_env(const coreir::CoreEnv& env) { m_coreir_captured_env = env; }

    /**
     * @brief Get captured CoreIR environment
     * @return CoreIR environment map
     */
    const coreir::CoreEnv& coreir_env() const { return m_coreir_captured_env; }

    // ========================================================================
    // Public Data Members (for direct access by evaluators)
    // ========================================================================

    Object* m_parms;        ///< Parameter list (nil, symbol, or list of symbols)
    Object* m_body;         ///< Function body (expression or sequence)
    Environment* m_env;      ///< Captured lexical environment

    // Bytecode representation (optional, for bytecode VM)
    std::vector<uint8_t> m_bytecode;      ///< Compiled bytecode instructions
    std::vector<Object*> m_constants;     ///< Constant pool for bytecode

    // CoreIR representation (optional, for CoreIR evaluator)
    coreir::Stmt* m_coreir_body;                  ///< CoreIR statement tree (owned)
    std::vector<std::string> m_coreir_params;     ///< CoreIR parameter names
    coreir::CoreEnv m_coreir_captured_env;        ///< CoreIR captured environment

protected:
    bool m_is_macro;  ///< true if this is a Macro transformer (set by Macro subclass)
};

} } // namespace zombie::runtime

