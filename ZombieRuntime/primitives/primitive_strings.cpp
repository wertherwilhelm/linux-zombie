#include "zombie_runtime_types.h"
#include "primitive_strings.h"
#include "../native_primitive.h"
#include "../Base.h"
#include "../atom.h"
#include "../numeric_runtime.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>

/**
 * @file primitive_strings.cpp
 * @brief String primitives (R7RS 6.7), table-driven.
 */

namespace {

using namespace zombie::runtime;

// nth argument (0-based), or nil if absent (pair-checked).
inline Object* nth(Object* a, int n) {
    for (int i = 0; i < n; i++) {
        if (!a || !a->pair()) return nil;
        a = rest(a);
    }
    return (a && a->pair()) ? first(a) : nil;
}

Object* string_append(Object* args) {
    std::string result;
    for (Object* arg = args; arg != NULL && !arg->is_nil(); arg = rest(arg)) {
        Object* s = first(arg);
        if (!is_string(s)) return error("string-append: all arguments must be strings");
        result += dynamic_cast<a_string*>(s)->value();
    }
    return gc_new<a_string>(result);
}

Object* chars_to_string(Object* args) {
    std::string result;
    for (Object* arg = args; arg != NULL && !arg->is_nil(); arg = rest(arg)) {
        Object* c = first(arg);
        if (!is_char(c)) return error("string: all arguments must be characters");
        result += dynamic_cast<a_char*>(c)->value();
    }
    return gc_new<a_string>(result);
}

// -1 / 0 / 1 for s1 </=/> s2 (optionally case-insensitive).
int stringCompare(Object* s1, Object* s2, bool ci) {
    if (!is_string(s1) || !is_string(s2)) { error("stringCompare: both arguments must be strings"); return 0; }
    std::string a = dynamic_cast<a_string*>(s1)->value();
    std::string b = dynamic_cast<a_string*>(s2)->value();
    if (ci) {
        std::transform(a.begin(), a.end(), a.begin(), ::tolower);
        std::transform(b.begin(), b.end(), b.begin(), ::tolower);
    }
    int r = a.compare(b);
    return (r < 0) ? -1 : (r > 0) ? 1 : 0;
}

// Variadic comparison chain (R7RS 6.7). op: 0 '=', 1 '<', 2 '>', 3 '<=', 4 '>='.
Object* stringCompareChain(Object* args, bool ci, int op) {
    Object* prev = first(args);
    for (Object* q = rest(args); q && !q->is_nil(); q = rest(q)) {
        Object* cur = first(q);
        int c = stringCompare(prev, cur, ci);
        bool ok = (op == 0) ? (c == 0) : (op == 1) ? (c < 0) : (op == 2) ? (c > 0)
                : (op == 3) ? (c <= 0) : (c >= 0);
        if (!ok) return static_cast<Object*>(false_obj);
        prev = cur;
    }
    return t;
}

Object* prim_stringq(IEvaluator*, Object* a, Environment*) { return truth(is_string(nth(a, 0))); }

Object* prim_make_string(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1);
    if (x == NULL || x->is_nil()) return error("make-string: requires size argument");
    int size = (int)num(x);
    if (size < 0) return error("make-string: size must be non-negative");
    char fill = ' ';
    if (y != NULL && !y->is_nil()) { a_char* ch = dynamic_cast<a_char*>(y); if (ch) fill = ch->value(); }
    return gc_new<a_string>(std::string(size, fill));
}

Object* prim_string(IEvaluator*, Object* a, Environment*) { return chars_to_string(a); }

Object* prim_string_length(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0);
    if (!is_string(x)) return error("string-length: argument must be a string");
    return numeric::make_integer(dynamic_cast<a_string*>(x)->value().length());
}

Object* prim_string_ref(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1);
    if (!is_string(x)) return error("string-ref: first argument must be a string");
    if (y == NULL || y->is_nil()) return error("string-ref: requires index argument");
    a_string* str = dynamic_cast<a_string*>(x);
    int i = (int)num(y);
    if (i < 0 || i >= (int)str->value().length()) return error("string-ref: index out of bounds");
    return gc_new<a_char>(str->value()[i]);
}

Object* prim_string_set(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2);
    if (!is_string(x)) return error("string-set!: first argument must be a string");
    if (y == NULL || y->is_nil()) return error("string-set!: requires index argument");
    a_string* str = dynamic_cast<a_string*>(x);
    int i = (int)num(y);
    if (i < 0 || i >= (int)str->value().length()) return error("string-set!: index out of bounds");
    if (z == NULL || z->is_nil()) return error("string-set!: requires character argument");
    a_char* ch = dynamic_cast<a_char*>(z);
    if (!ch) return error("string-set!: third argument must be a character");
    str->value()[i] = ch->value();
    return str;
}

Object* prim_substring(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2);
    if (!is_string(x)) return error("substring: first argument must be a string");
    if (y == NULL || y->is_nil()) return error("substring: requires start index");
    if (z == NULL || z->is_nil()) return error("substring: requires end index");
    a_string* str = dynamic_cast<a_string*>(x);
    int start = (int)num(y), end = (int)num(z);
    if (start < 0 || start > (int)str->value().length()) return error("substring: start index out of bounds");
    if (end < start || end > (int)str->value().length()) return error("substring: end index out of bounds");
    return gc_new<a_string>(str->value().substr(start, end - start));
}

Object* prim_string_append(IEvaluator*, Object* a, Environment*) { return string_append(a); }

Object* prim_string_copy(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2);
    if (!is_string(x)) return error("string-copy: argument must be a string");
    const std::string& s = dynamic_cast<a_string*>(x)->value();
    int len = (int)s.length();
    int start = (y && !y->is_nil()) ? (int)numeric::to_int64(y) : 0;
    int end   = (z && !z->is_nil()) ? (int)numeric::to_int64(z) : len;
    if (start < 0 || end > len || start > end) return error("string-copy: index out of range");
    return gc_new<a_string>(s.substr(start, end - start));
}

// upcase / downcase / foldcase (foldcase == downcase for ASCII).
Object* string_case(Object* a, const char* who, int (*fn)(int)) {
    Object* x = nth(a, 0);
    if (!is_string(x)) return error(std::string(who) + ": argument must be a string");
    std::string r = dynamic_cast<a_string*>(x)->value();
    std::transform(r.begin(), r.end(), r.begin(), fn);
    return gc_new<a_string>(r);
}
Object* prim_string_upcase(IEvaluator*, Object* a, Environment*) { return string_case(a, "string-upcase", ::toupper); }
Object* prim_string_downcase(IEvaluator*, Object* a, Environment*) { return string_case(a, "string-downcase", ::tolower); }
Object* prim_string_foldcase(IEvaluator*, Object* a, Environment*) { return string_case(a, "string-foldcase", ::tolower); }

Object* prim_string_fill(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2), *e = nth(a, 3);
    if (!is_string(x)) return error("string-fill!: first argument must be a string");
    if (!is_char(y)) return error("string-fill!: second argument must be a character");
    a_string* str = (a_string*)x;
    char fill = ((a_char*)y)->value();
    std::string& sv = str->value();
    int len = (int)sv.length();
    int start = (z && !z->is_nil()) ? (int)num(z) : 0;
    int end = (e && !e->is_nil()) ? (int)num(e) : len;
    if (start < 0 || end > len || start > end) return error("string-fill!: index out of range");
    std::fill(sv.begin() + start, sv.begin() + end, fill);
    return str;
}

// (string-copy! to at from [start [end]]) — overlap-safe.
Object* prim_string_copy_mut(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *at_obj = nth(a, 1), *from_obj = nth(a, 2);
    Object* start_obj = nth(a, 3), *end_obj = nth(a, 4);
    if (!is_string(x)) return error("string-copy!: 'to' must be a string");
    if (at_obj == nullptr || at_obj->is_nil() || !is_string(from_obj))
        return error("string-copy!: expected (to at from [start [end]])");
    a_string* to = (a_string*)x;
    std::string& tov = to->value();
    const std::string& fromv = ((a_string*)from_obj)->value();
    int at = (int)num(at_obj);
    int fl = (int)fromv.length();
    int start = (start_obj && !start_obj->is_nil()) ? (int)num(start_obj) : 0;
    int end = (end_obj && !end_obj->is_nil()) ? (int)num(end_obj) : fl;
    if (start < 0 || end > fl || start > end) return error("string-copy!: source range out of bounds");
    if (at < 0 || at + (end - start) > (int)tov.length()) return error("string-copy!: destination range out of bounds");
    if (at > start) { for (int i = end - start - 1; i >= 0; --i) tov[at + i] = fromv[start + i]; }
    else            { for (int i = 0; i < end - start; ++i) tov[at + i] = fromv[start + i]; }
    return to;
}

Object* prim_string_to_list(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1), *z = nth(a, 2);
    if (!is_string(x)) return error("string->list: argument must be a string");
    const std::string& s = dynamic_cast<a_string*>(x)->value();
    int len = (int)s.length();
    int start = (y && !y->is_nil()) ? (int)numeric::to_int64(y) : 0;
    int end   = (z && !z->is_nil()) ? (int)numeric::to_int64(z) : len;
    if (start < 0 || end > len || start > end) return error("string->list: index out of range");
    Object* result = nil;
    gc_root<Object> guard(result);
    for (int i = end - 1; i >= start; i--) result = cons(gc_new<a_char>(s[i]), result);
    return result;
}

Object* prim_list_to_string(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0);
    if (x == NULL || x->is_nil()) return gc_new<a_string>("");
    std::string result;
    for (Object* item = x; item != NULL && !item->is_nil(); item = rest(item)) {
        Object* c = first(item);
        if (!is_char(c)) return error("list->string: all list elements must be characters");
        result += dynamic_cast<a_char*>(c)->value();
    }
    return gc_new<a_string>(result);
}

Object* prim_string_eq(IEvaluator*, Object* a, Environment*) { return stringCompareChain(a, false, 0); }
Object* prim_string_lt(IEvaluator*, Object* a, Environment*) { return stringCompareChain(a, false, 1); }
Object* prim_string_gt(IEvaluator*, Object* a, Environment*) { return stringCompareChain(a, false, 2); }
Object* prim_string_le(IEvaluator*, Object* a, Environment*) { return stringCompareChain(a, false, 3); }
Object* prim_string_ge(IEvaluator*, Object* a, Environment*) { return stringCompareChain(a, false, 4); }
Object* prim_string_ci_eq(IEvaluator*, Object* a, Environment*) { return stringCompareChain(a, true, 0); }
Object* prim_string_ci_lt(IEvaluator*, Object* a, Environment*) { return stringCompareChain(a, true, 1); }
Object* prim_string_ci_gt(IEvaluator*, Object* a, Environment*) { return stringCompareChain(a, true, 2); }
Object* prim_string_ci_le(IEvaluator*, Object* a, Environment*) { return stringCompareChain(a, true, 3); }
Object* prim_string_ci_ge(IEvaluator*, Object* a, Environment*) { return stringCompareChain(a, true, 4); }

// Shared body for string-map (build a string from char results) and
// string-for-each (apply for side effects).
Object* string_map_foreach(IEvaluator* eval, Object* args, Environment* env,
                           bool collect, const char* who) {
    if (!eval) return error(std::string(who) + ": requires evaluator");
    Object* proc = first(args);
    Object* str_args = rest(args);
    if (!str_args || str_args->is_nil()) return error(std::string(who) + ": requires at least one string");
    Object* first_str = first(str_args);
    if (!is_string(first_str)) return error(std::string(who) + ": arguments must be strings");
    size_t len = ((a_string*)first_str)->value().length();
    for (Object* c = rest(str_args); c && !c->is_nil(); c = rest(c)) {
        Object* s = first(c);
        if (!is_string(s)) return error(std::string(who) + ": arguments must be strings");
        if (((a_string*)s)->value().length() != len) return error(std::string(who) + ": all strings must have same length");
    }
    Procedure* p = dynamic_cast<Procedure*>(proc);
    if (!p) return error(std::string(who) + ": first argument must be a Procedure");

    std::string result_str;
    for (size_t i = 0; i < len; i++) {
        Object* proc_args = nil, *tail = nil;
        for (Object* sl = str_args; sl && !sl->is_nil(); sl = rest(sl)) {
            Object* cell = cons(gc_new<a_char>(((a_string*)first(sl))->value()[i]), nil);
            if (proc_args == nil) { proc_args = cell; tail = cell; }
            else { set_rest(tail, cell); tail = cell; }
        }
        Object* r = p->apply(eval, proc_args, env);
        if (collect) {
            if (!is_char(r)) return error(std::string(who) + ": procedure must return a character");
            result_str += ((a_char*)r)->value();
        }
    }
    if (!collect) return nil;
    return gc_new<a_string>(result_str);
}
Object* prim_string_map(IEvaluator* e, Object* a, Environment* v) { return string_map_foreach(e, a, v, true, "string-map"); }
Object* prim_string_for_each(IEvaluator* e, Object* a, Environment* v) { return string_map_foreach(e, a, v, false, "string-for-each"); }

// (string-split str delim-char) -> list of substrings between delimiters.
Object* prim_string_split(IEvaluator*, Object* a, Environment*) {
    Object* x = nth(a, 0), *y = nth(a, 1);
    a_string* s = dynamic_cast<a_string*>(x);
    if (!s) return error("string-split: first argument must be a string");
    if (!is_char(y)) return error("string-split: delimiter must be a character");
    char delim = ((a_char*)y)->value();
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s->value()) {
        if (c == delim) { parts.push_back(cur); cur.clear(); }
        else cur += c;
    }
    parts.push_back(cur);
    Object* result = nil;
    gc_root<Object> guard(result);
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
        result = cons(gc_new<a_string>(*it), result);
        guard = gc_root<Object>(result);
    }
    return result;
}

// (string-contains haystack needle) -> index of first match, or #f.
Object* prim_string_contains(IEvaluator*, Object* a, Environment*) {
    a_string* hs = dynamic_cast<a_string*>(nth(a, 0));
    a_string* nd = dynamic_cast<a_string*>(nth(a, 1));
    if (!hs || !nd) return error("string-contains: expected two strings");
    size_t pos = hs->value().find(nd->value());
    if (pos == std::string::npos) return static_cast<Object*>(false_obj);
    return numeric::make_integer(static_cast<long long>(pos));
}

}  // namespace

void PrimitiveStrings::install(zombie::runtime::Environment* env) {
    using namespace zombie::runtime;
    static const NativeEntry entries[] = {
        { "string?",        1, 1,      prim_stringq         },
        { "make-string",    1, 2,      prim_make_string     },
        { "string",         0, 999999, prim_string          },
        { "string-length",  1, 1,      prim_string_length   },
        { "string-ref",     2, 2,      prim_string_ref      },
        { "string-set!",    3, 3,      prim_string_set      },
        { "substring",      3, 3,      prim_substring       },
        { "string-append",  0, 999999, prim_string_append   },
        { "string-copy",    1, 3,      prim_string_copy     },
        { "string-upcase",  1, 1,      prim_string_upcase   },
        { "string-downcase",1, 1,      prim_string_downcase },
        { "string-foldcase",1, 1,      prim_string_foldcase },
        { "string-fill!",   2, 4,      prim_string_fill     },
        { "string-copy!",   3, 5,      prim_string_copy_mut },
        { "string->list",   1, 3,      prim_string_to_list  },
        { "list->string",   1, 1,      prim_list_to_string  },
        { "string=?",       2, 999999, prim_string_eq       },
        { "string<?",       2, 999999, prim_string_lt       },
        { "string>?",       2, 999999, prim_string_gt       },
        { "string<=?",      2, 999999, prim_string_le       },
        { "string>=?",      2, 999999, prim_string_ge       },
        { "string-ci=?",    2, 999999, prim_string_ci_eq    },
        { "string-ci<?",    2, 999999, prim_string_ci_lt    },
        { "string-ci>?",    2, 999999, prim_string_ci_gt    },
        { "string-ci<=?",   2, 999999, prim_string_ci_le    },
        { "string-ci>=?",   2, 999999, prim_string_ci_ge    },
        { "string-map",     2, 999999, prim_string_map      },
        { "string-for-each",2, 999999, prim_string_for_each },
        { "string-split",   2, 2,      prim_string_split    },
        { "string-contains",2, 2,      prim_string_contains },
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}
