#pragma once

/**
 * @file mirror.h
 * @brief Cross-language mirrors - seamless C++ ↔ Scheme interop
 *
 * Mirrors are the bridges between realities. They let you:
 * - Wrap C++ functions so Scheme can call them
 * - Wrap Scheme lambdas so C++ can call them
 * - No manual marshalling, no FFI boilerplate!
 *
 * Philosophy:
 * - Mirrors are bidirectional (C++ ↔ Scheme)
 * - Type conversions are automatic (Value ↔ native types)
 * - Mirrors have full provenance (tracked in ledger)
 * - Mirrors can cross multiple backends (Scheme → Native → Bytecode)
 *
 * Example:
 *   // C++ → Scheme mirror
 *   auto cpp_fn = [](int x) { return x * 2; };
 *   Value mirror = make_native_mirror(cpp_fn);
 *   // Now Scheme can call it: (mirror 21) => 42
 *
 *   // Scheme → C++ mirror
 *   Object* scheme_fn = parse("(lambda (x) (+ x 1))");
 *   Value mirror = make_scheme_mirror(scheme_fn);
 *   // Now C++ can call it: mirror.invoke({21}) => 22
 */

#include "value.h"
#include "ledger.h"
#include <functional>
#include <memory>
#include <type_traits>
#include <stdexcept>

// Forward declarations
namespace zombie { namespace runtime {
class Object;
class Environment;
}}
using zombie::runtime::Object;
using zombie::runtime::Environment;

// ============================================================================
// Type Conversion Traits - Automatic Marshalling
// ============================================================================

/**
 * @brief Convert C++ type → void* for Value system
 *
 * This enables automatic type conversion:
 * - int → void* (box as Object*)
 * - double → void* (box as Object*)
 * - Object* → void* (pass through)
 * - Value → void* (extract ptr)
 */
template<typename T>
struct ToValue {
    // Default: assume it's already a pointer type
    static void* convert(T value) {
        return static_cast<void*>(value);
    }
};

// Specialization for Object* (Scheme values)
template<>
struct ToValue<Object*> {
    static void* convert(Object* obj) {
        return static_cast<void*>(obj);
    }
};

/**
 * @brief Convert void* → C++ type
 */
template<typename T>
struct FromValue {
    // Default: assume it's a pointer type
    static T convert(void* ptr) {
        return static_cast<T>(ptr);
    }
};

// Specialization for Object*
template<>
struct FromValue<Object*> {
    static Object* convert(void* ptr) {
        return static_cast<Object*>(ptr);
    }
};

// ============================================================================
// Native Mirror - Wrap C++ Functions for Scheme
// ============================================================================

/**
 * @brief Wrapper for native C++ functions
 *
 * This allows Scheme code to call C++ functions seamlessly:
 *
 * C++ side:
 *   auto add = [](int a, int b) { return a + b; };
 *   Value mirror = make_native_mirror(add);
 *
 * Scheme side:
 *   (define native-add mirror)
 *   (native-add 2 3)  ; => 5
 */
template<typename Fn>
class NativeMirror {
public:
    explicit NativeMirror(Fn fn) : fn_(std::move(fn)) {}

    // Invoke the wrapped function
    // Note: This is a simplified version - real implementation needs
    // proper argument count checking and type conversion
    void* invoke(const std::vector<void*>& args) {
        // TODO: Implement proper variadic argument unpacking
        // For now, this is a placeholder
        throw std::runtime_error("NativeMirror::invoke not yet implemented");
    }

    Fn& function() { return fn_; }
    const Fn& function() const { return fn_; }

private:
    Fn fn_;
};

// ============================================================================
// Scheme Mirror - Wrap Scheme Functions for C++
// ============================================================================

/**
 * @brief Wrapper for Scheme procedures
 *
 * This allows C++ code to call Scheme lambdas seamlessly:
 *
 * Scheme side:
 *   (define my-fn (lambda (x) (+ x 1)))
 *
 * C++ side:
 *   Value mirror = make_scheme_mirror(my_fn);
 *   void* result = mirror.invoke({21});  // Calls Scheme fn with arg 21
 */
class SchemeMirror {
public:
    explicit SchemeMirror(Object* proc, Environment* env = nullptr);

    // Invoke the Scheme Procedure
    void* invoke(const std::vector<void*>& args);

    Object* Procedure() const { return proc_; }
    Environment* environment() const { return env_; }

private:
    Object* proc_;         // The Scheme Procedure
    Environment* env_;      // Lexical environment (if needed)
};

// ============================================================================
// Mirror VTables - Behavior Implementations
// ============================================================================

/**
 * @brief VTable for native mirrors (C++ functions)
 */
extern const ValueVTable native_mirror_vtable;

/**
 * @brief VTable for Scheme mirrors (Scheme procedures)
 */
extern const ValueVTable scheme_mirror_vtable;

// ============================================================================
// Mirror Factory Functions
// ============================================================================

/**
 * @brief Create a mirror for a C++ function
 *
 * Example:
 *   auto square = [](double x) { return x * x; };
 *   Value mirror = make_native_mirror(square);
 *
 *   // Can now be called from Scheme:
 *   // (mirror 5.0) => 25.0
 *
 * @param fn C++ callable (function, lambda, functor)
 * @return Value wrapping the function as a mirror
 */
template<typename Fn>
Value make_native_mirror(Fn fn) {
    // Allocate mirror on heap (must outlive Value)
    auto* mirror = new NativeMirror<Fn>(std::move(fn));

    Value v;
    v.ptr = static_cast<void*>(mirror);
    v.vt = &native_mirror_vtable;
    v.ledger = LedgerEntry(
        BackendTag::Native,
        "native-mirror",
        "callable mirror native"
    );

    return v;
}

/**
 * @brief Create a mirror for a Scheme Procedure
 *
 * Example (from C++):
 *   Object* scheme_fn = parse_and_eval("(lambda (x) (+ x 1))");
 *   Value mirror = make_scheme_mirror(scheme_fn);
 *
 *   // Can now be called from C++:
 *   void* result = mirror.invoke({make_int(21)});  // Returns 22
 *
 * @param proc Scheme Procedure (Closure, Primitive, etc.)
 * @param env Optional environment (for closures)
 * @return Value wrapping the Procedure as a mirror
 */
Value make_scheme_mirror(Object* proc, Environment* env = nullptr);

// ============================================================================
// Type-Safe Mirror Builders (Advanced)
// ============================================================================

/**
 * @brief Type-safe wrapper for common function signatures
 *
 * This provides compile-time type checking for mirrors:
 *
 * Example:
 *   TypedMirror<int(int, int)> add_mirror(
 *       [](int a, int b) { return a + b; }
 *   );
 *
 *   int result = add_mirror.call(2, 3);  // Type-safe!
 */
template<typename Signature>
class TypedMirror;

// Specialization for function signatures
template<typename R, typename... Args>
class TypedMirror<R(Args...)> {
public:
    using FnType = std::function<R(Args...)>;

    explicit TypedMirror(FnType fn)
        : fn_(std::move(fn))
        , mirror_(make_native_mirror(fn_))
    {}

    // Type-safe call
    R call(Args... args) {
        return fn_(args...);
    }

    // Get underlying Value (for passing to Scheme)
    const Value& as_value() const { return mirror_; }

private:
    FnType fn_;
    Value mirror_;
};

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * @brief Check if a Value is a mirror (callable cross-language wrapper)
 */
inline bool is_mirror(const Value& v) {
    return has_trait(v, "mirror") && v.is_callable();
}

/**
 * @brief Check if mirror is native (C++ → Scheme)
 */
inline bool is_native_mirror(const Value& v) {
    return is_mirror(v) && is_backend(v, BackendTag::Native);
}

/**
 * @brief Check if mirror is Scheme (Scheme → C++)
 */
inline bool is_scheme_mirror(const Value& v) {
    return is_mirror(v) && is_backend(v, BackendTag::Scheme);
}

// ============================================================================
// Mirror Registry (Optional)
// ============================================================================

/**
 * @brief Global registry for named mirrors
 *
 * This lets you register mirrors by name for easy lookup:
 *
 * Example:
 *   register_mirror("math.square", make_native_mirror([](double x) { return x*x; }));
 *
 *   // Later, from Scheme:
 *   (define square (lookup-mirror "math.square"))
 *   (square 5)  ; => 25
 */
class MirrorRegistry {
public:
    void register_mirror(const std::string& name, const Value& mirror) {
        mirrors_[name] = mirror;
    }

    const Value* lookup(const std::string& name) const {
        auto it = mirrors_.find(name);
        return it == mirrors_.end() ? nullptr : &it->second;
    }

    bool has_mirror(const std::string& name) const {
        return mirrors_.find(name) != mirrors_.end();
    }

    std::vector<std::string> list_mirrors() const {
        std::vector<std::string> names;
        names.reserve(mirrors_.size());
        for (const auto& pair : mirrors_) {
            names.push_back(pair.first);
        }
        return names;
    }

private:
    std::unordered_map<std::string, Value> mirrors_;
};

// Global mirror registry
extern MirrorRegistry* g_mirrors;

inline void init_global_mirrors() {
    if (!g_mirrors) {
        g_mirrors = new MirrorRegistry();
    }
}

inline void shutdown_global_mirrors() {
    if (g_mirrors) {
        delete g_mirrors;
        g_mirrors = nullptr;
    }
}
