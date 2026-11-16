#pragma once

#include "Base.h"
#include "../GarbageCollector/gc_tracer.h"
#include "../GarbageCollector/gc_ptr.h"

namespace zombie { namespace runtime {

/**
 * @file hash_table.h
 * @brief Mutable hash table (SRFI-69 subset).
 *
 * Backed by an association list of (key . value) pairs with `equal?` key
 * comparison. Not O(1) — a simple, correct implementation; swap the backing
 * store for a real hash map later without changing the primitive surface.
 */
class HashTable : public Base {
public:
    HashTable() : m_alist(nil) {}

    // The backing association list: ((key . value) ...). Mutable.
    Object* alist() const { return m_alist; }
    void set_alist(Object* a) { m_alist = a; }

    int instance_of() override { return static_cast<int>(Base::atype::HASHTABLE); }

    // Hash tables are a distinct, opaque, mutable object — an atom, not a pair.
    int is_atom() const override { return 1; }
    int pair() const override { return 0; }

    // Shallow clone (shares the current alist snapshot).
    Object* clone() const override {
        HashTable* h = gc_new<HashTable>();
        h->m_alist = m_alist;
        return h;
    }

    void print(std::ostream& os) const override { os << "#<hash-table>"; }

    void trace_references(Tracer& tracer) override {
        tracer.visit_field(m_alist);
    }

private:
    Object* m_alist;
};

} } // namespace zombie::runtime
