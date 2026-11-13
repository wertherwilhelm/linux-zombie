#pragma once

#include "Object.h"
#include "gc_persistent_root.h"
#include "procedure.h"
#include <functional>
#include <string>

// Use types from zombie::runtime namespace
using zombie::runtime::Object;
using zombie::runtime::Environment;
using zombie::runtime::IEvaluator;
using zombie::runtime::error;

/**
 * @brief Callable C++ wrapper around a Scheme Procedure.
 *
 * Holds a Scheme Procedure (lambda, Closure, Primitive) with persistent GC
 * rooting, allowing C++ code to store and call Scheme functions safely.
 *
 * Key properties:
 * - Copyable (shared ownership via ref-counted root)
 * - Movable (efficient transfer)
 * - GC-safe (Procedure stays alive)
 * - Type-safe (optional typed interface)
 *
 * Example:
 * @code
 * scheme_fn fn = compile_proc("(lambda (x) (* x x))");
 * Object* result = fn.call(build_args(5));
 *
 * // Or type-safe:
 * auto typed_fn = fn.as_function<double(double)>();
 * double result2 = typed_fn(5.0);  // 25.0
 * @endcode
 */
class scheme_fn {
public:
    /**
     * @brief Construct from a Scheme Procedure Object.
     * @param proc Procedure Object to wrap.
     */
    explicit scheme_fn(Object* proc);

    /**
     * @brief Default constructor (empty/invalid wrapper).
     */
    scheme_fn();

    /// Copy constructor (shares GC root).
    scheme_fn(const scheme_fn&) = default;
    /// Move constructor.
    scheme_fn(scheme_fn&&) = default;
    /// Copy assignment (shares GC root).
    scheme_fn& operator=(const scheme_fn&) = default;
    /// Move assignment.
    scheme_fn& operator=(scheme_fn&&) = default;

    /**
     * @brief Check if wrapper holds a valid Procedure.
     * @return true if underlying Object is a Procedure.
     */
    bool is_valid() const;

    /**
     * @brief Call the wrapped Procedure with Scheme arguments.
     * @param args Proper list of arguments.
     * @param env Optional environment (uses global if null).
     * @return Result Object or ERROR Object on failure.
     */
    Object* call(Object* args, Environment* env = nullptr);

    /**
     * @brief Typed call interface (C++ types in/out).
     *
     * Converts C++ arguments to Scheme values, calls the Procedure, then
     * converts the result back to R. Returns default-constructed R on error.
     *
     * @tparam R Return type.
     * @tparam Args Argument types.
     * @param args Arguments to pass.
     * @return Result converted to R, or default R on failure.
     */
    template<class R, class... Args>
    R operator()(Args... args);

    /**
     * @brief Convert to std::function with a typed signature.
     *
     * @tparam R Return type.
     * @tparam Args Argument types.
     * @return std::function that forwards to this scheme_fn.
     */
    template<class R, class... Args>
    std::function<R(Args...)> as_function();

    /**
     * @brief Access the underlying Scheme Procedure Object.
     * @return Wrapped Procedure Object.
     */
    Object* get_procedure() const;

    /**
     * @brief Get last error message (if any).
     */
    std::string last_error() const { return m_last_error; }

    /**
     * @brief Clear last error message.
     */
    void clear_error() { m_last_error.clear(); }

private:
    gc_persistent_root m_root;      // Persistent root holding the Procedure
    mutable std::string m_last_error;  // Last error message

    /**
     * @brief Verify that wrapped Object is a Procedure.
     */
    bool verify_procedure() const;

    /**
     * @brief Build argument list from variadic C++ args.
     */
    template<class... Args>
    Object* build_args_impl(Args... args);
};

// Forward declaration of value_codec (defined in value_codec.h)
template<class T>
struct value_codec;

// ============================================================================
// Template implementations
// ============================================================================

#include "value_codec.h"

namespace detail {
    // Helper to build argument list from variadic args
    inline Object* build_args_base() {
        return nil;
    }

    template<class T, class... Rest>
    Object* build_args_base(T first, Rest... rest) {
        Object* first_obj = value_codec<T>::to_scheme(first);
        Object* rest_list = build_args_base(rest...);
        return cons(first_obj, rest_list);
    }
}

template<class... Args>
Object* scheme_fn::build_args_impl(Args... args) {
    return detail::build_args_base(args...);
}

template<class R, class... Args>
R scheme_fn::operator()(Args... args) {
    // Build argument list
    Object* arg_list = build_args_impl(args...);

    // Call the Procedure
    Object* result = call(arg_list);

    // Check for error
    if (!result) {
        return R();  // Default-constructed value on error
    }

    a_string* err = dynamic_cast<a_string*>(result);
    if (err && err->value().find("ERROR:") == 0) {
        return R();  // Default-constructed value on error
    }

    // Convert result to C++ type
    return value_codec<R>::from_scheme(result);
}

template<class R, class... Args>
std::function<R(Args...)> scheme_fn::as_function() {
    // Capture *this by value (copies the scheme_fn, shares the persistent root)
    return [fn = *this](Args... args) mutable -> R {
        return fn.operator()<R, Args...>(args...);
    };
}
