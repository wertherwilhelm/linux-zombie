#pragma once

#include "Object.h"
#include "atom.h"
#include "cons_cell.h"
#include "numeric_runtime.h"
#include "procedure.h"
#include "gc_ptr.h"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <tuple>
#include <type_traits>
#include <memory>

// Use types from zombie::runtime namespace
using zombie::runtime::Object;
using zombie::runtime::Environment;
using zombie::runtime::IEvaluator;
using zombie::runtime::Procedure;
using zombie::runtime::nil;
using zombie::runtime::t;
using zombie::runtime::false_obj;
using zombie::runtime::cons;
using zombie::runtime::car;
using zombie::runtime::cdr;
using zombie::runtime::set_rest;
using zombie::runtime::a_string;
using zombie::runtime::a_double;
using zombie::runtime::a_symbol;
template<typename T>
using atom = zombie::runtime::atom<T>;

/**
 * @file value_codec.h
 * @brief Bidirectional marshalling between C++ and Scheme types.
 */

/**
 * @brief Bidirectional marshalling between C++ and Scheme types.
 *
 * Provides conversion functions:
 *   to_scheme(T)   : C++ -> Scheme
 *   from_scheme(obj) : Scheme -> C++
 *
 * Specializations provided for:
 * - Numeric types: int, long, double
 * - bool
 * - std::string
 * - Object* (passthrough)
 * - std::vector<T> (Scheme lists)
 *
 * Custom types can add specializations:
 * @code
 * template<>
 * struct value_codec<MyType> {
 *     static Object* to_scheme(const MyType& val) { ... }
 *     static MyType from_scheme(Object* obj) { ... }
 * };
 * @endcode
 *
 * @tparam T C++ type to marshal.
 */

// Primary template (undefined - forces specialization)
template<class T>
struct value_codec;

// ============================================================================
// Numeric types
// ============================================================================

#define ZOMBIE_DEFINE_INTEGRAL_CODEC(Type) \
template<> \
struct value_codec<Type> { \
    static Object* to_scheme(Type val) { \
        return numeric::make_integer(static_cast<int64_t>(val)); \
    } \
    static Type from_scheme(Object* obj) { \
        return static_cast<Type>(numeric::to_int64(obj)); \
    } \
};

template<>
struct value_codec<int> {
    static Object* to_scheme(int val) {
        return numeric::make_integer(static_cast<int64_t>(val));
    }

    static int from_scheme(Object* obj) {
        return static_cast<int>(numeric::to_int64(obj));
    }
};

template<>
struct value_codec<long> {
    static Object* to_scheme(long val) {
        return numeric::make_integer(static_cast<int64_t>(val));
    }

    static long from_scheme(Object* obj) {
        return static_cast<long>(numeric::to_int64(obj));
    }
};

template<>
struct value_codec<double> {
    static Object* to_scheme(double val) {
        return numeric::make_inexact(val);
    }

    static double from_scheme(Object* obj) {
        return numeric::to_double(obj);
    }
};

template<>
struct value_codec<float> {
    static Object* to_scheme(float val) {
        return numeric::make_inexact(static_cast<double>(val));
    }

    static float from_scheme(Object* obj) {
        return static_cast<float>(numeric::to_double(obj));
    }
};

ZOMBIE_DEFINE_INTEGRAL_CODEC(short)
ZOMBIE_DEFINE_INTEGRAL_CODEC(unsigned short)
ZOMBIE_DEFINE_INTEGRAL_CODEC(unsigned int)
ZOMBIE_DEFINE_INTEGRAL_CODEC(unsigned long)
ZOMBIE_DEFINE_INTEGRAL_CODEC(long long)
ZOMBIE_DEFINE_INTEGRAL_CODEC(unsigned long long)
ZOMBIE_DEFINE_INTEGRAL_CODEC(signed char)
ZOMBIE_DEFINE_INTEGRAL_CODEC(unsigned char)
#undef ZOMBIE_DEFINE_INTEGRAL_CODEC

// ============================================================================
// Boolean
// ============================================================================

template<>
struct value_codec<bool> {
    static Object* to_scheme(bool val) {
        return gc_new<atom<double>>(val ? 1.0 : 0.0);
    }

    static bool from_scheme(Object* obj) {
        if (!obj || obj == nil) return false;
        a_double* d = dynamic_cast<a_double*>(obj);
        if (d) return d->value() != 0.0;
        return true;  // Non-nil is truthy
    }
};

// ============================================================================
// String
// ============================================================================

template<>
struct value_codec<std::string> {
    static Object* to_scheme(const std::string& val) {
        return gc_new<a_string>(val);
    }

    static std::string from_scheme(Object* obj) {
        if (!obj) return "";
        a_string* s = dynamic_cast<a_string*>(obj);
        if (s) return s->value();
        return "";
    }
};

template<>
struct value_codec<const char*> {
    static Object* to_scheme(const char* val) {
        return gc_new<a_string>(val ? val : "");
    }

    static const char* from_scheme(Object* obj) {
        // WARNING: Returns pointer to internal string - use with care!
        if (!obj) return "";
        a_string* s = dynamic_cast<a_string*>(obj);
        if (s) return s->value().c_str();
        return "";
    }
};

// ============================================================================
// Object* passthrough (unsafe/advanced mode)
// ============================================================================

template<>
struct value_codec<Object*> {
    static Object* to_scheme(Object* val) {
        return val;
    }

    static Object* from_scheme(Object* obj) {
        return obj;
    }
};

// ============================================================================
// std::vector<T> ↔ Scheme list
// ============================================================================

template<class T>
struct value_codec<std::vector<T>> {
    static Object* to_scheme(const std::vector<T>& vec) {
        Object* result = nil;
        Object* tail = nil;

        for (const auto& elem : vec) {
            Object* scheme_elem = value_codec<T>::to_scheme(elem);
            Object* new_cell = cons(scheme_elem, nil);

            if (result == nil) {
                result = new_cell;
                tail = new_cell;
            } else {
                set_rest(tail, new_cell);
                tail = new_cell;
            }
        }

        return result;
    }

    static std::vector<T> from_scheme(Object* obj) {
        std::vector<T> result;

        Object* current = obj;
        while (current && !current->is_nil() && !current->is_atom()) {
            Object* elem = car(current);
            result.push_back(value_codec<T>::from_scheme(elem));
            current = cdr(current);
        }

        return result;
    }
};

// ============================================================================
// void (for procedures with no return value)
// ============================================================================

template<>
struct value_codec<void> {
    static Object* to_scheme() {
        return nil;  // void returns nil
    }

    static void from_scheme(Object* obj) {
        // Discard result
        (void)obj;
    }
};

// ============================================================================
// std::function<R(Args...)> ↔ Scheme Procedure
// ============================================================================

// Forward declarations needed for std::function codec
class scheme_fn;
class Procedure;
class IEvaluator;
class Environment;

namespace detail {

/**
 * @brief Helper to call a std::function from Scheme with argument unpacking
 *
 * This creates a bridge Procedure that:
 * 1. Extracts typed arguments from Scheme list
 * 2. Calls the C++ std::function
 * 3. Converts result back to Scheme
 */
template<typename R, typename... Args, size_t... I>
R call_std_function_impl(
    const std::function<R(Args...)>& fn,
    Object* arg_list,
    std::index_sequence<I...>)
{
    // Build tuple of converted arguments
    std::tuple<Args...> args;

    Object* current = arg_list;
    size_t index = 0;
    auto extract_arg = [&current, &index](auto& arg) {
        using ArgType = std::decay_t<decltype(arg)>;
        if (current && !current->is_nil()) {
            arg = value_codec<ArgType>::from_scheme(car(current));
            current = cdr(current);
        }
        ++index;
    };

    // Extract each argument (fold expression)
    (extract_arg(std::get<I>(args)), ...);

    // Call the function with unpacked arguments
    if constexpr (std::is_void_v<R>) {
        fn(std::get<I>(args)...);
    } else {
        return fn(std::get<I>(args)...);
    }
}

/**
 * @brief Wrapper Procedure that holds a std::function and bridges to Scheme
 */
/**
 * @brief Wrapper Procedure that holds a std::function and bridges to Scheme.
 */
template<typename R, typename... Args>
class std_function_wrapper : public Procedure {
public:
    explicit std_function_wrapper(std::function<R(Args...)> fn)
        : fn_(std::move(fn)) {}

    Object* apply(IEvaluator* evaluator, Object* args, Environment* env) override {
        (void)evaluator;
        (void)env;

        if constexpr (std::is_void_v<R>) {
            call_std_function_impl(fn_, args, std::index_sequence_for<Args...>{});
            return nil;
        } else {
            R result = call_std_function_impl(fn_, args, std::index_sequence_for<Args...>{});
            return value_codec<R>::to_scheme(result);
        }
    }

    std::string to_string() const override {
        return "#<cpp-function>";
    }

    bool pair() const override { return false; }

private:
    std::function<R(Args...)> fn_;
};

} // namespace detail

/**
 * @brief Codec for std::function<R(Args...)>
 *
 * Enables passing C++ functions to Scheme as first-class procedures,
 * and receiving Scheme procedures as typed C++ functions.
 *
 * Usage:
 *   std::function<double(double)> cpp_fn = [](double x) { return x * 2; };
 *   Object* scheme_proc = value_codec<std::function<double(double)>>::to_scheme(cpp_fn);
 *   // scheme_proc can now be called from Scheme: (cpp_fn 5.0) → 10.0
 */
template<typename R, typename... Args>
struct value_codec<std::function<R(Args...)>> {
    using function_type = std::function<R(Args...)>;

    /**
     * @brief Convert C++ std::function to Scheme Procedure
     */
    static Object* to_scheme(const function_type& fn) {
        if (!fn) {
            return nil;  // Empty function → nil
        }
        return gc_new<detail::std_function_wrapper<R, Args...>>(fn);
    }

    /**
     * @brief Convert Scheme Procedure to C++ std::function
     *
     * The returned function captures the Scheme Procedure and calls it
     * when invoked, converting arguments and return value automatically.
     */
    static function_type from_scheme(Object* obj) {
        if (!obj || obj->is_nil()) {
            return function_type();  // Empty function
        }

        // Check if it's already a wrapped C++ function
        auto* wrapper = dynamic_cast<detail::std_function_wrapper<R, Args...>*>(obj);
        if (wrapper) {
            // Unwrap directly (avoids double-wrapping)
            // Note: We can't access fn_ directly, so we still wrap
        }

        // Wrap the Scheme Procedure in a C++ lambda
        // Use gc_ptr to protect the Procedure from GC (copyable, refcounted)
        auto root = std::make_shared<gc_ptr<Object>>(static_cast<Object*>(obj));

        return [root](Args... args) -> R {
            Object* proc = root->get();
            if (!proc) {
                if constexpr (std::is_void_v<R>) {
                    return;
                } else {
                    return R();
                }
            }

            // Build argument list
            Object* arg_list = nil;
            Object* tail = nil;

            auto append_arg = [&](auto arg) {
                using ArgType = std::decay_t<decltype(arg)>;
                Object* scheme_arg = value_codec<ArgType>::to_scheme(arg);
                Object* cell = cons(scheme_arg, nil);
                if (arg_list == nil) {
                    arg_list = cell;
                    tail = cell;
                } else {
                    set_rest(tail, cell);
                    tail = cell;
                }
            };

            (append_arg(args), ...);

            // Call the Scheme Procedure
            Procedure* p = dynamic_cast<Procedure*>(proc);
            if (!p) {
                if constexpr (std::is_void_v<R>) {
                    return;
                } else {
                    return R();
                }
            }

            Object* result = p->apply(nullptr, arg_list, nullptr);

            if constexpr (std::is_void_v<R>) {
                return;
            } else {
                return value_codec<R>::from_scheme(result);
            }
        };
    }
};
