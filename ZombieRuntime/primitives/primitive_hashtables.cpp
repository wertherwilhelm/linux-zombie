#include "zombie_runtime_types.h"
#include "primitive_hashtables.h"
#include "../hash_table.h"
#include "../numeric_runtime.h"
#include "gc_ptr.h"
#include <vector>

namespace zombie { namespace runtime {

PrimitiveHashTables::PrimitiveHashTables(ID id, int minArgs, int maxArgs)
    : PrimitiveBase(minArgs, maxArgs), m_id(id) {
    switch (id) {
        case MAKE:       m_name = "make-hash-table"; break;
        case HTQ:        m_name = "hash-table?"; break;
        case SET:        m_name = "hash-table-set!"; break;
        case REF:        m_name = "hash-table-ref"; break;
        case REFDEFAULT: m_name = "hash-table-ref/default"; break;
        case DELETE:     m_name = "hash-table-delete!"; break;
        case CONTAINS:   m_name = "hash-table-contains?"; break;
        case SIZE:       m_name = "hash-table-size"; break;
        case KEYS:       m_name = "hash-table-keys"; break;
        case VALUES_:    m_name = "hash-table-values"; break;
        case TOALIST:    m_name = "hash-table->alist"; break;
        case UPDATE:        m_name = "hash-table-update!"; break;
        case UPDATEDEFAULT: m_name = "hash-table-update!/default"; break;
    }
}

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
    // collect kept entries (reverse), then rebuild in order
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

} // namespace

Object* PrimitiveHashTables::apply(IEvaluator* eval, Object* args, Environment* env) {
    if (!check_arity(args)) return arity_error("");

    Object* x = (args && !args->is_nil()) ? first(args) : nil;
    Object* y = (args && !args->is_nil() && rest(args) && !rest(args)->is_nil()) ? second(args) : nil;

    switch (m_id) {
        case MAKE:
            // comparator/size args (if any) are accepted but ignored (equal? keys).
            return gc_new<HashTable>();

        case HTQ:
            return truth(as_table(x) != nullptr);

        case SET: {
            HashTable* h = as_table(x);
            if (!h) return error("hash-table-set!: first argument must be a hash table");
            Object* v = third(args);
            Object* existing = find_pair(h->alist(), y);
            if (existing) {
                // update value in place
                ((ConsCell*)existing)->set_cdr_noclone(v);
            } else {
                gc_root<Object> gk(y), gv(v);
                Object* entry = cons(y, v);
                gc_root<Object> ge(entry);
                h->set_alist(cons(entry, h->alist()));
            }
            return nil;  // unspecified
        }

        case REF: {
            HashTable* h = as_table(x);
            if (!h) return error("hash-table-ref: first argument must be a hash table");
            Object* entry = find_pair(h->alist(), y);
            if (entry) return rest(entry);
            // 3rd arg: a thunk (SRFI-69) or a plain default value (lenient).
            Object* dflt = (rest(rest(args)) && !rest(rest(args))->is_nil()) ? third(args) : nullptr;
            if (!dflt) return error("hash-table-ref: key not found");
            if (dynamic_cast<Procedure*>(dflt)) {
                return dynamic_cast<Procedure*>(dflt)->apply(eval, nil, env);
            }
            return dflt;
        }

        case REFDEFAULT: {
            HashTable* h = as_table(x);
            if (!h) return error("hash-table-ref/default: first argument must be a hash table");
            Object* entry = find_pair(h->alist(), y);
            return entry ? rest(entry) : third(args);
        }

        case UPDATE:
        case UPDATEDEFAULT: {
            // (hash-table-update! h key proc [failure-thunk])
            // (hash-table-update!/default h key proc default)
            // new value = (proc current); current is the existing value, or the
            // failure-thunk's result / default when the key is absent.
            HashTable* h = as_table(x);
            if (!h) return error("hash-table-update!: first argument must be a hash table");
            Object* a2 = (args && args->pair()) ? rest(args) : nil;   // (key proc ...)
            Object* a3 = (a2 && a2->pair()) ? rest(a2) : nil;         // (proc ...)
            Object* a4 = (a3 && a3->pair()) ? rest(a3) : nil;         // (4th ...)
            Object* proc = (a3 && a3->pair()) ? first(a3) : nil;
            Object* fourth = (a4 && a4->pair()) ? first(a4) : nullptr;
            Procedure* p = dynamic_cast<Procedure*>(proc);
            if (!p) return error("hash-table-update!: third argument must be a procedure");

            Object* entry = find_pair(h->alist(), y);
            Object* current;
            if (entry) {
                current = rest(entry);
            } else if (m_id == UPDATEDEFAULT) {
                current = fourth;  // plain default value
            } else if (fourth && dynamic_cast<Procedure*>(fourth)) {
                current = dynamic_cast<Procedure*>(fourth)->apply(eval, nil, env);
            } else if (fourth) {
                current = fourth;  // lenient: accept a plain default value too
            } else {
                return error("hash-table-update!: key not found and no failure procedure");
            }

            gc_root<Object> gc(current);
            Object* newval = p->apply(eval, cons(current, nil), env);
            gc_root<Object> gn(newval);

            Object* ex = find_pair(h->alist(), y);
            if (ex) {
                ((ConsCell*)ex)->set_cdr_noclone(newval);
            } else {
                gc_root<Object> gk(y);
                Object* e2 = cons(y, newval);
                gc_root<Object> ge(e2);
                h->set_alist(cons(e2, h->alist()));
            }
            return nil;
        }

        case DELETE: {
            HashTable* h = as_table(x);
            if (!h) return error("hash-table-delete!: first argument must be a hash table");
            h->set_alist(without_key(h->alist(), y));
            return nil;
        }

        case CONTAINS: {
            HashTable* h = as_table(x);
            if (!h) return error("hash-table-contains?: first argument must be a hash table");
            return truth(find_pair(h->alist(), y) != nullptr);
        }

        case SIZE: {
            HashTable* h = as_table(x);
            if (!h) return error("hash-table-size: argument must be a hash table");
            long long n = 0;
            for (Object* p = h->alist(); p && !p->is_nil() && !p->is_atom(); p = rest(p)) ++n;
            return numeric::make_integer(n);
        }

        case KEYS:
        case VALUES_:
        case TOALIST: {
            HashTable* h = as_table(x);
            if (!h) return error("hash-table accessor: argument must be a hash table");
            Object* out = nil;
            gc_root<Object> guard(out);
            // alist is newest-first; reverse to a stable-ish order
            std::vector<Object*> items;
            for (Object* p = h->alist(); p && !p->is_nil() && !p->is_atom(); p = rest(p)) {
                Object* entry = first(p);
                if (m_id == KEYS) items.push_back(first(entry));
                else if (m_id == VALUES_) items.push_back(rest(entry));
                else items.push_back(cons(first(entry), rest(entry)));
            }
            for (auto it = items.begin(); it != items.end(); ++it) {
                out = cons(*it, out);
                guard = gc_root<Object>(out);
            }
            return out;
        }
    }
    return error("hash-table: unknown primitive");
}

void PrimitiveHashTables::install(Environment* env) {
    auto def = [&](const char* name, ID id, int lo, int hi) {
        gc_root<a_string> n(gc_new<a_string>(name));
        env->define(n.get(), gc_new<PrimitiveHashTables>(id, lo, hi));
    };
    def("make-hash-table",       MAKE, 0, 9);
    def("hash-table?",           HTQ, 1, 1);
    def("hash-table-set!",       SET, 3, 3);
    def("hash-table-ref",        REF, 2, 3);
    def("hash-table-ref/default",REFDEFAULT, 3, 3);
    def("hash-table-delete!",    DELETE, 2, 2);
    def("hash-table-contains?",  CONTAINS, 2, 2);
    def("hash-table-size",       SIZE, 1, 1);
    def("hash-table-keys",       KEYS, 1, 1);
    def("hash-table-values",     VALUES_, 1, 1);
    def("hash-table->alist",     TOALIST, 1, 1);
    def("hash-table-update!",    UPDATE, 3, 4);
    def("hash-table-update!/default", UPDATEDEFAULT, 4, 4);
}

} } // namespace zombie::runtime
