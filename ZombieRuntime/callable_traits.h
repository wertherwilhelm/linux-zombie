#pragma once

/**
 * @file callable_traits.h
 * @brief Generic function type introspection for any callable
 *
 * Provides compile-time extraction of return types and argument types
 * from any callable: function pointers, lambdas, functors, member functions,
 * and std::function.
 *
 * Key components:
 * - function_traits<T>: Extract return_type, args tuple, arity
 * - callable_matches<Sig, Func>: Check if callable matches signature
 * - to_std_function(func): Convert any callable to std::function
 * - is_callable<T>: SFINAE-friendly callable detection
 *
 * Usage:
 *   auto lambda = [](int x, double y) { return x + y; };
 *
 *   using traits = function_traits<decltype(lambda)>;
 *   // traits::return_type = double
 *   // traits::arity = 2
 *   // traits::arg<0> = int
 *   // traits::arg<1> = double
 *
 *   auto fn = to_std_function(lambda);
 *   // fn is std::function<double(int, double)>
 */

#include <functional>
#include <tuple>
#include <type_traits>

// ============================================================================
// Function Traits - Extract Types from Callables
// ============================================================================

/**
 * @brief Primary template - undefined for non-callables
 */
template<typename T>
struct function_traits;

/**
 * @brief Specialization for function pointers
 */
template<typename R, typename... Args>
struct function_traits<R(*)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);

    template<size_t N>
    using arg = typename std::tuple_element<N, args_tuple>::type;

    using function_type = std::function<R(Args...)>;
};

/**
 * @brief Specialization for function references
 */
template<typename R, typename... Args>
struct function_traits<R(&)(Args...)> : function_traits<R(*)(Args...)> {};

/**
 * @brief Specialization for std::function
 */
template<typename R, typename... Args>
struct function_traits<std::function<R(Args...)>> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);

    template<size_t N>
    using arg = typename std::tuple_element<N, args_tuple>::type;

    using function_type = std::function<R(Args...)>;
};

/**
 * @brief Specialization for member function pointers (const)
 */
template<typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...) const> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);

    template<size_t N>
    using arg = typename std::tuple_element<N, args_tuple>::type;

    using function_type = std::function<R(Args...)>;
    using class_type = C;
};

/**
 * @brief Specialization for member function pointers (non-const)
 */
template<typename C, typename R, typename... Args>
struct function_traits<R(C::*)(Args...)> {
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);

    template<size_t N>
    using arg = typename std::tuple_element<N, args_tuple>::type;

    using function_type = std::function<R(Args...)>;
    using class_type = C;
};

/**
 * @brief Specialization for lambdas and functors (via operator())
 *
 * This captures any class with operator() by examining &T::operator()
 */
template<typename T>
struct function_traits : function_traits<decltype(&T::operator())> {};

// ============================================================================
// Callable Detection - SFINAE-friendly
// ============================================================================

namespace detail {

template<typename T, typename = void>
struct is_callable_impl : std::false_type {};

template<typename T>
struct is_callable_impl<T, std::void_t<decltype(&T::operator())>> : std::true_type {};

template<typename R, typename... Args>
struct is_callable_impl<R(*)(Args...)> : std::true_type {};

template<typename R, typename... Args>
struct is_callable_impl<R(&)(Args...)> : std::true_type {};

template<typename R, typename... Args>
struct is_callable_impl<std::function<R(Args...)>> : std::true_type {};

} // namespace detail

/**
 * @brief Check if T is a callable type
 */
template<typename T>
struct is_callable : detail::is_callable_impl<std::decay_t<T>> {};

template<typename T>
constexpr bool is_callable_v = is_callable<T>::value;

// ============================================================================
// Signature Matching
// ============================================================================

/**
 * @brief Check if a callable matches a specific signature
 *
 * Usage:
 *   auto lambda = [](int x) { return x * 2; };
 *   static_assert(callable_matches<int(int), decltype(lambda)>::value);
 */
template<typename Sig, typename Func>
struct callable_matches;

template<typename R, typename... Args, typename Func>
struct callable_matches<R(Args...), Func> {
private:
    using traits = function_traits<std::decay_t<Func>>;

    static constexpr bool return_matches =
        std::is_same_v<typename traits::return_type, R> ||
        std::is_convertible_v<typename traits::return_type, R>;

    static constexpr bool args_match =
        std::is_same_v<typename traits::args_tuple, std::tuple<Args...>>;

public:
    static constexpr bool value = return_matches && args_match;
};

template<typename Sig, typename Func>
constexpr bool callable_matches_v = callable_matches<Sig, Func>::value;

// ============================================================================
// Conversion to std::function
// ============================================================================

/**
 * @brief Convert any callable to std::function with deduced signature
 *
 * Usage:
 *   auto lambda = [](int x) { return x * 2; };
 *   auto fn = to_std_function(lambda);
 *   // fn is std::function<int(int)>
 */
template<typename Func>
auto to_std_function(Func&& func) {
    using traits = function_traits<std::decay_t<Func>>;
    using function_type = typename traits::function_type;
    return function_type(std::forward<Func>(func));
}

// ============================================================================
// Signature String Generation (for debugging/registration)
// ============================================================================

namespace detail {

template<typename T>
constexpr const char* type_name_impl() {
    if constexpr (std::is_same_v<T, int>) return "int";
    else if constexpr (std::is_same_v<T, long>) return "long";
    else if constexpr (std::is_same_v<T, long long>) return "int64";
    else if constexpr (std::is_same_v<T, int64_t>) return "int64";
    else if constexpr (std::is_same_v<T, double>) return "double";
    else if constexpr (std::is_same_v<T, float>) return "float";
    else if constexpr (std::is_same_v<T, bool>) return "bool";
    else if constexpr (std::is_same_v<T, std::string>) return "string";
    else if constexpr (std::is_same_v<T, void>) return "void";
    else if constexpr (std::is_pointer_v<T>) return "ptr";
    else return "unknown";
}

} // namespace detail

/**
 * @brief Get a human-readable type name (for simple types)
 */
template<typename T>
constexpr const char* type_name() {
    return detail::type_name_impl<std::decay_t<T>>();
}

// ============================================================================
// Arity Extraction Helpers
// ============================================================================

/**
 * @brief Get arity (parameter count) of a callable at compile time
 */
template<typename Func>
constexpr size_t callable_arity() {
    return function_traits<std::decay_t<Func>>::arity;
}

/**
 * @brief Get arity of a callable (runtime version)
 */
template<typename Func>
size_t get_arity(const Func&) {
    return callable_arity<Func>();
}
