#include "pch.h"
#include "gc_persistent_root.h"
#include "gc_interface.h"
#include "gc_context.h"

gc_persistent_root::gc_persistent_root(gc_object* obj) {
    if (obj) {
        // Capture the current GC instance (thread-local)
        IGarbageCollector* gc = gc_context::current();
        m_entry = new root_entry(obj, gc);
        register_root(m_entry);
    } else {
        m_entry = nullptr;
    }
}

gc_persistent_root::gc_persistent_root(const gc_persistent_root& other)
    : m_entry(other.m_entry) {
    acquire();
}

gc_persistent_root::gc_persistent_root(gc_persistent_root&& other) noexcept
    : m_entry(other.m_entry) {
    other.m_entry = nullptr;  // Transfer ownership
}

gc_persistent_root::~gc_persistent_root() {
    release();
}

gc_persistent_root& gc_persistent_root::operator=(const gc_persistent_root& other) {
    if (this != &other) {
        release();           // Release current
        m_entry = other.m_entry;
        acquire();           // Acquire new
    }
    return *this;
}

gc_persistent_root& gc_persistent_root::operator=(gc_persistent_root&& other) noexcept {
    if (this != &other) {
        release();           // Release current
        m_entry = other.m_entry;
        other.m_entry = nullptr;  // Transfer ownership
    }
    return *this;
}

void gc_persistent_root::acquire() {
    if (m_entry) {
        m_entry->refcount++;
    }
}

void gc_persistent_root::release() {
    if (m_entry) {
        m_entry->refcount--;
        if (m_entry->refcount == 0) {
            unregister_root(m_entry);
            delete m_entry;
            m_entry = nullptr;
        }
    }
}

void gc_persistent_root::register_root(root_entry* entry) {
    if (entry && entry->gc) {
        // Register with the captured GC instance
        entry->gc->register_persistent_root(&(entry->obj));
    }
}

void gc_persistent_root::unregister_root(root_entry* entry) {
    if (entry && entry->gc) {
        // Unregister from the captured GC instance
        entry->gc->unregister_persistent_root(&(entry->obj));
    }
}
