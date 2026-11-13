#pragma once

#include "gc_object.h"
#include "gc_field.h"
#include "gc_tracer.h"
#include "gc_context.h"
#include "auto_traceable.h"
#include "gc_manager_template.h"
#include "generational_gc_strategy.h"
#include <vector>
#include <unordered_map>
#include <map>
#include <set>
#include <utility>
#include <algorithm>

/**
 * @file gc_containers.h
 * @brief GC-aware STL containers with automatic tracing
 *
 * Provides drop-in replacements for std::vector, std::map, etc. that:
 * - Store gc_object* elements safely
 * - Automatically trace all contained objects
 * - Integrate with gc_field and auto_traceable
 *
 * Usage:
 *   struct Scene : auto_traceable<Scene> {
 *       gc_vector<Node> children;  // Instead of std::vector<Node*>
 *       gc_map<std::string, Object> table;  // Instead of std::map<...>
 *
 *       GC_FIELDS(children, table);  // Automatic tracing!
 *   };
 */

// ============================================================================
// gc_vector - Dynamic array of GC pointers
// ============================================================================

/**
 * @brief GC-aware vector container
 *
 * @tparam T Element type (must derive from gc_object)
 *
 * This is a thin wrapper around std::vector<T*> that:
 * - Stores raw pointers (for performance and STL compatibility)
 * - Provides trace() method for GC integration
 * - Natural STL-like API (push_back, operator[], iterators)
 *
 * Design Philosophy:
 * - Store raw pointers (not gc_field) for zero overhead
 * - Trace all elements during GC via single trace() call
 * - Interoperable with std::vector (can std::move)
 *
 * Example:
 *   gc_vector<Node> nodes;
 *   nodes.push_back(gc_new<Node>(1));
 *   nodes.push_back(gc_new<Node>(2));
 *
 *   void trace_references(Tracer& t) override {
 *       nodes.trace(t);  // Traces all contained nodes
 *   }
 */
template<typename T>
class gc_vector {
    static_assert(std::is_base_of_v<gc_object, T>,
                  "gc_vector<T> requires T to derive from gc_object");

    std::vector<T*> m_data;

public:
    // ========================================================================
    // Type Aliases (STL Compatibility)
    // ========================================================================

    using ValueType = T*;
    using size_type = typename std::vector<T*>::size_type;
    using iterator = typename std::vector<T*>::iterator;
    using const_iterator = typename std::vector<T*>::const_iterator;

    // ========================================================================
    // Constructors
    // ========================================================================

    gc_vector() = default;

    explicit gc_vector(size_type count) : m_data(count, nullptr) {}

    gc_vector(size_type count, T* value) : m_data(count, value) {}

    template<typename InputIt>
    gc_vector(InputIt first, InputIt last) : m_data(first, last) {}

    gc_vector(std::initializer_list<T*> init) : m_data(init) {}

    // Copy/move constructors
    gc_vector(const gc_vector&) = default;
    gc_vector(gc_vector&&) noexcept = default;
    gc_vector& operator=(const gc_vector&) = default;
    gc_vector& operator=(gc_vector&&) noexcept = default;

    // ========================================================================
    // Element Access
    // ========================================================================

    T*& operator[](size_type pos) { return m_data[pos]; }
    const T* operator[](size_type pos) const { return m_data[pos]; }

    T*& at(size_type pos) { return m_data.at(pos); }
    const T* at(size_type pos) const { return m_data.at(pos); }

    T*& front() { return m_data.front(); }
    const T* front() const { return m_data.front(); }

    T*& back() { return m_data.back(); }
    const T* back() const { return m_data.back(); }

    T** data() { return m_data.data(); }
    const T* const* data() const { return m_data.data(); }

    // ========================================================================
    // Iterators
    // ========================================================================

    iterator begin() { return m_data.begin(); }
    const_iterator begin() const { return m_data.begin(); }
    const_iterator cbegin() const { return m_data.cbegin(); }

    iterator end() { return m_data.end(); }
    const_iterator end() const { return m_data.end(); }
    const_iterator cend() const { return m_data.cend(); }

    // ========================================================================
    // Capacity
    // ========================================================================

    bool empty() const { return m_data.empty(); }
    size_type size() const { return m_data.size(); }
    size_type capacity() const { return m_data.capacity(); }

    void reserve(size_type new_cap) { m_data.reserve(new_cap); }
    void shrink_to_fit() { m_data.shrink_to_fit(); }

    // ========================================================================
    // Modifiers
    // ========================================================================

    void clear() { m_data.clear(); }

    /**
     * @brief Add element to vector
     *
     * NOTE: Does NOT record write barrier automatically (container doesn't
     * know its owner). For generational GC, use push_back_with_barrier()
     * if the container is in old generation.
     */
    void push_back(T* value) { m_data.push_back(value); }

    /**
     * @brief Add element with explicit write barrier
     * @param owner The Object containing this vector
     * @param value The pointer to add
     *
     * Use when container is in old generation and you need write barriers.
     *
     * Example:
     *   struct Scene : gc_object {
     *       gc_vector<Node> children;
     *   };
     *
     *   scene->children.push_back_with_barrier(scene, child);
     */
    void push_back_with_barrier(gc_object* owner, T* value) {
        m_data.push_back(value);

        // Record write barrier if GC is generational
        IGarbageCollector* gc = gc_context::current();
        if (!gc) return;

        auto* gen_gc = dynamic_cast<gc_manager_t<GenerationalGCStrategy>*>(gc);
        if (gen_gc && owner && value) {
            gen_gc->strategy().record_write_barrier(owner, value);
        }
    }

    template<typename... Args>
    void emplace_back(Args&&... args) {
        m_data.emplace_back(std::forward<Args>(args)...);
    }

    void pop_back() { m_data.pop_back(); }

    void resize(size_type count) { m_data.resize(count); }
    void resize(size_type count, T* value) { m_data.resize(count, value); }

    iterator erase(const_iterator pos) { return m_data.erase(pos); }
    iterator erase(const_iterator first, const_iterator last) {
        return m_data.erase(first, last);
    }

    iterator insert(const_iterator pos, T* value) {
        return m_data.insert(pos, value);
    }

    // ========================================================================
    // GC Integration
    // ========================================================================

    /**
     * @brief Trace all contained objects
     *
     * Call from trace_references():
     *   void trace_references(Tracer& t) override {
     *       children.trace(t);
     *   }
     *
     * CRITICAL for moving GC: Uses visit_field() to pass pointer LOCATION,
     * not pointer value. This allows moving GC to update vector elements
     * after relocating objects.
     */
    void trace(Tracer& t) {
        for (T*& ptr : m_data) {  // Non-const ref to allow updates
            if (ptr) {
                t.visit_field(&ptr);  // Pass address for moving GC
            }
        }
    }

    // ========================================================================
    // Interoperability
    // ========================================================================

    /**
     * @brief Get underlying std::vector (for compatibility)
     */
    std::vector<T*>& underlying() { return m_data; }
    const std::vector<T*>& underlying() const { return m_data; }

    /**
     * @brief Move from std::vector
     */
    static gc_vector from_std_vector(std::vector<T*>&& vec) {
        gc_vector result;
        result.m_data = std::move(vec);
        return result;
    }
};

// ============================================================================
// gc_map - Associative container of GC pointers
// ============================================================================

/**
 * @brief GC-aware map container
 *
 * @tparam Key Key type (does NOT need to be gc_object)
 * @tparam Value Value type (must derive from gc_object)
 *
 * Wraps std::map<Key, Value*> with automatic tracing.
 *
 * Example:
 *   gc_map<std::string, Object> symbol_table;
 *   symbol_table["foo"] = gc_new<Object>(42);
 *
 *   void trace_references(Tracer& t) override {
 *       symbol_table.trace(t);  // Traces all values
 *   }
 */
template<typename Key, typename Value>
class gc_map {
    static_assert(std::is_base_of_v<gc_object, Value>,
                  "gc_map<K,V> requires V to derive from gc_object");

    std::map<Key, Value*> m_data;

public:
    // Type aliases
    using key_type = Key;
    using mapped_type = Value*;
    using ValueType = typename std::map<Key, Value*>::ValueType;
    using size_type = typename std::map<Key, Value*>::size_type;
    using iterator = typename std::map<Key, Value*>::iterator;
    using const_iterator = typename std::map<Key, Value*>::const_iterator;

    // Constructors
    gc_map() = default;
    gc_map(std::initializer_list<ValueType> init) : m_data(init) {}

    // Element access
    Value*& operator[](const Key& key) { return m_data[key]; }
    Value*& operator[](Key&& key) { return m_data[std::move(key)]; }

    Value*& at(const Key& key) { return m_data.at(key); }
    const Value* at(const Key& key) const { return m_data.at(key); }

    // Iterators
    iterator begin() { return m_data.begin(); }
    const_iterator begin() const { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator end() const { return m_data.end(); }

    // Capacity
    bool empty() const { return m_data.empty(); }
    size_type size() const { return m_data.size(); }

    // Modifiers
    void clear() { m_data.clear(); }

    std::pair<iterator, bool> insert(const ValueType& value) {
        return m_data.insert(value);
    }

    iterator erase(const_iterator pos) { return m_data.erase(pos); }
    size_type erase(const Key& key) { return m_data.erase(key); }

    // Lookup
    iterator find(const Key& key) { return m_data.find(key); }
    const_iterator find(const Key& key) const { return m_data.find(key); }

    bool contains(const Key& key) const { return m_data.find(key) != m_data.end(); }

    // GC Integration
    /**
     * @brief Trace all map values
     *
     * CRITICAL for moving GC: Uses visit_field() with mutable reference
     * to allow updating map values after Object relocation.
     *
     * NOTE: Uses const_cast because std::map iterator returns const pair,
     * but we need mutable value pointer for GC updates.
     */
    void trace(Tracer& t) {
        for (auto& pair : m_data) {
            // Map stores pair<const Key, Value*>, need mutable Value* ref
            Value*& value_ref = const_cast<Value*&>(pair.second);
            if (value_ref) {
                t.visit_field(&value_ref);  // Pass address for moving GC
            }
        }
    }

    // Interoperability
    std::map<Key, Value*>& underlying() { return m_data; }
    const std::map<Key, Value*>& underlying() const { return m_data; }
};

// ============================================================================
// gc_unordered_map - Hash map of GC pointers
// ============================================================================

/**
 * @brief GC-aware unordered_map container
 *
 * Same as gc_map but using std::unordered_map for O(1) lookup.
 */
template<typename Key, typename Value, typename Hash = std::hash<Key>>
class gc_unordered_map {
    static_assert(std::is_base_of_v<gc_object, Value>,
                  "gc_unordered_map<K,V> requires V to derive from gc_object");

    std::unordered_map<Key, Value*, Hash> m_data;

public:
    using key_type = Key;
    using mapped_type = Value*;
    using ValueType = typename std::unordered_map<Key, Value*, Hash>::ValueType;
    using size_type = typename std::unordered_map<Key, Value*, Hash>::size_type;
    using iterator = typename std::unordered_map<Key, Value*, Hash>::iterator;
    using const_iterator = typename std::unordered_map<Key, Value*, Hash>::const_iterator;

    gc_unordered_map() = default;
    gc_unordered_map(std::initializer_list<ValueType> init) : m_data(init) {}

    Value*& operator[](const Key& key) { return m_data[key]; }
    Value*& operator[](Key&& key) { return m_data[std::move(key)]; }

    iterator begin() { return m_data.begin(); }
    const_iterator begin() const { return m_data.begin(); }
    iterator end() { return m_data.end(); }
    const_iterator end() const { return m_data.end(); }

    bool empty() const { return m_data.empty(); }
    size_type size() const { return m_data.size(); }

    void clear() { m_data.clear(); }

    std::pair<iterator, bool> insert(const ValueType& value) {
        return m_data.insert(value);
    }

    iterator find(const Key& key) { return m_data.find(key); }
    const_iterator find(const Key& key) const { return m_data.find(key); }

    bool contains(const Key& key) const { return m_data.find(key) != m_data.end(); }

    /**
     * @brief Trace all map values
     *
     * CRITICAL for moving GC: Uses visit_field() with mutable reference.
     */
    void trace(Tracer& t) {
        for (auto& pair : m_data) {
            // Unordered map stores pair<const Key, Value*>, need mutable Value* ref
            Value*& value_ref = const_cast<Value*&>(pair.second);
            if (value_ref) {
                t.visit_field(&value_ref);  // Pass address for moving GC
            }
        }
    }

    std::unordered_map<Key, Value*, Hash>& underlying() { return m_data; }
    const std::unordered_map<Key, Value*, Hash>& underlying() const { return m_data; }
};

// ============================================================================
// Example Usage
// ============================================================================

namespace gc_containers_example {
    struct Node : gc_object {
        int value;
        Node(int v) : value(v) {}
        void trace_references(Tracer&) override {}
    };

    // Scene graph using gc_vector
    struct Scene : auto_traceable<Scene> {
        gc_vector<Node> children;
        gc_field<Node> root;

        GC_FIELDS(children, root);  // Traces both vector and field!

        void add_child(Node* child) {
            children.push_back(child);
        }

        Node* get_child(size_t i) const {
            return children[i];
        }
    };

    // Symbol table using gc_map
    struct Environment : auto_traceable<Environment> {
        gc_map<std::string, Object> bindings;
        gc_field<Environment> parent;

        GC_FIELDS(bindings, parent);

        void define(const std::string& name, Object* value) {
            bindings[name] = value;
        }

        Object* lookup(const std::string& name) {
            auto it = bindings.find(name);
            if (it != bindings.end()) {
                return it->second;
            }
            return parent ? parent->lookup(name) : nullptr;
        }
    };

    // Complete example with manual trace_references
    struct Graph : gc_object {
        gc_vector<Node> nodes;
        gc_map<int, Node> node_by_id;

        void trace_references(Tracer& t) override {
            nodes.trace(t);       // Trace all nodes in vector
            node_by_id.trace(t);  // Trace all nodes in map
        }

        void add_node(int id, Node* node) {
            nodes.push_back(node);
            node_by_id[id] = node;
        }
    };
}

/**
 * @brief Design Philosophy
 *
 * Why wrap std containers instead of custom implementation?
 * =========================================================
 * - Leverage battle-tested STL implementations
 * - Zero overhead (thin wrapper around std::vector)
 * - Familiar API (drop-in replacement)
 * - Interoperable with existing std code
 *
 * Why store raw pointers instead of gc_field<T>?
 * ===============================================
 * - Performance: No overhead per element
 * - Memory: gc_field has same size as T* but adds complexity
 * - Write barriers: Not needed for generational GC (container is young gen)
 * - Simplicity: trace() method handles all elements at once
 *
 * When would we need gc_field in containers?
 * ===========================================
 * - If container itself is in old generation
 * - If elements can move between containers frequently
 * - If individual element writes need barriers
 *
 * Future enhancement: gc_vector<T, UseBarriers=false> template parameter
 *
 * Container Performance Characteristics:
 * ======================================
 * gc_vector<T>:
 * - Same as std::vector<T*> (no overhead)
 * - trace() is O(n) during GC
 *
 * gc_map<K,V>:
 * - Same as std::map<K,V*> (no overhead)
 * - trace() is O(n) during GC
 *
 * gc_unordered_map<K,V>:
 * - Same as std::unordered_map<K,V*> (no overhead)
 * - trace() is O(n) during GC (but with better cache locality)
 */
