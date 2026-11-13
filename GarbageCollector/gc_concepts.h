#pragma once

#include "gc_object.h"
#include "gc_tracer.h"
#include "gc_interface.h"
#include "gc_field.h"
#include <concepts>
#include <type_traits>

/**
 * @file gc_concepts.h
 * @brief C++20 concepts for compile-time GC validation
 *
 * Provides type constraints that:
 * - Catch GC errors at compile time (not runtime)
 * - Improve error messages (concept failures are readable)
 * - Document requirements (concepts are self-documenting)
 * - Enable concept-based overloading
 *
 * Usage:
 *   template<Traceable T>
 *   void mark_tree(T* root) { ... }  // Only works with traceable types
 *
 *   template<GCAllocatable T>
 *   T* allocate_object(IGarbageCollector* gc) { ... }
 */

// ============================================================================
// Core GC Concepts
// ============================================================================

/**
 * @brief Object can be traced by the garbage collector
 *
 * Requirements:
 * - Derives from gc_object
 * - Has trace_references(Tracer&) method
 *
 * Example:
 *   template<Traceable T>
 *   void trace_graph(T* root, Tracer& tracer) {
 *       root->trace_references(tracer);
 *   }
 */
template<typename T>
concept Traceable = requires(T obj, Tracer& tracer) {
    // Must derive from gc_object
    requires std::is_base_of_v<gc_object, T>;

    // Must have trace_references method
    { obj.trace_references(tracer) } -> std::same_as<void>;
};

/**
 * @brief Object can be allocated by garbage collector
 *
 * Requirements:
 * - Derives from gc_object
 * - Is default constructible (or has custom constructor)
 * - Is not abstract (can be instantiated)
 *
 * Example:
 *   template<GCAllocatable T, typename... Args>
 *   T* gc_allocate(IGarbageCollector* gc, Args&&... args) {
 *       return gc->allocate<T>(std::forward<Args>(args)...);
 *   }
 */
template<typename T>
concept GCAllocatable = requires {
    // Must derive from gc_object
    requires std::is_base_of_v<gc_object, T>;

    // Must not be abstract (can be instantiated)
    requires !std::is_abstract_v<T>;

    // Must be destructible (GC needs to clean up)
    requires std::is_destructible_v<T>;
};

/**
 * @brief Object uses RAII for automatic tracing
 *
 * Requirements:
 * - Provides _gc_fields() method returning tuple-like Object
 * - Derives from gc_object (via auto_traceable)
 *
 * Example:
 *   template<AutoTraceable T>
 *   void verify_auto_trace(T* obj) {
 *       auto fields = obj->_gc_fields();  // Guaranteed to exist
 *   }
 */
template<typename T>
concept AutoTraceable = requires(T obj) {
    // Must provide _gc_fields() method
    { obj._gc_fields() };

    // Must derive from gc_object (via auto_traceable)
    requires std::is_base_of_v<gc_object, T>;
};

/**
 * @brief Container provides trace() method for contained objects
 *
 * Requirements:
 * - Has trace(Tracer&) method (not trace_references - different name!)
 * - Used by gc_vector, gc_map, etc.
 *
 * Example:
 *   template<ContainerTraceable C>
 *   void trace_container_contents(C& container, Tracer& t) {
 *       container.trace(t);
 *   }
 */
template<typename C>
concept ContainerTraceable = requires(C container, Tracer& tracer) {
    // Must have trace() method (container convention)
    { container.trace(tracer) } -> std::same_as<void>;
};

/**
 * @brief Type is a GC-managed pointer field
 *
 * Requirements:
 * - Is gc_field<T> specialization
 * - Has trace(Tracer&) method
 * - Wraps a gc_object-derived type
 *
 * Example:
 *   template<GCField F>
 *   void trace_field(F& field, Tracer& t) {
 *       field.trace(t);
 *   }
 */
template<typename F>
concept GCField = requires(F field, Tracer& tracer) {
    // Must have trace() method
    { field.trace(tracer) } -> std::same_as<void>;

    // Must have get() method returning gc_object-derived pointer
    requires std::is_base_of_v<gc_object, std::remove_pointer_t<decltype(field.get())>>;
};

// ============================================================================
// Advanced Concepts
// ============================================================================

/**
 * @brief Object is movable by copying GC
 *
 * Requirements:
 * - GCAllocatable (can be allocated)
 * - Trivially copyable OR has copy constructor
 * - Used for generational GC with Cheney's algorithm
 *
 * Example:
 *   template<Movable T>
 *   T* copy_to_space(T* from_obj, char*& to_space) {
 *       T* to_obj = new (to_space) T(*from_obj);
 *       to_space += sizeof(T);
 *       return to_obj;
 *   }
 */
template<typename T>
concept Movable = requires {
    requires GCAllocatable<T>;

    // Must be copy constructible for relocation
    requires std::is_copy_constructible_v<T> || std::is_trivially_copyable_v<T>;
};

/**
 * @brief GC strategy supports nursery allocation
 *
 * Requirements:
 * - Has allocate_young<T>() method
 * - Has allocate_old<T>() method
 * - Used by gc_new for automatic routing
 *
 * Example:
 *   template<GenerationalStrategy S>
 *   Node* allocate_temp(S* strategy) {
 *       return strategy->allocate_young<Node>(42);
 *   }
 */
template<typename S>
concept GenerationalStrategy = requires(S strategy) {
    // Must have allocate_young method
    { strategy.template allocate_young<gc_object>() } -> std::same_as<gc_object*>;

    // Must have allocate_old method
    { strategy.template allocate_old<gc_object>() } -> std::same_as<gc_object*>;
};

/**
 * @brief Type can be used as GC root
 *
 * Requirements:
 * - Is a pointer to gc_object-derived type
 * - Pointer is not const (GC may update it for moving collectors)
 *
 * Example:
 *   template<GCRoot R>
 *   void register_root(IGarbageCollector* gc, R* root_location) {
 *       gc->register_persistent_root(reinterpret_cast<gc_object**>(root_location));
 *   }
 */
template<typename R>
concept GCRoot = requires {
    // Must be a pointer type
    requires std::is_pointer_v<R>;

    // Pointed-to type must derive from gc_object
    requires std::is_base_of_v<gc_object, std::remove_pointer_t<R>>;

    // Must not be const pointer (GC may update it)
    requires !std::is_const_v<std::remove_pointer_t<R>>;
};

/**
 * @brief Type is safe to store in gc_field
 *
 * Requirements:
 * - Derives from gc_object
 * - Is not a reference or pointer (gc_field stores pointer internally)
 *
 * Example:
 *   template<FieldStorable T>
 *   struct Container {
 *       gc_field<T> field;  // OK
 *   };
 */
template<typename T>
concept FieldStorable = requires {
    // Must derive from gc_object
    requires std::is_base_of_v<gc_object, T>;

    // Must not be reference or pointer
    requires !std::is_reference_v<T>;
    requires !std::is_pointer_v<T>;
};

// ============================================================================
// Compound Concepts (Logical Combinations)
// ============================================================================

/**
 * @brief Full-featured GC Object
 *
 * Combines: Traceable + GCAllocatable + Movable
 * Use for objects that work with any GC strategy.
 */
template<typename T>
concept GCObject = Traceable<T> && GCAllocatable<T> && Movable<T>;

/**
 * @brief Container holding GC objects
 *
 * Combines: ContainerTraceable + requires element type is Traceable
 * Use for generic container operations.
 */
template<typename C, typename T>
concept GCContainer = requires {
    requires ContainerTraceable<C>;
    requires Traceable<T>;
};

// ============================================================================
// Type Traits (Concept-Based)
// ============================================================================

/**
 * @brief Check if type uses auto-traceable pattern
 */
template<typename T>
inline constexpr bool is_auto_traceable_v = AutoTraceable<T>;

/**
 * @brief Check if type is GC-allocatable
 */
template<typename T>
inline constexpr bool is_gc_allocatable_v = GCAllocatable<T>;

/**
 * @brief Check if type is a GC field
 */
template<typename F>
inline constexpr bool is_gc_field_v = GCField<F>;

/**
 * @brief Check if container is GC-traceable
 */
template<typename C>
inline constexpr bool is_container_traceable_v = ContainerTraceable<C>;

// ============================================================================
// Concept Validation (Compile-Time Tests)
// ============================================================================

namespace gc_concepts_tests {
    // Forward declarations for testing
    template<typename T> class gc_field;
    template<typename... Ts> class gc_local;
    template<typename T> class gc_vector;

    // Test basic concepts
    static_assert(std::is_base_of_v<gc_object, gc_object>, "gc_object is its own Base");

    // Test that gc_object satisfies core concepts
    static_assert(Traceable<gc_object>, "gc_object must be Traceable");
    static_assert(!GCAllocatable<gc_object>, "gc_object is abstract (cannot allocate)");

    // Example concrete type
    struct ConcreteNode : gc_object {
        void trace_references(Tracer&) override {}
    };

    static_assert(Traceable<ConcreteNode>, "ConcreteNode must be Traceable");
    static_assert(GCAllocatable<ConcreteNode>, "ConcreteNode must be GCAllocatable");
    static_assert(Movable<ConcreteNode>, "ConcreteNode must be Movable");
    static_assert(GCObject<ConcreteNode>, "ConcreteNode must satisfy GCObject");
}

// ============================================================================
// Usage Examples
// ============================================================================

namespace gc_concepts_examples {
    struct Node : gc_object {
        gc_field<Node> left, right;
        int value;

        void trace_references(Tracer& t) override {
            left.trace(t);
            right.trace(t);
        }
    };

    // Example 1: Concept-constrained function
    template<Traceable T>
    void mark_all_reachable(T* root, Tracer& tracer) {
        if (root) {
            root->trace_references(tracer);
        }
    }

    // Example 2: Concept-based overloading
    template<AutoTraceable T>
    void trace_auto(T* obj, Tracer& t) {
        // Use auto-generated trace_references
        obj->trace_references(t);
    }

    template<Traceable T>
    requires (!AutoTraceable<T>)
    void trace_manual(T* obj, Tracer& t) {
        // Manual trace_references
        obj->trace_references(t);
    }

    // Example 3: Container constraints
    template<typename C>
    requires ContainerTraceable<C>
    void trace_container(C& container, Tracer& t) {
        container.trace(t);
    }

    // Example 4: GC root validation
    template<GCRoot R>
    void safe_register_root(IGarbageCollector* gc, R* root_ptr) {
        gc->register_persistent_root(reinterpret_cast<gc_object**>(root_ptr));
    }

    void example_usage() {
        Node* root = nullptr;

        // OK: Node* satisfies GCRoot
        // safe_register_root(gc, &root);

        // Compile error: const Node* doesn't satisfy GCRoot (can't update)
        // const Node* const_root = nullptr;
        // safe_register_root(gc, &const_root);  // ERROR

        // Compile error: int* doesn't satisfy GCRoot (not gc_object-derived)
        // int* not_gc = nullptr;
        // safe_register_root(gc, &not_gc);  // ERROR
    }
}

/**
 * @brief Design Philosophy
 *
 * Why C++20 concepts instead of SFINAE?
 * ======================================
 * - Better error messages (concept failures are readable)
 * - Clearer intent (self-documenting constraints)
 * - Easier to compose (logical && and || operators)
 * - Compiler can diagnose earlier (during template parsing)
 *
 * Concept Naming Conventions:
 * ===========================
 * - Traceable/GCAllocatable/Movable - Capability concepts (Object can do X)
 * - AutoTraceable/ContainerTraceable - Pattern concepts (Object uses pattern X)
 * - GCRoot/GCField - Type category concepts (type is X)
 * - GCObject/GCContainer - Compound concepts (combines multiple concepts)
 *
 * When to use concepts:
 * =====================
 * 1. Template constraints (replace enable_if)
 * 2. Concept-based overloading (multiple implementations)
 * 3. Static assertions (compile-time validation)
 * 4. Documentation (concepts document requirements)
 *
 * When NOT to use concepts:
 * =========================
 * - For simple type traits (use std::is_* directly)
 * - For runtime polymorphism (use virtual functions)
 * - For C++17 codebases (concepts require C++20)
 *
 * Concept vs Static Assert:
 * =========================
 * Concept: Used in template parameter list
 *   template<Traceable T>
 *   void func(T* obj) { ... }
 *
 * Static Assert: Used in function body
 *   template<typename T>
 *   void func(T* obj) {
 *       static_assert(Traceable<T>, "T must be Traceable");
 *       ...
 *   }
 *
 * Prefer concepts (better error messages, checked earlier).
 */
