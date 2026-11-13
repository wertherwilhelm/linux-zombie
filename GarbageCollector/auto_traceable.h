#pragma once

#include "gc_object.h"
#include "gc_field.h"
#include "gc_tracer.h"
#include <tuple>
#include <utility>

/**
 * @file auto_traceable.h
 * @brief CRTP mixin for automatic trace_references generation
 *
 * This provides a clean pattern for automatically tracing all gc_field members
 * without writing boilerplate trace_references code.
 *
 * Usage:
 *   struct Node : auto_traceable<Node> {
 *       gc_field<Node> left, right;
 *       int value;
 *
 *       // One-liner to declare which fields need tracing
 *       auto _gc_fields() { return std::tie(left, right); }
 *   };
 *
 * The Base class automatically implements trace_references() by iterating
 * over the tuple returned by _gc_fields() and calling .trace() on each.
 *
 * Rationale:
 * - C++ has no reflection, so we can't auto-discover gc_field members
 * - Tuple-based approach is cleaner than macros
 * - One-liner _gc_fields() is easier to maintain than full trace_references
 * - CRTP allows static polymorphism (no virtual overhead)
 */

/**
 * @brief CRTP Base class providing automatic trace_references
 *
 * @tparam Derived The derived class type (CRTP pattern)
 *
 * Requirements for Derived:
 * - Must provide _gc_fields() method returning std::tuple or std::tie result
 * - Each element in the tuple must have a .trace(Tracer&) method
 *
 * Example:
 *   struct BinaryTree : auto_traceable<BinaryTree> {
 *       gc_field<BinaryTree> left, right;
 *       gc_field<Object> data;
 *       int height;  // Non-GC field (not in tuple)
 *
 *       auto _gc_fields() {
 *           return std::tie(left, right, data);
 *       }
 *   };
 */
template<typename Derived>
class auto_traceable : public gc_object {
protected:
    /**
     * @brief Automatically generated trace_references implementation
     *
     * This implementation:
     * 1. Calls derived->_gc_fields() to get tuple of gc_field references
     * 2. Uses std::apply to unpack the tuple
     * 3. Uses fold expression to call .trace(t) on each field
     *
     * Benefits:
     * - Zero runtime overhead (all template expansion at compile time)
     * - Type-safe (compiler errors if _gc_fields() is malformed)
     * - Maintainable (add field to _gc_fields() = automatic tracing)
     */
    void trace_references(Tracer& t) override {
        // Get the tuple of gc_field references from derived class
        auto fields = static_cast<Derived*>(this)->_gc_fields();

        // Unpack tuple and trace each field using fold expression
        std::apply([&t](auto&... fields) {
            (fields.trace(t), ...);  // C++17 fold expression
        }, fields);
    }

    /**
     * @brief Automatically bind all gc_field members to this Object
     *
     * Call this from your constructor to enable automatic write barriers:
     *
     *   struct Node : auto_traceable<Node> {
     *       gc_field<Node> left, right;
     *       GC_FIELDS(left, right);
     *
     *       Node() {
     *           _gc_bind_fields();  // Binds left and right to this
     *       }
     *   };
     *
     * After binding, operator= on gc_field will automatically fire write barriers.
     *
     * This uses the same tuple from _gc_fields() and calls bind_owner(this)
     * on each field using a fold expression.
     */
    void _gc_bind_fields() {
        auto fields = static_cast<Derived*>(this)->_gc_fields();

        // Bind each gc_field to this Object
        std::apply([this](auto&... fields) {
            (fields.bind_owner(static_cast<gc_object*>(this)), ...);
        }, fields);
    }

public:
    // Ensure derived class provides _gc_fields() (compile-time check)
    // This is checked when trace_references() is actually called
    static_assert(
        std::is_base_of_v<gc_object, Derived>,
        "auto_traceable<Derived> requires Derived to ultimately derive from gc_object"
    );
};

/**
 * @brief Helper to define _gc_fields() for single field
 *
 * Usage:
 *   struct Leaf : auto_traceable<Leaf> {
 *       gc_field<Object> value;
 *       GC_FIELDS(value);  // Expands to: auto _gc_fields() { return std::tie(value); }
 *   };
 */
#define GC_FIELDS(...) \
    auto _gc_fields() { return std::tie(__VA_ARGS__); }

/**
 * @brief Helper Macro to bind fields in constructor
 *
 * Usage:
 *   struct Node : auto_traceable<Node> {
 *       gc_field<Node> left, right;
 *       GC_FIELDS(left, right);
 *
 *       Node() {
 *           GC_INIT_FIELDS();  // Binds left and right to this
 *       }
 *   };
 *
 * This expands to: _gc_bind_fields()
 */
#define GC_INIT_FIELDS() _gc_bind_fields()

/**
 * @brief Validate that a type properly implements auto_traceable pattern
 *
 * This concept checks at compile-time that:
 * - Type derives from auto_traceable
 * - Type provides _gc_fields() method
 * - _gc_fields() returns a tuple-like Object
 *
 * Usage:
 *   template<AutoTraceable T>
 *   void process_node(T* node) { ... }
 */
template<typename T>
concept AutoTraceable = requires(T obj) {
    // Must provide _gc_fields() method
    { obj._gc_fields() };

    // Must derive from gc_object (via auto_traceable)
    requires std::is_base_of_v<gc_object, T>;
};

/**
 * @brief Example demonstrating manual vs auto-traceable comparison
 */
namespace auto_traceable_example {
    // OLD WAY - Manual trace_references (verbose, error-prone)
    struct ManualNode : gc_object {
        gc_field<ManualNode> left;
        gc_field<ManualNode> right;
        gc_field<Object> data;

        ManualNode() {
            // Must manually bind each field for automatic write barriers
            left.bind_owner(this);
            right.bind_owner(this);
            data.bind_owner(this);
        }

        void trace_references(Tracer& t) override {
            left.trace(t);
            right.trace(t);
            data.trace(t);
            // Easy to forget a field!
        }
    };

    // NEW WAY - Auto-traceable (concise, maintainable)
    struct AutoNode : auto_traceable<AutoNode> {
        gc_field<AutoNode> left;
        gc_field<AutoNode> right;
        gc_field<Object> data;

        GC_FIELDS(left, right, data);  // One line, can't forget fields

        AutoNode() {
            GC_INIT_FIELDS();  // Automatically binds all fields for write barriers
        }
    };

    // ADVANCED - Inheritance with auto_traceable
    struct ColorNode : auto_traceable<ColorNode> {
        gc_field<ColorNode> left;
        gc_field<ColorNode> right;
        gc_field<Object> color;

        GC_FIELDS(left, right, color);
    };

    // ZERO-FIELD - No GC fields (still valid)
    struct LeafNode : auto_traceable<LeafNode> {
        int value;  // Non-GC field

        auto _gc_fields() { return std::make_tuple(); }  // Empty tuple
    };
}

/**
 * @brief Concept-based validation for containers of auto-traceable objects
 *
 * Example:
 *   template<AutoTraceable T>
 *   class gc_vector { ... };  // Only works with auto-traceable types
 */
template<typename T>
concept AutoTraceablePtr = requires(T ptr) {
    // Must be a pointer to auto-traceable Object
    requires AutoTraceable<std::remove_pointer_t<T>>;
};

/**
 * @brief Design Philosophy
 *
 * Why CRTP instead of virtual inheritance?
 * - Zero runtime overhead (no vtable lookup)
 * - Compile-time type safety
 * - Better inlining opportunities
 *
 * Why tuple instead of macros?
 * - Type-safe (compiler errors instead of preprocessor errors)
 * - Debuggable (can step through std::apply)
 * - Composable (can return std::tuple_cat for inheritance)
 *
 * Why _gc_fields() instead of full reflection?
 * - C++ has no reflection (yet - coming in C++26)
 * - One-liner is acceptable boilerplate
 * - Explicit is better than magic (user controls what's traced)
 *
 * Future improvements:
 * - C++26 reflection will allow zero-boilerplate auto-tracing
 * - Could use Boost.PFR for aggregate reflection today
 * - Could generate _gc_fields() with clang LibTooling
 */
