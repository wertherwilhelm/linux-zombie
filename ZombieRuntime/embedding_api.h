#pragma once

#include "scheme_fn.h"
#include "scheme_reader.h"
#include "Object.h"
#include "callable_traits.h"
#include "signature_registry.h"
#include "export_registry.h"
#include <string>
#include <functional>
#include <optional>

/**
 * embedding_api.h - Public C++ API for embedding Scheme in C++ applications
 *
 * This provides high-level compilation and invocation functions that:
 * - Parse Scheme source text
 * - Optionally expand macros
 * - Compile to callable procedures
 * - Return type-safe C++ wrappers (scheme_fn)
 *
 * Key functions:
 *   compile_proc(code)       - Compile Scheme Procedure from source
 *   compile_thunk_proc(expr) - Wrap expression as (lambda () expr)
 *   compile_fn<R(Args...)>(code)   - Return typed std::function
 *   compile_thunk<R>(expr)         - Return typed thunk
 *
 * Error handling:
 *   All functions return valid objects even on error.
 *   Check scheme_fn::is_valid() or scheme_fn::last_error()
 *   to detect failures.
 */

// Forward declarations
class IEvaluator;
class Environment;
class scheme_engine;

namespace embedding {

/**
 * Configure the embedding engine for the current thread.
 *
 * @param engine scheme_engine instance to use (not owned by embedding API)
 *
 * If set to nullptr, will use or create a default global engine.
 * The engine is stored thread-locally, so different threads can use
 * different engines.
 *
 * Note: The embedding API does NOT take ownership of the engine.
 * The caller is responsible for keeping it alive.
 */
void set_embedding_engine(scheme_engine* engine);

/**
 * Get the current embedding engine (creates default if needed).
 *
 * @return The current thread's embedding engine
 *
 * If no engine has been set via set_embedding_engine(), a default
 * engine will be created and returned.
 */
scheme_engine* get_default_engine();

/**
 * Compile a Scheme Procedure from source text.
 *
 * @param code Scheme source that evaluates to a Procedure
 *             Example: "(lambda (x) (* x x))"
 * @param expand_macros If true, perform Macro expansion (default: true)
 * @return scheme_fn wrapper (check is_valid())
 *
 * On error: Returns invalid scheme_fn with error message in last_error()
 */
scheme_fn compile_proc(const std::string& code, bool expand_macros = true);

/**
 * Compile an expression as a thunk (zero-argument lambda).
 *
 * @param expr Scheme expression
 *             Example: "(+ 3 4)"
 * @param expand_macros If true, perform Macro expansion (default: true)
 * @return scheme_fn wrapper for (lambda () <expr>)
 *
 * On error: Returns invalid scheme_fn with error message
 */
scheme_fn compile_thunk_proc(const std::string& expr, bool expand_macros = true);

/**
 * Compile a Scheme Procedure with type-safe C++ signature (function type syntax).
 *
 * @tparam Sig Function signature type (e.g., double(double, double))
 * @param code Scheme source that evaluates to a Procedure
 * @param expand_macros If true, perform Macro expansion
 * @return std::function<Sig> that calls the Scheme Procedure
 *
 * Example:
 *   auto square = compile_fn<double(double)>("(lambda (x) (* x x))");
 *   double result = square(5.0);  // Returns 25.0
 *
 * On error: Function returns default-constructed R
 */
template<class Sig>
std::function<Sig> compile_fn(const std::string& code, bool expand_macros = true);

// Specialization for function signature parsing
namespace detail {
    template<class>
    struct fn_traits;

    template<class R, class... Args>
    struct fn_traits<R(Args...)> {
        using return_type = R;
        using function_type = std::function<R(Args...)>;
        static constexpr size_t arity = sizeof...(Args);

        static function_type make_function(scheme_fn& fn) {
            return fn.as_function<R, Args...>();
        }
    };
}

template<class Sig>
std::function<Sig> compile_fn(const std::string& code, bool expand_macros) {
    scheme_fn fn = compile_proc(code, expand_macros);
    return detail::fn_traits<Sig>::make_function(fn);
}

/**
 * Compile an expression as a typed thunk.
 *
 * @tparam Sig Function signature (must be R() form)
 * @param expr Scheme expression
 * @param expand_macros If true, perform Macro expansion
 * @return std::function<Sig> that evaluates the expression
 *
 * Example:
 *   auto compute = compile_thunk<double()>("(+ 3 4)");
 *   double result = compute();  // Returns 7.0
 */
template<class Sig>
std::function<Sig> compile_thunk(const std::string& expr, bool expand_macros = true) {
    scheme_fn fn = compile_thunk_proc(expr, expand_macros);
    return detail::fn_traits<Sig>::make_function(fn);
}

/**
 * Compile a multi-form program (evaluates as begin, requires last value is Procedure).
 *
 * @param program Scheme source with multiple forms
 *                Example: "(define (square x) (* x x)) square"
 * @param expand_macros If true, perform Macro expansion
 * @return scheme_fn wrapper for the Procedure result
 *
 * Semantics: Wraps as (begin <forms...>) and evaluates
 * Last value must be a Procedure, or error is returned
 */
scheme_fn compile_program_proc(const std::string& program, bool expand_macros = true);

/**
 * Helper: Build a Scheme argument list from C++ values.
 *
 * @tparam Args Argument types
 * @param args C++ values to convert
 * @return Proper Scheme list
 *
 * Example:
 *   Object* args = build_args(1, 2.0, "hello");
 *   // Returns: (1 . (2.0 . ("hello" . nil)))
 */
template<class... Args>
Object* build_args(Args... args);

} // namespace embedding

// ============================================================================
// Template implementations
// ============================================================================

namespace embedding {
namespace detail {
    inline Object* build_args_impl() {
        return nil;
    }

    template<class T, class... Rest>
    Object* build_args_impl(T first, Rest... rest) {
        Object* first_obj = value_codec<T>::to_scheme(first);
        Object* rest_list = build_args_impl(rest...);
        return cons(first_obj, rest_list);
    }
}

template<class... Args>
Object* build_args(Args... args) {
    return detail::build_args_impl(args...);
}

// ============================================================================
// Type-Safe Export/Import API
// ============================================================================

/**
 * @brief Export a C++ callable with compile-time type checking
 *
 * @tparam Sig Function signature (e.g., double(double, double))
 * @tparam Func Callable type (lambda, function pointer, etc.)
 * @param name Export name
 * @param func Callable to export
 * @param env Environment to register in (optional)
 * @param origin Origin string for provenance tracking
 * @param traits Traits string for metadata
 *
 * Example:
 *   auto cpp_square = [](double x) { return x * x; };
 *   export_checked<double(double)>("cpp-square", cpp_square, env);
 */
template<typename Sig, typename Func>
void export_checked(const std::string& name, Func&& func,
                    Environment* env = nullptr,
                    const std::string& origin = "cpp",
                    const std::string& traits = "native") {
    // Compile-time signature check
    static_assert(callable_matches_v<Sig, Func>,
                  "Callable does not match the specified signature");

    // Register signature
    constexpr size_t arity = function_traits<std::decay_t<Func>>::arity;
    SignatureRegistry::instance().register_signature(name, typeid(Sig).name(), arity);

    // Convert to std::function
    using fn_traits = detail::fn_traits<Sig>;
    auto std_fn = to_std_function(std::forward<Func>(func));

    // Wrap as Scheme Procedure
    Object* proc = value_codec<typename fn_traits::function_type>::to_scheme(std_fn);

    // Register in export registry
    scheme_fn wrapper(proc);
    ExportRegistry::instance().register_export(name, wrapper, origin, traits);

    // Optionally bind in environment
    if (env) {
        env->define(name, proc);
    }
}

/**
 * @brief Import a function from the export registry with type checking
 *
 * @tparam Sig Expected function signature
 * @param name Export name to import
 * @return std::function<Sig> or empty function if not found
 *
 * Example:
 *   auto scheme_add = import_checked<double(double, double)>("scheme-add");
 *   double result = scheme_add(3.0, 4.0);
 */
template<typename Sig>
std::function<Sig> import_checked(const std::string& name) {
    auto entry = ExportRegistry::instance().get(name);
    if (!entry || !entry->proc.is_valid()) {
        return std::function<Sig>();  // Empty function
    }

    // Validate arity if signature is registered
    constexpr size_t expected_arity = detail::fn_traits<Sig>::arity;
    if (!SignatureRegistry::instance().validate_arity(name, expected_arity)) {
        return std::function<Sig>();  // Arity mismatch
    }

    // Use make_function to properly extract and apply template args
    scheme_fn proc_copy = entry->proc;
    return detail::fn_traits<Sig>::make_function(proc_copy);
}

/**
 * @brief Dynamic call by name with typed return
 *
 * @tparam R Return type
 * @tparam Args Argument types
 * @param name Export name to call
 * @param args Arguments to pass
 * @return std::optional<R> with result, or nullopt if call failed
 *
 * Example:
 *   auto result = dynamic_call<double>("scheme-add", 3.0, 4.0);
 *   if (result) {
 *       std::cout << "Result: " << *result << "\n";
 *   }
 */
template<typename R, typename... Args>
std::optional<R> dynamic_call(const std::string& name, Args... args) {
    auto entry = ExportRegistry::instance().get(name);
    if (!entry || !entry->proc.is_valid()) {
        return std::nullopt;
    }

    // Build argument list
    Object* arg_list = build_args(args...);

    // Call the Procedure
    Object* result = entry->proc.call(arg_list);
    if (!result) {
        return std::nullopt;
    }

    // Convert result
    return value_codec<R>::from_scheme(result);
}

/**
 * @brief Dynamic call using an engine (for Macro expansion support)
 *
 * @tparam R Return type
 * @tparam Args Argument types
 * @param engine Scheme engine to use
 * @param name Function name (can be export name or defined in engine)
 * @param args Arguments to pass
 * @return std::optional<R> with result, or nullopt if call failed
 */
template<typename R, typename... Args>
std::optional<R> dynamic_call(scheme_engine* engine, const std::string& name, Args... args);

/**
 * @brief Export a Scheme expression as a typed C++ callable
 *
 * @tparam Sig Function signature
 * @param engine Scheme engine
 * @param name Export name
 * @param code Scheme code that evaluates to a Procedure
 * @param origin Origin string for provenance
 * @param traits Traits string for metadata
 * @return std::function<Sig> callable
 *
 * Example:
 *   auto scheme_mul = scheme_export<double(double, double)>(
 *       engine, "scheme-mul", "(lambda (a b) (* a b))");
 *   double result = scheme_mul(3.0, 4.0);  // Returns 12.0
 */
template<typename Sig>
std::function<Sig> scheme_export(scheme_engine* engine,
                                  const std::string& name,
                                  const std::string& code,
                                  const std::string& origin = "scheme",
                                  const std::string& traits = "") {
    // Compile the Scheme code
    scheme_fn fn = compile_proc(code);
    if (!fn.is_valid()) {
        return std::function<Sig>();
    }

    // Register signature
    using fn_traits = detail::fn_traits<Sig>;
    constexpr size_t arity = fn_traits::arity;
    SignatureRegistry::instance().register_signature(name, typeid(Sig).name(), arity);

    // Register in export registry
    ExportRegistry::instance().register_export(name, fn, origin, traits);

    // Return typed wrapper
    return fn_traits::make_function(fn);
}

// ============================================================================
// Convenience Macros
// ============================================================================

/**
 * @brief Macro to export a C++ function with automatic type deduction
 *
 * Usage:
 *   auto add = [](int a, int b) { return a + b; };
 *   EXPORT_FN("add", add, env);
 */
#define EXPORT_FN(name, func, env) \
    embedding::export_checked<decltype(to_std_function(func))::result_type( \
        /* args from traits */)>(name, func, env)

/**
 * @brief Macro to import a Scheme function as typed C++ callable
 *
 * Usage:
 *   auto scheme_add = IMPORT_FN(double(double, double), "scheme-add");
 */
#define IMPORT_FN(sig, name) \
    embedding::import_checked<sig>(name)

} // namespace embedding
