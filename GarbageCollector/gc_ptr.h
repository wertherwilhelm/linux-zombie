#pragma once

#include <utility>  // for std::move, std::forward
#include <memory>   // for std::shared_ptr
#include "gc_context.h"  // For gc_context::current()
#include "gc_object.h"   // For gc_object Base

class gc_manager;

/**
 * @file gc_ptr.h
 * @brief Smart pointer wrappers for GC-managed objects
 *
 * This file provides RAII wrappers for GC-managed objects that automatically
 * handle root registration and cleanup. This eliminates manual gc_m_rootguard
 * usage and provides type safety.
 *
 * TWO TYPES OF HANDLES (both copyable with refcounted roots):
 *
 * 1. gc_root<T> - Temporary RAII protection
 *    - Use for: Local variables, function parameters, temporary protection
 *    - Semantic: "I need to protect this Object temporarily"
 *    - Example: gc_root<ConsCell> list(gc->allocate<ConsCell>(car, cdr));
 *    - Visual cue: Lightweight guard, minimal interface
 *
 * 2. gc_ptr<T> - Longer-lived ownership
 *    - Use for: Class members, return values, container storage
 *    - Semantic: "I own this Object for its lifetime"
 *    - Example: std::vector<gc_ptr<gc_object>> objects;
 *    - Visual cue: Full smart pointer interface with comparison operators
 *
 * BOTH are now copyable (refcounted roots) and can be stored in containers.
 * The distinction is primarily semantic/documentary to clarify intent.
 *
 * IMPORTANT: GC Root Refcounting
 * - Multiple handles to the SAME Object share a refcounted root
 * - add_root() increments refcount, remove_root() decrements
 * - Root is only removed when refcount reaches zero
 * - This prevents premature unrooting when multiple handles exist
 *
 * IMPORTANT: gc_scope and Isolated GC Contexts
 * - Handles capture the active gc_manager* at construction time
 * - Always construct handles while the correct gc_scope is active:
 *   CORRECT:
 *     gc_scope swap(isolated_gc);
 *     gc_object* obj = gc->allocate<T>(...);
 *     gc_root<T> handle(obj);  // Uses isolated_gc (via global gc)
 *   INCORRECT:
 *     gc_scope swap(isolated_gc);
 *     gc_object* obj = isolated_gc->allocate<T>(...);
 *     // gc_scope ends
 *     gc_root<T> handle(obj);  // BUG! Uses wrong gc_manager
 * - Or use explicit gc_manager* constructor: gc_root<T>(mgr, obj)
 */

/**
 * @brief Type-safe RAII root guard with refcounted roots
 *
 * Use gc_root<T> for TEMPORARY protection of GC-managed objects.
 * Ideal for local variables, function parameters, and short-lived guards.
 *
 * Automatically registers an Object as a GC root on construction
 * and unregisters it when the last copy is destroyed. Prevents the
 * Object from being collected while any copy exists.
 *
 * NOW COPYABLE: Copies share the same root registration (refcounted).
 * The last copy to be destroyed will unroot the Object.
 *
 * Example (temporary protection):
 *   gc_root<ConsCell> cell(gc->allocate<ConsCell>(car, cdr));
 *   process_list(cell);  // Protected during function call
 *   // Automatically unrooted when cell goes out of scope
 *
 * Use gc_ptr<T> instead if you need longer-lived ownership or comparison ops.
 */
template<typename T>
class gc_root {
private:
    // Refcounted root entry - roots on construction, unroots on destruction
    // IMPORTANT: Uses slot-based registration (register_persistent_root) for moving GC support
    // The obj field is AUTOMATICALLY UPDATED by moving GC after objects are relocated
    struct m_rootentry {
        IGarbageCollector* mgr;
        gc_object* obj;  // This field's ADDRESS is registered as a root slot

        m_rootentry(IGarbageCollector* m, gc_object* o) : mgr(m), obj(o) {
            if (obj && mgr) {
                // Register the LOCATION of obj (not just the value)
                // After moving GC, the collector will update obj to point to new location
                mgr->register_persistent_root(&obj);
            }
        }

        ~m_rootentry() {
            if (mgr && obj) {
                // Unregister the slot
                mgr->unregister_persistent_root(&obj);
            }
        }

        // Non-copyable (managed by shared_ptr)
        m_rootentry(const m_rootentry&) = delete;
        m_rootentry& operator=(const m_rootentry&) = delete;
    };

public:
    /**
     * @brief Construct a gc_root from a raw pointer
     * @param mgr The gc_manager to register with
     * @param ptr Pointer to register as root (can be null)
     */
    explicit gc_root(IGarbageCollector* mgr, T* ptr)
        : m_root(ptr ? std::make_shared<m_rootentry>(mgr, static_cast<gc_object*>(ptr)) : nullptr) {
    }

    /**
     * @brief Construct with current thread-local GC
     * @param ptr Pointer to register as root (can be null)
     */
    explicit gc_root(T* ptr = nullptr)
        : m_root(ptr ? std::make_shared<m_rootentry>(gc_context::current(), static_cast<gc_object*>(ptr)) : nullptr) {
    }

    /**
     * @brief Destructor - automatic via shared_ptr refcount
     */
    ~gc_root() = default;

    // COPYABLE! Copies share the same root registration
    gc_root(const gc_root&) = default;
    gc_root& operator=(const gc_root&) = default;

    // Movable (transfers shared ownership)
    gc_root(gc_root&&) noexcept = default;
    gc_root& operator=(gc_root&&) noexcept = default;

    /**
     * @brief Reset to a new pointer (creates new root registration)
     * @param ptr New pointer to protect (can be null)
     */
    void reset(T* ptr = nullptr) {
        if (ptr) {
            m_root = std::make_shared<m_rootentry>(m_root ? m_root->mgr : gc_context::current(), static_cast<gc_object*>(ptr));
        } else {
            m_root.reset();
        }
    }

    /**
     * @brief Release ownership and force immediate unroot
     * @return The managed pointer (CURRENT location after any GC moves)
     *
     * WARNING: Immediately unroots the Object regardless of other handles.
     * The returned pointer is UNPROTECTED - you must manually register it
     * to re-protect it, or ensure it's reachable from other roots.
     *
     * This breaks shared ownership semantics - use with caution!
     */
    T* release() {
        T* result = get();  // Get current location (may have moved!)
        // Force immediate unroot by unregistering the slot
        if (m_root && m_root->obj && m_root->mgr) {
            m_root->mgr->unregister_persistent_root(&m_root->obj);
            // CRITICAL: Set obj to nullptr so destructor won't double-unregister
            m_root->obj = nullptr;
        }
        m_root.reset();
        return result;
    }

    // Smart pointer-like accessors - ALWAYS read from m_root->obj (updated by GC!)
    T* operator->() const { return get(); }
    T& operator*() const { return *get(); }
    T* get() const { return m_root ? static_cast<T*>(m_root->obj) : nullptr; }
    explicit operator bool() const { return get() != nullptr; }

    // Convenient gc_object* conversion for interop (prevents cast boilerplate)
    gc_object* as_object() const { return m_root ? m_root->obj : nullptr; }
    const gc_object* as_object_const() const { return m_root ? m_root->obj : nullptr; }
    operator gc_object*() const { return as_object(); }

private:
    std::shared_ptr<m_rootentry> m_root;  // Refcounted root registration
    // NOTE: NO cached pointer! Always read from m_root->obj for correctness with moving GC
};

/**
 * @brief Smart pointer for GC-managed objects with refcounted roots
 *
 * Use gc_ptr<T> for LONGER-LIVED ownership of GC-managed objects.
 * Ideal for class members, return values, and container storage.
 *
 * Similar to std::shared_ptr but integrates with the GC system.
 * Automatically registers the Object as a root on construction and
 * unregisters when the last copy is destroyed.
 *
 * NOW COPYABLE: Copies share the same root registration (refcounted).
 * Can be stored in containers, passed by value, returned from functions.
 *
 * Key differences from gc_root<T>:
 * - Semantic: Longer-lived ownership vs temporary protection
 * - Includes comparison operators (==, !=) for use in containers
 * - Visual cue: "I own this" vs "I'm just protecting this temporarily"
 *
 * Example (ownership):
 *   class MyClass {
 *       gc_ptr<ConsCell> owned_list;  // Member ownership
 *   };
 *
 *   gc_ptr<ConsCell> make_list() {
 *       return gc->make_gc_ptr<ConsCell>(car, cdr);  // Return ownership
 *   }
 *
 *   std::vector<gc_ptr<gc_object>> objects;  // Container storage
 *
 * Use gc_root<T> instead for temporary/local protection.
 */
template<typename T>
class gc_ptr {
private:
    // Refcounted root entry - roots on construction, unroots on destruction
    // IMPORTANT: Uses slot-based registration (register_persistent_root) for moving GC support
    // The obj field is AUTOMATICALLY UPDATED by moving GC after objects are relocated
    struct m_rootentry {
        IGarbageCollector* mgr;
        gc_object* obj;  // This field's ADDRESS is registered as a root slot

        m_rootentry(IGarbageCollector* m, gc_object* o) : mgr(m), obj(o) {
            if (obj && mgr) {
                // Register the LOCATION of obj (not just the value)
                // After moving GC, the collector will update obj to point to new location
                mgr->register_persistent_root(&obj);
            }
        }

        ~m_rootentry() {
            if (mgr && obj) {
                // Unregister the slot
                mgr->unregister_persistent_root(&obj);
            }
        }

        // Non-copyable (managed by shared_ptr)
        m_rootentry(const m_rootentry&) = delete;
        m_rootentry& operator=(const m_rootentry&) = delete;
    };

public:
    /**
     * @brief Construct from raw pointer (takes ownership)
     * @param mgr The gc_manager to register with
     * @param ptr Pointer to manage
     */
    explicit gc_ptr(IGarbageCollector* mgr, T* ptr)
        : m_root(ptr ? std::make_shared<m_rootentry>(mgr, static_cast<gc_object*>(ptr)) : nullptr) {
    }

    /**
     * @brief Construct with current thread-local GC
     * @param ptr Pointer to manage
     */
    explicit gc_ptr(T* ptr = nullptr)
        : m_root(ptr ? std::make_shared<m_rootentry>(gc_context::current(), static_cast<gc_object*>(ptr)) : nullptr) {
    }

    /**
     * @brief Destructor - automatic via shared_ptr refcount
     */
    ~gc_ptr() = default;

    // COPYABLE! Copies share the same root registration
    gc_ptr(const gc_ptr&) = default;
    gc_ptr& operator=(const gc_ptr&) = default;

    // Movable (transfers shared ownership)
    gc_ptr(gc_ptr&&) noexcept = default;
    gc_ptr& operator=(gc_ptr&&) noexcept = default;

    /**
     * @brief Reset to a new pointer (creates new root registration)
     */
    void reset(T* ptr = nullptr) {
        if (ptr) {
            m_root = std::make_shared<m_rootentry>(m_root ? m_root->mgr : gc_context::current(), static_cast<gc_object*>(ptr));
        } else {
            m_root.reset();
        }
    }

    /**
     * @brief Release ownership and force immediate unroot
     * @return The managed pointer (CURRENT location after any GC moves)
     *
     * WARNING: Immediately unroots the Object regardless of other handles.
     * The returned pointer is UNPROTECTED - you must manually register it
     * to re-protect it, or ensure it's reachable from other roots.
     *
     * This breaks shared ownership semantics - use with caution!
     */
    T* release() {
        T* result = get();  // Get current location (may have moved!)
        // Force immediate unroot by unregistering the slot
        if (m_root && m_root->obj && m_root->mgr) {
            m_root->mgr->unregister_persistent_root(&m_root->obj);
            // CRITICAL: Set obj to nullptr so destructor won't double-unregister
            m_root->obj = nullptr;
        }
        m_root.reset();
        return result;
    }

    // Smart pointer interface - ALWAYS read from m_root->obj (updated by GC!)
    T* operator->() const { return get(); }
    T& operator*() const { return *get(); }
    T* get() const { return m_root ? static_cast<T*>(m_root->obj) : nullptr; }
    explicit operator bool() const { return get() != nullptr; }

    // Convenient gc_object* conversion for interop (prevents cast boilerplate)
    gc_object* as_object() const { return m_root ? m_root->obj : nullptr; }
    const gc_object* as_object_const() const { return m_root ? m_root->obj : nullptr; }
    operator gc_object*() const { return as_object(); }

    // Comparison operators - compare current locations (may have moved!)
    bool operator==(const gc_ptr& other) const { return get() == other.get(); }
    bool operator!=(const gc_ptr& other) const { return get() != other.get(); }
    bool operator==(std::nullptr_t) const { return get() == nullptr; }
    bool operator!=(std::nullptr_t) const { return get() != nullptr; }

private:
    std::shared_ptr<m_rootentry> m_root;  // Refcounted root registration
    // NOTE: NO cached pointer! Always read from m_root->obj for correctness with moving GC
};

/**
 * @brief Factory function to create a gc_ptr from allocate
 *
 * Usage:
 *   auto cell = make_gc_ptr<ConsCell>(car, cdr);
 */
template<typename T, typename... Args>
gc_ptr<T> make_gc_ptr(Args&&... args) {
    IGarbageCollector* gc = gc_context::current();
    return gc_ptr<T>(gc->allocate<T>(std::forward<Args>(args)...));
}

/**
 * @brief Factory function to create a gc_root from allocate
 *
 * Usage:
 *   auto cell = make_gc_root<ConsCell>(car, cdr);
 */
template<typename T, typename... Args>
gc_root<T> make_gc_root(Args&&... args) {
    IGarbageCollector* gc = gc_context::current();
    return gc_root<T>(gc->allocate<T>(std::forward<Args>(args)...));
}

// ============================================================================
// gc_manager allocation helper implementations
// ============================================================================
// Include full gc_manager definition for template method implementations
#include "gc_manager.h"

/**
 * @brief gc_manager::make_gc_ptr implementation
 *
 * Allocates and wraps in gc_ptr for automatic rooting.
 * Binds to this specific gc_manager instance.
 */
template<typename T, typename... Args>
auto gc_manager::make_gc_ptr(Args&&... args) {
    return gc_ptr<T>(this, allocate<T>(std::forward<Args>(args)...));
}

/**
 * @brief gc_manager::make_gc_root implementation
 *
 * Allocates and wraps in gc_root for automatic rooting.
 * Binds to this specific gc_manager instance.
 */
template<typename T, typename... Args>
auto gc_manager::make_gc_root(Args&&... args) {
    return gc_root<T>(this, allocate<T>(std::forward<Args>(args)...));
}

// ============================================================================
// Syntactic Sugar - Allocation Helpers
// ============================================================================

/**
 * @brief Generic allocation helper (non-rooted)
 *
 * Shorthand for gc_context::current()->allocate<T>(...)
 * Returns raw pointer - caller responsible for rooting if needed.
 *
 * Usage: auto* str = gc_new<a_string>("hello");
 */
template<typename T, typename... Args>
inline T* gc_new(Args&&... args) {
    IGarbageCollector* gc = gc_context::current();
    return gc->allocate<T>(std::forward<Args>(args)...);
}

// ============================================================================
// Usage Examples
// ============================================================================
//
// The syntactic sugar provided above enables cleaner allocation:
//
// OLD:  auto* str = gc->allocate<a_string>("hello");
// NEW:  auto* str = gc_new<a_string>("hello");
//
// OLD:  auto str = make_gc_ptr<a_string>("hello");
// NEW:  auto str = make_gc_ptr<a_string>("hello");  // Already clean!
//
// The generic helpers avoid the need for type-specific factories while
// keeping gc isolated in gc_ptr.h without additional type dependencies.
