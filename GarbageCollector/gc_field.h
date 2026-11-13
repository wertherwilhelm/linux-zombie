#pragma once

#include "gc_object.h"
#include "gc_context.h"
#include "gc_tracer.h"
#include "gc_manager_template.h"
#include "generational_gc_strategy.h"
#include <type_traits>

/**
 * @file gc_field.h
 * @brief Smart GC field with automatic write barriers and natural syntax
 *
 * gc_field<T> wraps a pointer field and automatically:
 * - Triggers write barriers on assignment (for generational GC)
 * - Provides natural dereferencing (-> and *)
 * - Auto-traces during GC (via .trace() helper)
 *
 * Usage:
 *   struct Node : gc_object {
 *       gc_field<Node> left, right;  // Instead of Node* left, right;
 *
 *       void trace_references(Tracer& t) override {
 *           left.trace(t);
 *           right.trace(t);
 *       }
 *   };
 *
 *   node->left = gc_new<Node>(1);  // Write barrier fires automatically!
 */

/**
 * @brief GC-aware field wrapper with automatic write barriers.
 *
 * Wraps a pointer field and optionally triggers a write barrier on assignment
 * (when bound to an owner). To enable barriers, call bind_owner() once after
 * construction.
 *
 * @tparam T GC-managed type (must derive from gc_object).
 */
template<typename T>
class gc_field {
    static_assert(std::is_base_of_v<gc_object, T>,
                  "gc_field<T> requires T to derive from gc_object");

    T* m_ptr = nullptr;
    gc_object* m_owner = nullptr;  // Object containing this field (for write barriers)

public:
    // ========================================================================
    // Constructors
    // ========================================================================

    /**
     * @brief Construct a null field.
     */
    gc_field() = default;

    /**
     * @brief Construct a null field.
     */
    gc_field(std::nullm_ptrt) : m_ptr(nullptr) {}

    /**
     * @brief Construct from a raw pointer.
     * @param p Initial pointer value.
     */
    explicit gc_field(T* p) : m_ptr(p) {}

    // Copy/move constructors (preserve owner binding)
    /**
     * @brief Copy constructor (preserves owner binding).
     */
    gc_field(const gc_field& other) : m_ptr(other.m_ptr), m_owner(other.m_owner) {}

    /**
     * @brief Move constructor (preserves owner binding).
     */
    gc_field(gc_field&& other) noexcept : m_ptr(other.m_ptr), m_owner(other.m_owner) {
        other.m_ptr = nullptr;
        other.m_owner = nullptr;
    }

    // ========================================================================
    // Owner Binding (for Automatic Write Barriers)
    // ========================================================================

    /**
     * @brief Bind this field to its owner Object
     * @param owner The gc_object containing this field
     *
     * Must be called once after construction to enable automatic write barriers.
     * Typically called from auto_traceable::_gc_bind_fields() or GC_INIT_FIELDS() Macro.
     *
     * Example:
     *   struct Node : gc_object {
     *       gc_field<Node> left, right;
     *
     *       Node() {
     *           left.bind_owner(this);
     *           right.bind_owner(this);
     *       }
     *   };
     */
    void bind_owner(gc_object* owner) {
        m_owner = owner;
    }

    /**
     * @brief Check if field is bound to an owner
     */
    bool is_bound() const {
        return m_owner != nullptr;
    }

    // ========================================================================
    // Assignment with Automatic Write Barrier
    // ========================================================================

    /**
     * @brief Assign a new pointer value with automatic write barrier
     *
     * If owner is bound (via bind_owner()), automatically triggers write barrier
     * by calling gc->write_barrier(m_owner, old_value, new_value).
     *
     * Write barrier behavior:
     * - Mark-sweep GC: No-op (entire heap traced)
     * - Generational GC: Records owner in remembered set if old→young edge
     *
     * If owner is NOT bound, silently skips barrier (conservative - safe but
     * may retain objects longer than necessary).
     *
     * For hot paths that don't need barriers, use set_unsafe() instead.
     */
    gc_field& operator=(T* value) {
        if (m_ptr == value) {
            return *this;  // No change, skip barrier
        }

        T* old_value = m_ptr;
        m_ptr = value;

        // Fire write barrier if owner is bound and GC is active
        if (m_owner) {
            IGarbageCollector* gc = gc_context::current();
            if (gc) {
                gc->write_barrier(m_owner, static_cast<gc_object*>(old_value), static_cast<gc_object*>(value));
            }
        }

        return *this;
    }

    /**
     * @brief Set value without write barrier (for performance-critical paths)
     * @param value The new pointer value
     *
     * Use this when you KNOW the assignment doesn't create an old→young edge,
     * or when you're using mark-sweep GC (which doesn't need barriers).
     *
     * Example:
     *   struct Node : gc_object {
     *       gc_field<Node> left;
     *
     *       void set_child_unsafe(Node* child) {
     *           left.set_unsafe(child);  // Skip barrier
     *       }
     *   };
     *
     * WARNING: Using this incorrectly with generational GC can cause live
     * objects to be collected. Only use when:
     * - Both owner and value are in same generation
     * - Using mark-sweep GC (not generational)
     * - In tight loop where barrier overhead is measured bottleneck
     */
    void set_unsafe(T* value) {
        m_ptr = value;
    }

    gc_field& operator=(const gc_field& other) {
        return *this = other.m_ptr;
    }

    gc_field& operator=(gc_field&& other) noexcept {
        *this = other.m_ptr;
        other.m_ptr = nullptr;
        return *this;
    }

    gc_field& operator=(std::nullm_ptrt) {
        return *this = static_cast<T*>(nullptr);
    }

    // ========================================================================
    // Natural Pointer Semantics
    // ========================================================================

    // Dereference
    T& operator*() const { return *m_ptr; }
    T* operator->() const { return m_ptr; }

    // Boolean conversion (nullptr check)
    explicit operator bool() const { return m_ptr != nullptr; }

    // Implicit conversion to raw pointer (for compatibility)
    operator T*() const { return m_ptr; }

    // Explicit get
    T* get() const { return m_ptr; }

    // Comparison
    bool operator==(const gc_field& other) const { return m_ptr == other.m_ptr; }
    bool operator!=(const gc_field& other) const { return m_ptr != other.m_ptr; }
    bool operator==(T* other) const { return m_ptr == other; }
    bool operator!=(T* other) const { return m_ptr != other; }
    bool operator==(std::nullm_ptrt) const { return m_ptr == nullptr; }
    bool operator!=(std::nullm_ptrt) const { return m_ptr != nullptr; }

    // ========================================================================
    // GC Integration
    // ========================================================================

    /**
     * @brief Trace this field during GC
     *
     * Call from trace_references():
     *   void trace_references(Tracer& t) override {
     *       left.trace(t);
     *       right.trace(t);
     *   }
     */
    void trace(Tracer& t) const {
        if (m_ptr) {
            t.visit_field(const_cast<T**>(&m_ptr));
        }
    }

    // ========================================================================
    // Swap Support (for STL algorithms)
    // ========================================================================

    friend void swap(gc_field& a, gc_field& b) noexcept {
        using std::swap;
        swap(a.m_ptr, b.m_ptr);
    }
};

/**
 * @brief Helper for tracing multiple gc_field members at once
 *
 * Usage:
 *   void trace_references(Tracer& t) override {
 *       gc_trace_fields(t, left, right, parent, child);
 *   }
 */
template<typename... Fields>
void gc_trace_fields(Tracer& t, Fields&... fields) {
    (fields.trace(t), ...);  // C++17 fold expression
}
