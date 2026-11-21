#include "zombie_runtime_types.h"
#include "primitive_hashtables.h"
#include "../native_primitive.h"
#include "../hash_table.h"
#include "../numeric_runtime.h"
#include "gc_ptr.h"
#include <vector>

/**
 * @file primitive_hashtables.cpp
 * @brief Hash table primitives (SRFI-69 subset), table-driven.
 *
 * Keys compare with equal?; the table is a simple association list (newest-first)
 * on the HashTable object.
 */

namespace zombie { namespace runtime {

namespace {

HashTable* as_table(Object* x) { return dynamic_cast<HashTable*>(x); }

// Find the (key . value) pair for `key` (equal?) in the alist, or nullptr.
Object* find_pair(Object* alist, Object* key) {
    for (Object* p = alist; p && !p->is_nil() && !p->is_atom(); p = rest(p)) {
        Object* entry = first(p);
        if (entry && !entry->is_atom() && equal(first(entry), key)) return entry;
    }
    return nullptr;
}

// Copy the alist excluding any entry whose key equals `key`.
Object* without_key(Object* alist, Object* key) {
    Object* out = nil;
    gc_root<Object> guard(out);
    std::vector<Object*> kept;
    for (Object* p = alist; p && !p->is_nil() && !p->is_atom(); p = rest(p)) {
        Object* entry = first(p);
        if (!(entry && !entry->is_atom() && equal(first(entry), key))) kept.push_back(entry);
    }
    for (auto it = kept.rbegin(); it != kept.rend(); ++it) {
        out = cons(*it, out);
        guard = gc_root<Object>(out);
    }
    return out;
}

// (make-hash-table ...) — comparator/size args accepted but ignored (equal? keys).
Object* prim_make(IEvaluator*, Object*, Environment*) { return gc_new<HashTable>(); }

Object* prim_htq(IEvaluator*, Object* a, Environment*) { return truth(as_table(first(a)) != nullptr); }

// (hash-table-set! h key value)
Object* prim_set(IEvaluator*, Object* a, Environment*) {
    HashTable* h = as_table(first(a));
    if (!h) return error("hash-table-set!: first argument must be a hash table");
    Object* key = second(a), *v = third(a);
    Object* existing = find_pair(h->alist(), key);
    if (existing) {
        ((ConsCell*)existing)->set_cdr_noclone(v);
    } else {
        gc_root<Object> gk(key), gv(v);
        Object* entry = cons(key, v);
        gc_root<Object> ge(entry);
        h->set_alist(cons(entry, h->alist()));
    }
    return nil;
}

// (hash-table-ref h key [default]) — default may be a thunk (SRFI-69) or a value.
Object* prim_ref(IEvaluator* eval, Object* a, Environment* env) {
    HashTable* h = as_table(first(a));
    if (!h) return error("hash-table-ref: first argument must be a hash table");
    Object* entry = find_pair(h->alist(), second(a));
    if (entry) return rest(entry);
    Object* dflt = (rest(rest(a)) && !rest(rest(a))->is_nil()) ? third(a) : nullptr;
    if (!dflt) return error("hash-table-ref: key not found");
    if (dynamic_cast<Procedure*>(dflt)) return dynamic_cast<Procedure*>(dflt)->apply(eval, nil, env);
    return dflt;
}

// (hash-table-ref/default h key default)
Object* prim_ref_default(IEvaluator*, Object* a, Environment*) {
    HashTable* h = as_table(first(a));
    if (!h) return error("hash-table-ref/default: first argument must be a hash table");
    Object* entry = find_pair(h->alist(), second(a));
    return entry ? rest(entry) : third(a);
}

// Shared body for hash-table-update! (failure-thunk) and update!/default (value).
Object* ht_update(IEvaluator* eval, Object* a, Environment* env, bool is_default) {
    HashTable* h = as_table(first(a));
    if (!h) return error("hash-table-update!: first argument must be a hash table");
    Object* key = second(a);
    Object* a3 = rest(rest(a));                        // (proc ...)
    Object* a4 = (a3 && a3->pair()) ? rest(a3) : nil;  // (4th ...)
    Object* proc = (a3 && a3->pair()) ? first(a3) : nil;
    Object* fourth = (a4 && a4->pair()) ? first(a4) : nullptr;
    Procedure* p = dynamic_cast<Procedure*>(proc);
    if (!p) return error("hash-table-update!: third argument must be a procedure");

    Object* entry = find_pair(h->alist(), key);
    Object* current;
    if (entry) {
        current = rest(entry);
    } else if (is_default) {
        current = fourth;  // plain default value
    } else if (fourth && dynamic_cast<Procedure*>(fourth)) {
        current = dynamic_cast<Procedure*>(fourth)->apply(eval, nil, env);
    } else if (fourth) {
        current = fourth;  // lenient: accept a plain default value too
    } else {
        return error("hash-table-update!: key not found and no failure procedure");
    }

    gc_root<Object> gcur(current);
    Object* newval = p->apply(eval, cons(current, nil), env);
    gc_root<Object> gn(newval);

    Object* ex = find_pair(h->alist(), key);
    if (ex) {
        ((ConsCell*)ex)->set_cdr_noclone(newval);
    } else {
        gc_root<Object> gk(key);
        Object* e2 = cons(key, newval);
        gc_root<Object> ge(e2);
        h->set_alist(cons(e2, h->alist()));
    }
    return nil;
}
Object* prim_update(IEvaluator* e, Object* a, Environment* v)         { return ht_update(e, a, v, false); }
Object* prim_update_default(IEvaluator* e, Object* a, Environment* v) { return ht_update(e, a, v, true); }

Object* prim_delete(IEvaluator*, Object* a, Environment*) {
    HashTable* h = as_table(first(a));
    if (!h) return error("hash-table-delete!: first argument must be a hash table");
    h->set_alist(without_key(h->alist(), second(a)));
    return nil;
}

Object* prim_contains(IEvaluator*, Object* a, Environment*) {
    HashTable* h = as_table(first(a));
    if (!h) return error("hash-table-contains?: first argument must be a hash table");
    return truth(find_pair(h->alist(), second(a)) != nullptr);
}

Object* prim_size(IEvaluator*, Object* a, Environment*) {
    HashTable* h = as_table(first(a));
    if (!h) return error("hash-table-size: argument must be a hash table");
    long long n = 0;
    for (Object* p = h->alist(); p && !p->is_nil() && !p->is_atom(); p = rest(p)) ++n;
    return numeric::make_integer(n);
}

// which: 0 keys, 1 values, 2 (key . value) alist.
Object* ht_collect(Object* a, const char* who, int which) {
    HashTable* h = as_table(first(a));
    if (!h) return error(std::string(who) + ": argument must be a hash table");
    Object* out = nil;
    gc_root<Object> guard(out);
    std::vector<Object*> items;
    for (Object* p = h->alist(); p && !p->is_nil() && !p->is_atom(); p = rest(p)) {
        Object* entry = first(p);
        if (which == 0) items.push_back(first(entry));
        else if (which == 1) items.push_back(rest(entry));
        else items.push_back(cons(first(entry), rest(entry)));
    }
    for (auto it = items.begin(); it != items.end(); ++it) {
        out = cons(*it, out);
        guard = gc_root<Object>(out);
    }
    return out;
}
Object* prim_keys(IEvaluator*, Object* a, Environment*)    { return ht_collect(a, "hash-table-keys", 0); }
Object* prim_values(IEvaluator*, Object* a, Environment*)  { return ht_collect(a, "hash-table-values", 1); }
Object* prim_toalist(IEvaluator*, Object* a, Environment*) { return ht_collect(a, "hash-table->alist", 2); }

} // namespace

void PrimitiveHashTables::install(Environment* env) {
    static const NativeEntry entries[] = {
        { "make-hash-table",          0, 9, prim_make           },
        { "hash-table?",              1, 1, prim_htq            },
        { "hash-table-set!",          3, 3, prim_set            },
        { "hash-table-ref",           2, 3, prim_ref            },
        { "hash-table-ref/default",   3, 3, prim_ref_default    },
        { "hash-table-delete!",       2, 2, prim_delete         },
        { "hash-table-contains?",     2, 2, prim_contains       },
        { "hash-table-size",          1, 1, prim_size           },
        { "hash-table-keys",          1, 1, prim_keys           },
        { "hash-table-values",        1, 1, prim_values         },
        { "hash-table->alist",        1, 1, prim_toalist        },
        { "hash-table-update!",       3, 4, prim_update         },
        { "hash-table-update!/default", 4, 4, prim_update_default },
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}

} } // namespace zombie::runtime
