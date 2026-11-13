#pragma once

#include <concepts>

/**
 * @file gc_traceable.h
 * @brief Object-agnostic tracing contract for garbage collection
 *
 * This file provides a generic concept for objects that can be traced by
 * the garbage collector, without coupling to any specific Object hierarchy.
 *
 * This enables the GC to work with:
 * - Scheme objects (Object Base class)
 * - C++ objects (any class with trace_references)
 * - Custom types in other languages/VMs
 * - Testing mocks
 */

// Forward declaration
class Tracer;

/**
 * @brief Concept for objects that can be garbage-collected
 *
 * An Object is Traceable if it provides a trace_references method that
 * enumerates all child pointers via a Tracer.
 *
 * This is the core contract that makes the GC library-independent:
 * - No dependency on Scheme's Object hierarchy
 * - No dependency on specific types
 * - Works with any class that can enumerate its children
 *
 * Example implementation:
 *   class MyObject {
 *   public:
 *       MyObject* child1;
 *       MyObject* child2;
 *
 *       void trace_references(Tracer& tracer) {
 *           tracer.visit(child1);
 *           tracer.visit(child2);
 *       }
 *   };
 */
template<typename T>
concept Traceable = requires(T& obj, Tracer& tracer) {
    /**
     * The Object must be able to enumerate its child pointers.
     * This method is called during the mark phase of GC.
     *
     * @param tracer The tracer to visit each child pointer
     */
    { obj.trace_references(tracer) } -> std::same_as<void>;
};

/**
 * @brief Check if a type is traceable at compile time
 *
 * Usage:
 *   static_assert(is_traceable_v<MyObject>, "MyObject must be traceable");
 */
template<typename T>
inline constexpr bool is_traceable_v = Traceable<T>;

/**
 * @brief Trait for checking traceability
 *
 * Usage:
 *   if constexpr (is_traceable<MyObject>::value) {
 *       // Can trace MyObject
 *   }
 */
template<typename T>
struct is_traceable : std::bool_constant<Traceable<T>> {};
