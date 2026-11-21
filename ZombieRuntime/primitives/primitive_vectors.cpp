#include "zombie_runtime_types.h"
#include "primitive_vectors.h"
#include "../native_primitive.h"
#include "../Base.h"
#include "../atom.h"
#include "../a_vector.h"
#include "../numeric_runtime.h"
#include <vector>

/**
 * @file primitive_vectors.cpp
 * @brief Vector primitives (R7RS 6.8), table-driven.
 */

namespace {

using namespace zombie::runtime;

a_vector* list_to_vector(Object* lst) {
    if (lst == NULL || lst->is_nil()) return gc_new<a_vector>();
    std::vector<Object*> elements;
    for (Object* cur = lst; cur != NULL && !cur->is_nil() && !cur->is_atom(); cur = cdr(cur)) {
        elements.push_back(car(cur));
    }
    return gc_new<a_vector>(elements);
}

// nth argument (0-based), or nil if absent (pair-checked, no rest-of-atom noise).
inline Object* nth(Object* a, int n) {
    for (int i = 0; i < n; i++) {
        if (!a || !a->pair()) return nil;
        a = rest(a);
    }
    return (a && a->pair()) ? first(a) : nil;
}

Object* prim_vectorq(IEvaluator*, Object* a, Environment*) { return truth(is_vector(nth(a, 0))); }

Object* prim_make_vector(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1);
    if (x == NULL || x->is_nil()) return error("make-vector: requires size argument");
    int size = (int)num(x);
    if (size < 0) return error("make-vector: size must be non-negative");
    Object* fill = (y != NULL && !y->is_nil()) ? y : nil;
    return gc_new<a_vector>(size, fill);
}

Object* prim_vector(IEvaluator*, Object* a, Environment*) { return list_to_vector(a); }

Object* prim_vector_length(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0);
    if (!is_vector(x)) return error("vector-length: argument must be a vector");
    return numeric::make_integer(((a_vector*)x)->length());
}

Object* prim_vector_ref(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1);
    if (!is_vector(x)) return error("vector-ref: first argument must be a vector");
    if (y == NULL || y->is_nil()) return error("vector-ref: requires index argument");
    int i = (int)num(y);
    a_vector* vec = (a_vector*)x;
    if (i < 0 || i >= vec->length()) return error("vector-ref: index out of bounds");
    return vec->get(i);
}

Object* prim_vector_set(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *val = nth(a, 2);
    if (!is_vector(x)) return error("vector-set!: first argument must be a vector");
    if (y == NULL || y->is_nil()) return error("vector-set!: requires index argument");
    int i = (int)num(y);
    a_vector* vec = (a_vector*)x;
    if (i < 0 || i >= vec->length()) return error("vector-set!: index out of bounds");
    vec->set(i, val);
    return nil;
}

Object* prim_vector_fill(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2), *e = nth(a, 3);
    if (!is_vector(x)) return error("vector-fill!: first argument must be a vector");
    if (y == NULL || y->is_nil()) return error("vector-fill!: requires fill value argument");
    a_vector* vec = (a_vector*)x;
    int len = vec->length();
    int start = (z && !z->is_nil()) ? (int)num(z) : 0;
    int end = (e && !e->is_nil()) ? (int)num(e) : len;
    if (start < 0 || end > len || start > end) return error("vector-fill!: index out of range");
    for (int i = start; i < end; i++) vec->set(i, y);
    return nil;
}

Object* prim_vector_to_list(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2);
    if (!is_vector(x)) return error("vector->list: argument must be a vector");
    a_vector* vec = (a_vector*)x;
    int len = vec->length();
    int start = (y && !y->is_nil()) ? (int)num(y) : 0;
    int end   = (z && !z->is_nil()) ? (int)num(z) : len;
    if (start < 0 || end > len || start > end) return error("vector->list: index out of range");
    Object* result = nil;
    gc_root<Object> rguard(result);
    for (int i = end - 1; i >= start; i--) result = cons(vec->get(i), result);
    return result;
}

Object* prim_list_to_vector(IEvaluator*, Object* a, Environment*) { return list_to_vector(nth(a, 0)); }

Object* prim_vector_copy(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2);
    if (!is_vector(x)) return error("vector-copy: argument must be a vector");
    a_vector* vec = (a_vector*)x;
    int start = 0, end = vec->length();
    if (y && !y->is_nil()) {
        start = (int)num(y);
        if (start < 0 || start > vec->length()) return error("vector-copy: start index out of bounds");
        if (z && !z->is_nil()) {
            end = (int)num(z);
            if (end < start || end > vec->length()) return error("vector-copy: end index out of bounds");
        }
    }
    std::vector<Object*> elements;
    for (int i = start; i < end; i++) elements.push_back(vec->get(i));
    return gc_new<a_vector>(elements);
}

// (vector-copy! to at from [start [end]]) — overlap-safe when to/from alias.
Object* prim_vector_copy_mut(IEvaluator*, Object* a, Environment*) {
    Object* to_obj = nth(a, 0), *at_obj = nth(a, 1), *from_obj = nth(a, 2);
    Object* start_obj = nth(a, 3), *end_obj = nth(a, 4);
    if (!is_vector(to_obj) || !is_vector(from_obj)) return error("vector-copy!: to and from must be vectors");
    if (at_obj == nullptr || at_obj->is_nil()) return error("vector-copy!: requires an 'at' index");
    a_vector* to = (a_vector*)to_obj;
    a_vector* from = (a_vector*)from_obj;
    int at = (int)num(at_obj);
    int start = (start_obj && !start_obj->is_nil()) ? (int)num(start_obj) : 0;
    int end = (end_obj && !end_obj->is_nil()) ? (int)num(end_obj) : from->length();
    if (start < 0 || end > from->length() || start > end) return error("vector-copy!: source range out of bounds");
    if (at < 0 || at + (end - start) > to->length()) return error("vector-copy!: destination range out of bounds");
    if (at > start) {
        for (int i = end - start - 1; i >= 0; i--) to->set(at + i, from->get(start + i));
    } else {
        for (int i = 0; i < end - start; i++) to->set(at + i, from->get(start + i));
    }
    return nil;
}

Object* prim_vector_append(IEvaluator*, Object* a, Environment*) {
    std::vector<Object*> result;
    for (Object* cur = a; cur && !cur->is_nil(); cur = rest(cur)) {
        Object* v = first(cur);
        if (!is_vector(v)) return error("vector-append: all arguments must be vectors");
        a_vector* vec = (a_vector*)v;
        for (int i = 0; i < vec->length(); i++) result.push_back(vec->get(i));
    }
    return gc_new<a_vector>(result);
}

// Shared body for vector-map (collect results) and vector-for-each (discard).
Object* vec_map_foreach(IEvaluator* eval, Object* args, Environment* env,
                        bool collect, const char* who) {
    if (!eval) return error(std::string(who) + ": requires evaluator");
    Object* proc = first(args);
    Object* vec_args = rest(args);
    if (!vec_args || vec_args->is_nil()) return error(std::string(who) + ": requires at least one vector");
    Object* first_vec = first(vec_args);
    if (!is_vector(first_vec)) return error(std::string(who) + ": arguments must be vectors");
    int len = ((a_vector*)first_vec)->length();
    for (Object* c = rest(vec_args); c && !c->is_nil(); c = rest(c)) {
        Object* v = first(c);
        if (!is_vector(v)) return error(std::string(who) + ": arguments must be vectors");
        if (((a_vector*)v)->length() != len) return error(std::string(who) + ": all vectors must have same length");
    }
    Procedure* p = dynamic_cast<Procedure*>(proc);
    if (!p) return error(std::string(who) + ": first argument must be a Procedure");

    std::vector<Object*> result_elements;
    for (int i = 0; i < len; i++) {
        Object* proc_args = nil, *tail = nil;
        for (Object* vl = vec_args; vl && !vl->is_nil(); vl = rest(vl)) {
            Object* cell = cons(((a_vector*)first(vl))->get(i), nil);
            if (proc_args == nil) { proc_args = cell; tail = cell; }
            else { set_rest(tail, cell); tail = cell; }
        }
        Object* r = p->apply(eval, proc_args, env);
        if (collect) result_elements.push_back(r);
    }
    if (!collect) return nil;
    return gc_new<a_vector>(result_elements);
}
Object* prim_vector_map(IEvaluator* e, Object* a, Environment* v) { return vec_map_foreach(e, a, v, true, "vector-map"); }
Object* prim_vector_for_each(IEvaluator* e, Object* a, Environment* v) { return vec_map_foreach(e, a, v, false, "vector-for-each"); }

Object* prim_string_to_vector(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2);
    if (!is_string(x)) return error("string->vector: argument must be a string");
    std::string s = static_cast<a_string*>(x)->value();
    int start = 0, end = (int)s.length();
    if (y && !y->is_nil()) {
        start = (int)num(y);
        if (start < 0 || start > (int)s.length()) return error("string->vector: start index out of bounds");
        if (z && !z->is_nil()) {
            end = (int)num(z);
            if (end < start || end > (int)s.length()) return error("string->vector: end index out of bounds");
        }
    }
    std::vector<Object*> elements;
    for (int i = start; i < end; i++) elements.push_back(gc_new<a_char>(s[i]));
    return gc_new<a_vector>(elements);
}

Object* prim_vector_to_string(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2);
    if (!is_vector(x)) return error("vector->string: argument must be a vector");
    a_vector* vec = (a_vector*)x;
    int start = 0, end = vec->length();
    if (y && !y->is_nil()) {
        start = (int)num(y);
        if (start < 0 || start > vec->length()) return error("vector->string: start index out of bounds");
        if (z && !z->is_nil()) {
            end = (int)num(z);
            if (end < start || end > vec->length()) return error("vector->string: end index out of bounds");
        }
    }
    std::string result;
    for (int i = start; i < end; i++) {
        Object* elem = vec->get(i);
        if (!is_char(elem)) return error("vector->string: vector must contain only characters");
        result += dynamic_cast<a_char*>(elem)->value();
    }
    return gc_new<a_string>(result);
}

}  // namespace

void PrimitiveVectors::install(zombie::runtime::Environment* env) {
    using namespace zombie::runtime;
    static const NativeEntry entries[] = {
        { "vector?",         1, 1,      prim_vectorq          },
        { "make-vector",     1, 2,      prim_make_vector      },
        { "vector",          0, 999999, prim_vector           },
        { "vector-length",   1, 1,      prim_vector_length    },
        { "vector-ref",      2, 2,      prim_vector_ref       },
        { "vector-set!",     3, 3,      prim_vector_set       },
        { "vector-fill!",    2, 4,      prim_vector_fill      },
        { "vector->list",    1, 3,      prim_vector_to_list   },
        { "list->vector",    1, 1,      prim_list_to_vector   },
        { "vector-copy",     1, 3,      prim_vector_copy      },
        { "vector-copy!",    3, 5,      prim_vector_copy_mut  },
        { "vector-append",   0, 999999, prim_vector_append    },
        { "vector-map",      2, 999999, prim_vector_map       },
        { "vector-for-each", 2, 999999, prim_vector_for_each  },
        { "string->vector",  1, 3,      prim_string_to_vector },
        { "vector->string",  1, 3,      prim_vector_to_string },
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}
