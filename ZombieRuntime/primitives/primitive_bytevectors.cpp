#include "zombie_runtime_types.h"
#include "primitive_bytevectors.h"
#include "../native_primitive.h"
#include "../a_bytevector.h"
#include "../atom.h"
#include "../Base.h"
#include "../cons_cell.h"
#include "../constants.h"
#include "../numeric_runtime.h"
#include <vector>
#include <algorithm>

/**
 * @file primitive_bytevectors.cpp
 * @brief Bytevector primitives (R7RS 6.9), table-driven.
 */

namespace {

using namespace zombie::runtime;

// nth argument (0-based), or nil if absent. Checks pair() to avoid the spurious
// "cannot take rest of an atom!" stderr from rest() on a nil tail.
inline Object* nth(Object* a, int n) {
    for (int i = 0; i < n; i++) {
        if (!a || !a->pair()) return nil;
        a = rest(a);
    }
    return (a && a->pair()) ? first(a) : nil;
}

Object* prim_bytevectorq(IEvaluator*, Object* a, Environment*) {
    return is_bytevector(nth(a, 0)) ? t : static_cast<Object*>(false_obj);
}

Object* prim_make_bytevector(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1);
    if (!numeric::is_number(x)) return error("make-bytevector: k must be a number");
    int64_t k = numeric::to_int64(x);
    if (k < 0) return error("make-bytevector: k must be non-negative");
    uint8_t fill = 0;
    if (y && !y->is_nil()) {
        if (!numeric::is_number(y)) return error("make-bytevector: fill must be a number");
        int64_t b = numeric::to_int64(y);
        if (b < 0 || b > 255) return error("make-bytevector: fill must be in range [0, 255]");
        fill = static_cast<uint8_t>(b);
    }
    return gc_new<a_bytevector>(static_cast<int>(k), fill);
}

Object* prim_bytevector(IEvaluator*, Object* a, Environment*) {
    std::vector<uint8_t> bytes;
    for (Object* cur = a; cur && !cur->is_nil(); cur = cdr(cur)) {
        Object* b = car(cur);
        if (!numeric::is_number(b)) return error("bytevector: all arguments must be numbers");
        int64_t v = numeric::to_int64(b);
        if (v < 0 || v > 255) return error("bytevector: bytes must be in range [0, 255]");
        bytes.push_back(static_cast<uint8_t>(v));
    }
    return gc_new<a_bytevector>(bytes);
}

Object* prim_bytevector_length(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0);
    if (!is_bytevector(x)) return error("bytevector-length: argument must be a bytevector");
    return numeric::make_integer(static_cast<a_bytevector*>(x)->length());
}

Object* prim_bytevector_u8_ref(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1);
    if (!is_bytevector(x)) return error("bytevector-u8-ref: first argument must be a bytevector");
    if (!numeric::is_number(y)) return error("bytevector-u8-ref: index must be a number");
    a_bytevector* bv = static_cast<a_bytevector*>(x);
    int64_t k = numeric::to_int64(y);
    if (k < 0 || k >= bv->length()) return error("bytevector-u8-ref: index out of range");
    return numeric::make_integer(bv->get(static_cast<int>(k)));
}

Object* prim_bytevector_u8_set(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2);
    if (!is_bytevector(x)) return error("bytevector-u8-set!: first argument must be a bytevector");
    if (!numeric::is_number(y)) return error("bytevector-u8-set!: index must be a number");
    if (!numeric::is_number(z)) return error("bytevector-u8-set!: byte must be a number");
    a_bytevector* bv = static_cast<a_bytevector*>(x);
    int64_t k = numeric::to_int64(y), b = numeric::to_int64(z);
    if (k < 0 || k >= bv->length()) return error("bytevector-u8-set!: index out of range");
    if (b < 0 || b > 255) return error("bytevector-u8-set!: byte must be in range [0, 255]");
    bv->set(static_cast<int>(k), static_cast<uint8_t>(b));
    return bv;
}

Object* prim_bytevector_copy(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2);
    if (!is_bytevector(x)) return error("bytevector-copy: argument must be a bytevector");
    a_bytevector* bv = static_cast<a_bytevector*>(x);
    int start = 0, end = bv->length();
    if (y && !y->is_nil()) {
        if (!numeric::is_number(y)) return error("bytevector-copy: start must be a number");
        start = static_cast<int>(numeric::to_int64(y));
        if (z && !z->is_nil()) {
            if (!numeric::is_number(z)) return error("bytevector-copy: end must be a number");
            end = static_cast<int>(numeric::to_int64(z));
        }
    }
    if (start < 0 || start > bv->length() || end < start || end > bv->length())
        return error("bytevector-copy: invalid start/end indices");
    std::vector<uint8_t> copy(bv->bytes().begin() + start, bv->bytes().begin() + end);
    return gc_new<a_bytevector>(copy);
}

// (bytevector-copy! to at from [start [end]])
Object* prim_bytevector_copy_mut(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2);
    if (!is_bytevector(x)) return error("bytevector-copy!: 'to' must be a bytevector");
    if (!numeric::is_number(y)) return error("bytevector-copy!: 'at' must be a number");
    if (!is_bytevector(z)) return error("bytevector-copy!: 'from' must be a bytevector");
    a_bytevector* to_bv = static_cast<a_bytevector*>(x);
    a_bytevector* from_bv = static_cast<a_bytevector*>(z);
    int at = static_cast<int>(numeric::to_int64(y));
    int start = 0, end = from_bv->length();
    Object* s = nth(a, 3), *e = nth(a, 4);
    if (s && !s->is_nil()) {
        if (!numeric::is_number(s)) return error("bytevector-copy!: start must be a number");
        start = static_cast<int>(numeric::to_int64(s));
        if (e && !e->is_nil()) {
            if (!numeric::is_number(e)) return error("bytevector-copy!: end must be a number");
            end = static_cast<int>(numeric::to_int64(e));
        }
    }
    if (start < 0 || start > from_bv->length() || end < start || end > from_bv->length())
        return error("bytevector-copy!: invalid start/end indices");
    int count = end - start;
    if (at < 0 || at + count > to_bv->length())
        return error("bytevector-copy!: invalid 'at' index or insufficient space");
    std::copy(from_bv->bytes().begin() + start, from_bv->bytes().begin() + end,
              to_bv->bytes().begin() + at);
    return to_bv;
}

Object* prim_bytevector_append(IEvaluator*, Object* a, Environment*) {
    std::vector<uint8_t> result;
    for (Object* cur = a; cur && !cur->is_nil(); cur = cdr(cur)) {
        Object* b = car(cur);
        if (!is_bytevector(b)) return error("bytevector-append: all arguments must be bytevectors");
        a_bytevector* bv = static_cast<a_bytevector*>(b);
        result.insert(result.end(), bv->bytes().begin(), bv->bytes().end());
    }
    return gc_new<a_bytevector>(result);
}

Object* prim_utf8_to_string(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2);
    if (!is_bytevector(x)) return error("utf8->string: argument must be a bytevector");
    a_bytevector* bv = static_cast<a_bytevector*>(x);
    int start = 0, end = bv->length();
    if (y && !y->is_nil()) {
        if (!numeric::is_number(y)) return error("utf8->string: start must be a number");
        start = static_cast<int>(numeric::to_int64(y));
        if (z && !z->is_nil()) {
            if (!numeric::is_number(z)) return error("utf8->string: end must be a number");
            end = static_cast<int>(numeric::to_int64(z));
        }
    }
    if (start < 0 || start > bv->length() || end < start || end > bv->length())
        return error("utf8->string: invalid start/end indices");
    std::string str(bv->bytes().begin() + start, bv->bytes().begin() + end);
    return gc_new<a_string>(str);
}

Object* prim_string_to_utf8(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2);
    if (x->get_type() != static_cast<int>(Type::STRING))
        return error("string->utf8: argument must be a string");
    const std::string& s = static_cast<a_string*>(x)->value();
    int start = 0, end = static_cast<int>(s.length());
    if (y && !y->is_nil()) {
        if (!numeric::is_number(y)) return error("string->utf8: start must be a number");
        start = static_cast<int>(numeric::to_int64(y));
        if (z && !z->is_nil()) {
            if (!numeric::is_number(z)) return error("string->utf8: end must be a number");
            end = static_cast<int>(numeric::to_int64(z));
        }
    }
    if (start < 0 || start > (int)s.length() || end < start || end > (int)s.length())
        return error("string->utf8: invalid start/end indices");
    std::vector<uint8_t> bytes(s.begin() + start, s.begin() + end);
    return gc_new<a_bytevector>(bytes);
}

}  // namespace

void PrimitiveBytevectors::install(zombie::runtime::Environment* env) {
    using namespace zombie::runtime;
    static const NativeEntry entries[] = {
        { "bytevector?",       1, 1,      prim_bytevectorq         },
        { "make-bytevector",   1, 2,      prim_make_bytevector     },
        { "bytevector",        0, 999999, prim_bytevector          },
        { "bytevector-length", 1, 1,      prim_bytevector_length   },
        { "bytevector-u8-ref", 2, 2,      prim_bytevector_u8_ref   },
        { "bytevector-u8-set!",3, 3,      prim_bytevector_u8_set   },
        { "bytevector-copy",   1, 3,      prim_bytevector_copy     },
        { "bytevector-copy!",  3, 5,      prim_bytevector_copy_mut },
        { "bytevector-append", 0, 999999, prim_bytevector_append   },
        { "utf8->string",      1, 3,      prim_utf8_to_string      },
        { "string->utf8",      1, 3,      prim_string_to_utf8      },
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}
