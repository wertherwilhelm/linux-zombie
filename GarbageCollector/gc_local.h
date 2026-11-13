#pragma once

#include "gc_context.h"
#include "gc_object.h"
#include <tuple>
#include <utility>

/**
 * @file gc_local.h
 * @brief Variadic local root guard with automatic registration/unregistration
 *
 * gc_local<T...> provides RAII-based root management for local variables.
 * It registers multiple local pointers as GC roots and automatically
 * unregisters them when the scope exits.
 *
 * Usage:
 *   void process() {
 *       auto root = gc_new<Node>(1);
 *       auto temp = gc_new<Node>(2);
 *
 *       gc_local locals{root, temp};  // Both rooted!
 *
 *       gc_context::current()->collect();  // Safe - locals won't be collected
 *
 *       // Auto-unroots on scope exit
 *   }
 *
 * With structured bindings (C++17):
 *   gc_local locals{root, temp};
 *   auto& [r, t] = locals;  // Access via references
 */

/**
 * @brief Variadic local root guard with automatic registration/unregistration.
 *
 * Registers multiple local pointer variables as GC roots for the lifetime of
 * the gc_local instance. The caller's pointer variables are updated in-place
 * if the GC moves objects.
 *
 * @tparam Ts GC-managed types (must derive from gc_object).
 */
template<typename... Ts>
class gc_local {
    static_assert((std::is_base_of_v<gc_object, Ts> && ...),
                  "gc_local<T...> requires all types to derive from gc_object");

    std::tuple<Ts**...> m_ptr_addrs;  // Store ADDRESSES of caller's variables
    IGarbageCollector* m_gc;

public:
    // ========================================================================
    // Constructor - Auto-register roots
    // ========================================================================

    /**
     * @brief Construct and register caller's pointer variables as GC roots
     * @param args LVALUE REFERENCES to pointers (caller's variables)
     *
     * CRITICAL: Moving GC updates the caller's variables directly!
     * This requires passing lvalue references, not rvalue temporaries.
     *
     * Correct usage:
     *   Node* root = gc_new<Node>(1);
     *   Node* left = gc_new<Node>(2);
     *   gc_local locals{root, left};  // Pass variables, not temporaries
     *   // After GC: root and left are automatically updated
     *
     * WRONG usage (won't compile):
     *   gc_local locals{gc_new<Node>(1), gc_new<Node>(2)};  // ERROR: rvalues
     *
     * For temporaries, use gc_root_guard (stores values, provides get()):
     *   gc_root_guard guard(gc_new<Node>(1));
     *   auto* root = guard.get();  // Access through guard
     */
    explicit gc_local(Ts*&... args)  // Take lvalue references!
        : m_ptr_addrs(&args...)       // Store addresses of caller's variables
        , m_gc(gc_context::current())
    {
        if (!m_gc) {
            // No active GC - this is fine, just don't register roots
            return;
        }

        // Register each caller's variable address using fold expression
        std::apply([this](auto... addrs) {
            (m_gc->register_persistent_root(
                reinterpret_cast<gc_object**>(addrs)
            ), ...);
        }, m_ptr_addrs);
    }

    // ========================================================================
    // Destructor - Auto-unregister roots
    // ========================================================================

    /**
     * @brief Unregister roots on scope exit.
     */
    ~gc_local() {
        if (!m_gc) return;

        // Unregister each caller's variable address
        std::apply([this](auto... addrs) {
            (m_gc->unregister_persistent_root(
                reinterpret_cast<gc_object**>(addrs)
            ), ...);
        }, m_ptr_addrs);
    }

    // ========================================================================
    // No Copy/Move (roots are tied to stack location)
    // ========================================================================

    gc_local(const gc_local&) = delete;
    gc_local(gc_local&&) = delete;
    gc_local& operator=(const gc_local&) = delete;
    gc_local& operator=(gc_local&&) = delete;

    // ========================================================================
    // Accessors
    // ========================================================================

    /**
     * @brief Get reference to caller's pointer by index
     * Usage: auto* ptr = locals.get<0>();  // Returns caller's variable
     *
     * NOTE: This returns a reference to the caller's variable, which
     * has been updated by moving GC if objects were relocated.
     */
    template<size_t I>
    auto& get() {
        return *std::get<I>(m_ptr_addrs);  // Dereference address
    }

    /**
     * @brief Get reference to caller's pointer by index (const).
     */
    template<size_t I>
    const auto& get() const {
        return *std::get<I>(m_ptr_addrs);  // Dereference address
    }

    /**
     * @brief Get reference to caller's pointer by type (only works if all types are distinct)
     * Usage: auto* ptr = locals.get<Node>();
     */
    template<typename T>
    T*& get() {
        return *std::get<T**>(m_ptr_addrs);  // Dereference address
    }

    /**
     * @brief Get reference to caller's pointer by type (const).
     */
    template<typename T>
    T* const& get() const {
        return *std::get<T**>(m_ptr_addrs);  // Dereference address
    }

    // ========================================================================
    // Tuple Interface (for structured bindings)
    // ========================================================================

    /**
     * @brief Support structured bindings
     *
     * Usage:
     *   Node* root = nullptr;
     *   Node* left = nullptr;
     *   gc_local locals{root, left};
     *   auto& [r, l] = locals;  // r and l are references to root and left
     *   r = gc_new<Node>(1);    // Updates root directly
     */
    template<size_t I>
    auto& get_ref() {
        return *std::get<I>(m_ptr_addrs);  // Dereference address
    }

    /**
     * @brief Get reference to caller's pointer by index (const).
     */
    template<size_t I>
    const auto& get_ref() const {
        return *std::get<I>(m_ptr_addrs);  // Dereference address
    }

    // ========================================================================
    // Utility
    // ========================================================================

    /**
     * @brief Check if GC is active for these locals
     */
    bool has_gc() const { return m_gc != nullptr; }

    /**
     * @brief Get the GC instance managing these roots
     */
    IGarbageCollector* gc() const { return m_gc; }

    /**
     * @brief Get count of managed roots
     */
    static constexpr size_t size() { return sizeof...(Ts); }
};

// ============================================================================
// Deduction Guide (C++17)
// ============================================================================

/**
 * @brief Allow template argument deduction from constructor arguments
 *
 * This enables:
 *   Node* root = nullptr;
 *   Node* left = nullptr;
 *   gc_local locals{root, left};  // Deduces gc_local<Node, Node>
 */
template<typename... Ts>
gc_local(Ts*&...) -> gc_local<Ts...>;

// ============================================================================
// Structured Binding Support (C++17)
// ============================================================================

/**
 * Specialize std::tuple_size and std::tuple_element to enable structured bindings
 */
namespace std {
    template<typename... Ts>
    struct tuple_size<gc_local<Ts...>> : integral_constant<size_t, sizeof...(Ts)> {};

    template<size_t I, typename... Ts>
    struct tuple_element<I, gc_local<Ts...>> {
        using type = decltype(declval<gc_local<Ts...>>().get_ref<I>());
    };
}

// ============================================================================
// Convenience Helpers
// ============================================================================

/**
 * @brief Create a gc_local with type deduction
 *
 * Usage:
 *   Node* root = nullptr;
 *   Node* left = nullptr;
 *   auto locals = make_gc_local(root, left);  // Takes lvalue refs
 *
 * NOTE: Prefer direct construction (gc_local locals{root, left}) over
 * this helper, as the deduction guide makes it unnecessary.
 */
template<typename... Ts>
gc_local<Ts...> make_gc_local(Ts*&... args) {
    return gc_local<Ts...>(args...);
}

/**
 * @brief Single-pointer root guard
 *
 * NOTE: gc_root_guard is defined in gc_manager.h, not here.
 * The existing gc_root_guard stores the pointer value (not a reference),
 * which is appropriate for protecting single temporaries:
 *
 *   gc_root_guard guard(gc_new<Node>(1));  // Stores value
 *   auto* ptr = guard.get();               // Access through guard
 *
 * For multiple pointers with lvalue reference semantics, use gc_local:
 *
 *   Node* root = gc_new<Node>(1);
 *   Node* left = gc_new<Node>(2);
 *   gc_local locals{root, left};  // Protects caller's variables
 */
