#pragma once

#include "gc_object.h"
#include <map>

// Forward declaration
class IGarbageCollector;

/**
 * gc_persistent_root - Reference-counted persistent GC root
 *
 * Unlike gc_root_guard (stack-scoped RAII), this provides heap-allocated
 * roots that can be copied, moved, and shared across multiple owners.
 *
 * Use case: Embedding API where C++ needs to hold Scheme objects long-term
 * (e.g., compiled procedures, closures returned from Scheme to C++)
 *
 * Thread-safety: Thread-safe when used with thread-local GC instances
 * (each gc_persistent_root captures its owning GC at construction)
 */
class gc_persistent_root {
private:
    // Shared root entry with reference counting
    struct root_entry {
        gc_object* obj;
        int refcount;
        IGarbageCollector* gc;  // Owning GC instance

        explicit root_entry(gc_object* o, IGarbageCollector* g)
            : obj(o), refcount(1), gc(g) {}
    };

    root_entry* m_entry;  // Shared between all copies

    void acquire();
    void release();

    void register_root(root_entry* entry);
    void unregister_root(root_entry* entry);

public:
    // Construct from gc_object (creates new root, refcount=1)
    explicit gc_persistent_root(gc_object* obj);

    // Copy constructor (shares root, increments refcount)
    gc_persistent_root(const gc_persistent_root& other);

    // Move constructor (transfers ownership, no refcount change)
    gc_persistent_root(gc_persistent_root&& other) noexcept;

    // Destructor (decrements refcount, unroots when refcount reaches 0)
    ~gc_persistent_root();

    // Assignment operators
    gc_persistent_root& operator=(const gc_persistent_root& other);
    gc_persistent_root& operator=(gc_persistent_root&& other) noexcept;

    // Access the wrapped Object
    gc_object* get() const { return m_entry ? m_entry->obj : nullptr; }
    gc_object* operator->() const { return get(); }
    operator gc_object*() const { return get(); }

    // Check if valid
    bool is_valid() const { return m_entry != nullptr && m_entry->obj != nullptr; }
};
