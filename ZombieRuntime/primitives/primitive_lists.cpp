#include "zombie_runtime_types.h"
#include "primitive_lists.h"
#include "../native_primitive.h"
#include "../Base.h"
#include "../cons_cell.h"
#include "../atom.h"
#include "../numeric_runtime.h"
#include "../procedure.h"
#include "../i_evaluator.h"
#include <cstring>

/**
 * @file primitive_lists.cpp
 * @brief List / pair primitives (R7RS 6.4), table-driven.
 */

namespace {

using namespace zombie::runtime;

// member/assoc with built-in equality. m: 'm' member (whole tail on match),
// 'a' assoc (matching pair). eq: 'q' eq?, 'v' eqv?, ' ' equal?.
Object* member_assoc(Object* obj, Object* List, char m, char eq) {
    while (!List->is_atom()) {
        Object* target = (m == 'm') ? first(List) : first(first(List));
        bool found;
        switch (eq) {
            case 'q': found = ::eq(target, obj); break;
            case 'v': found = eqv(target, obj); break;
            case ' ': found = equal(target, obj); break;
            default:  warn("Bad option to member_assoc:" + eq); return false_obj;
        }
        if (found) return (m == 'm') ? List : first(List);
        List = rest(List);
    }
    return false_obj;
}

// member/assoc with a user comparator (R7RS): called as (compare obj element).
Object* member_assoc_cmp(IEvaluator* eval, Environment* env,
                         Object* obj, Object* list, Object* cmp, char m) {
    Procedure* proc = dynamic_cast<Procedure*>(cmp);
    if (!proc) return error("member/assoc: comparator must be a procedure");
    gc_root<Object> lguard(list);
    gc_root<Object> oguard(obj);
    while (!list->is_atom()) {
        Object* target = (m == 'm') ? first(list) : first(first(list));
        Object* callargs = cons(obj, cons(target, nil));
        gc_root<Object> aguard(callargs);
        Object* res = proc->apply(eval, callargs, env);
        if (truth(res)) return (m == 'm') ? list : first(list);
        list = rest(list);
    }
    return false_obj;
}

// cXr: the pattern is read right to left ("cadr" = cdr then car). Out-of-bounds
// access (nil/atom) yields nil.
Object* cxr(Object* x, const char* name) {
    for (int i = static_cast<int>(std::strlen(name)) - 2; i >= 1; i--) {
        if (x == nil || x->is_nil() || x->is_atom()) return nil;
        x = (name[i] == 'a') ? first(x) : rest(x);
    }
    return x;
}

// Helpers to pull args that may be absent.
inline Object* arg1(Object* a) { return (a && !a->is_nil()) ? first(a) : nil; }
inline Object* arg2(Object* a) { return (a && !a->is_nil() && rest(a) && !rest(a)->is_nil()) ? second(a) : nil; }
inline Object* arg3(Object* a) {
    Object* r2 = (a && !a->is_nil() && rest(a) && !rest(a)->is_nil()) ? rest(rest(a)) : nil;
    return (r2 && r2->pair()) ? first(r2) : nil;
}

Object* prim_pairq(IEvaluator*, Object* a, Environment*) { return truth(!arg1(a)->is_atom()); }
Object* prim_listq(IEvaluator*, Object* a, Environment*) { return truth(::is_list(arg1(a))); }
Object* prim_nullq(IEvaluator*, Object* a, Environment*) { Object* x=arg1(a); return truth(x == NULL || x == nil || x->is_nil()); }
Object* prim_cons(IEvaluator*, Object* a, Environment*) { return cons(arg1(a), arg2(a)); }
Object* prim_car(IEvaluator*, Object* a, Environment*) { return first(arg1(a)); }
Object* prim_cdr(IEvaluator*, Object* a, Environment*) { return rest(arg1(a)); }
Object* prim_set_car(IEvaluator*, Object* a, Environment*) { return set_first(arg1(a), arg2(a)); }
Object* prim_set_cdr(IEvaluator*, Object* a, Environment*) { return set_rest(arg1(a), arg2(a)); }
Object* prim_second(IEvaluator*, Object* a, Environment*) { return second(arg1(a)); }
Object* prim_third(IEvaluator*, Object* a, Environment*) { return third(arg1(a)); }
Object* prim_list(IEvaluator*, Object* a, Environment*) { return a; }

Object* prim_length(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (x == NULL || x->is_nil()) return numeric::make_integer(0);
    return numeric::make_integer(length(x));
}

Object* prim_append(IEvaluator*, Object* a, Environment*) {
    if (a == NULL || a->is_nil()) return nil;
    Object* result = first(a);
    for (Object* rem = rest(a); rem != NULL && !rem->is_nil(); rem = rest(rem)) {
        result = append(result, first(rem));
    }
    return result;
}

Object* prim_reverse(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    return (x == NULL || x->is_nil()) ? nil : reverse(x);
}

Object* prim_last_pair(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (x == NULL || x->is_atom()) return error("last-pair: argument must be a pair");
    while (!rest(x)->is_atom()) x = rest(x);
    return x;
}

Object* prim_list_tail(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    int k = (int)(((a_int*)arg2(a))->value());
    for (; k > 0; k--) x = rest(x);
    return x;
}

Object* prim_list_ref(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a), *y = arg2(a);
    if (x == NULL || x->is_nil()) return error("list-ref: list is empty");
    if (y == NULL || y->is_nil()) return error("list-ref: index required");
    int k = (int)num(y);
    if (k < 0) return error("list-ref: index must be non-negative");
    Object* cur = x;
    for (int i = 0; i < k; i++) {
        if (cur == NULL || cur->is_nil()) return error("list-ref: index out of bounds");
        cur = rest(cur);
    }
    if (cur == NULL || cur->is_nil()) return error("list-ref: index out of bounds");
    return first(cur);
}

Object* prim_memq(IEvaluator*, Object* a, Environment*) { return member_assoc(arg1(a), arg2(a), 'm', 'q'); }
Object* prim_memv(IEvaluator*, Object* a, Environment*) { return member_assoc(arg1(a), arg2(a), 'm', 'v'); }
Object* prim_member(IEvaluator* e, Object* a, Environment* v) {
    Object* z = arg3(a);
    if (z && !z->is_nil()) return member_assoc_cmp(e, v, arg1(a), arg2(a), z, 'm');
    return member_assoc(arg1(a), arg2(a), 'm', ' ');
}
Object* prim_assq(IEvaluator*, Object* a, Environment*) { return member_assoc(arg1(a), arg2(a), 'a', 'q'); }
Object* prim_assv(IEvaluator*, Object* a, Environment*) { return member_assoc(arg1(a), arg2(a), 'a', 'v'); }
Object* prim_assoc(IEvaluator* e, Object* a, Environment* v) {
    Object* z = arg3(a);
    if (z && !z->is_nil()) return member_assoc_cmp(e, v, arg1(a), arg2(a), z, 'a');
    return member_assoc(arg1(a), arg2(a), 'a', ' ');
}

Object* prim_make_list(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a), *y = arg2(a);
    if (x == NULL || x->is_nil()) return error("make-list: requires integer argument");
    int k = (int)num(x);
    if (k < 0) return error("make-list: count must be non-negative");
    Object* fill = (y && !y->is_nil()) ? y : nil;
    Object* result = nil;
    for (int i = 0; i < k; i++) result = cons(fill, result);
    return result;
}

Object* prim_list_copy(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a);
    if (x == NULL || x->is_nil()) return nil;
    Object* result = nil, *tail = nil, *cur = x;
    while (cur && !cur->is_nil()) {
        if (!cur->pair()) {  // improper list: attach the dotted tail
            if (tail) set_rest(tail, cur); else result = cur;
            break;
        }
        Object* cell = cons(first(cur), nil);
        if (result == nil) { result = cell; tail = cell; }
        else { set_rest(tail, cell); tail = cell; }
        cur = rest(cur);
    }
    return result;
}

Object* prim_list_set(IEvaluator*, Object* a, Environment*) {
    Object* x = arg1(a), *y = arg2(a);
    if (x == NULL || x->is_nil()) return error("list-set!: list is empty");
    if (y == NULL || y->is_nil()) return error("list-set!: index required");
    int k = (int)num(y);
    if (k < 0) return error("list-set!: index must be non-negative");
    Object* obj = arg3(a);
    Object* cur = x;
    for (int i = 0; i < k; i++) {
        if (cur == NULL || cur->is_nil() || !cur->pair()) return error("list-set!: index out of bounds");
        cur = rest(cur);
    }
    if (cur == NULL || cur->is_nil() || !cur->pair()) return error("list-set!: index out of bounds");
    set_first(cur, obj);
    return nil;
}

// The cXr family (R7RS 6.4): two-, three-, and four-level compositions.
#define CXR_LIST(X) \
    X(caar) X(cadr) X(cdar) X(cddr) \
    X(caaar) X(caadr) X(cadar) X(caddr) X(cdaar) X(cdadr) X(cddar) X(cdddr) \
    X(caaaar) X(caaadr) X(caadar) X(caaddr) X(cadaar) X(cadadr) X(caddar) X(cadddr) \
    X(cdaaar) X(cdaadr) X(cdadar) X(cdaddr) X(cddaar) X(cddadr) X(cdddar) X(cddddr)

#define DEF_CXR(nm) Object* prim_##nm(IEvaluator*, Object* a, Environment*) { return cxr(arg1(a), #nm); }
CXR_LIST(DEF_CXR)
#undef DEF_CXR

}  // namespace

void PrimitiveLists::install(zombie::runtime::Environment* env) {
    using namespace zombie::runtime;
    static const NativeEntry entries[] = {
        { "pair?",     1, 1,      prim_pairq    },
        { "list?",     1, 1,      prim_listq    },
        { "null?",     1, 1,      prim_nullq    },
        { "cons",      2, 2,      prim_cons     },
        { "car",       1, 1,      prim_car      },
        { "cdr",       1, 1,      prim_cdr      },
        { "first",     1, 1,      prim_car      },  // alias
        { "rest",      1, 1,      prim_cdr      },  // alias
        { "set-car!",  2, 2,      prim_set_car  },
        { "set-cdr!",  2, 2,      prim_set_cdr  },
        { "list",      0, 999999, prim_list     },
        { "length",    1, 1,      prim_length   },
        { "append",    0, 999999, prim_append   },
        { "reverse",   1, 1,      prim_reverse  },
        { "list-tail", 2, 2,      prim_list_tail},
        { "list-ref",  2, 2,      prim_list_ref },
        { "last-pair", 1, 1,      prim_last_pair},
        { "second",    1, 1,      prim_second   },
        { "third",     1, 1,      prim_third    },
        { "memq",      2, 2,      prim_memq     },
        { "memv",      2, 2,      prim_memv     },
        { "member",    2, 3,      prim_member   },
        { "assq",      2, 2,      prim_assq     },
        { "assv",      2, 2,      prim_assv     },
        { "assoc",     2, 3,      prim_assoc    },
        { "make-list", 1, 2,      prim_make_list},
        { "list-copy", 1, 1,      prim_list_copy},
        { "list-set!", 3, 3,      prim_list_set },
#define ROW_CXR(nm) { #nm, 1, 1, prim_##nm },
        CXR_LIST(ROW_CXR)
#undef ROW_CXR
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}
