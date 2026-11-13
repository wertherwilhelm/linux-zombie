#include "pch.h"
#include "Base.h"
#include "constants.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include "numeric_runtime.h"
#include "a_exception.h"
#include <stdexcept>

namespace zombie { namespace runtime {

Base::Base() {

}

Base::~Base() {

}

Base::Base(const Base& p) {
    *this = p;
}

Object * Base::clone() const {
    Base * p = gc_new<Base>(*this);
    return p;
}

void Base::print(std::ostream &os) const {
    os << "(**BASE**)";
}

int Base::is_atom() const {
    return 0;
}

int Base::pair() const {
    return 0;
}

// Global Functions

Object* truth(bool x) {
    // Convert C++ boolean to Scheme boolean (#t or #f)
    return x ? (Object*)t : (Object*)false_obj;
}

bool truth(Object * x) {
    // R5RS Section 6.1: Only #f is false. Everything else is true.
    // This includes: #t, 0, empty list '(), strings, symbols, numbers, etc.
    //
    // CRITICAL: The empty list '() is TRUE in R5RS Scheme!
    // This is different from Common Lisp where nil = '() = false.
    if (!x || x == false_obj) return false;
    return true;
}

bool equal(Object *x, Object *y) {
    // Recursive structural equality (R5RS section 6.1)

    // Quick pointer check
    if (x == y) return true;

    // nil checks
    if (x == nil && y == nil) return true;
    if (x == nil || y == nil) return false;

    // Type mismatch
    if (x->get_type() != y->get_type()) return false;

    // Pairs - recursive comparison
    if (x->pair() && y->pair()) {
        return equal(first(x), first(y)) && equal(rest(x), rest(y));
    }

    // Strings - compare by content
    if (is_string(x) && is_string(y)) {
        return ((a_string*)x)->value() == ((a_string*)y)->value();
    }

    // Symbols - compare by name
    if (is_symbol(x) && is_symbol(y)) {
        return ((a_symbol*)x)->value() == ((a_symbol*)y)->value();
    }

    // Numbers - compare by value
    if (x->get_type() == static_cast<int>(Type::DOUBLE) && y->get_type() == static_cast<int>(Type::DOUBLE)) {
        return ((a_double*)x)->value() == ((a_double*)y)->value();
    }
    if (x->get_type() == static_cast<int>(Type::INT) && y->get_type() == static_cast<int>(Type::INT)) {
        return ((atom<int>*)x)->value() == ((atom<int>*)y)->value();
    }
    if (x->get_type() == static_cast<int>(Type::LONG) && y->get_type() == static_cast<int>(Type::LONG)) {
        return ((atom<long long>*)x)->value() == ((atom<long long>*)y)->value();
    }

    // Characters - compare by value
    if (is_char(x) && is_char(y)) {
        return ((a_char*)x)->value() == ((a_char*)y)->value();
    }

    // Everything else - pointer equality
    return x == y;
}

bool eqv(Object *x, Object *y) {
    // eqv? is like eq? but compares numbers and characters by value (R5RS section 6.1)

    // Quick pointer check
    if (x == y) return true;

    // nil checks
    if (x == nil && y == nil) return true;
    if (x == nil || y == nil) return false;

    // Type mismatch
    if (x->get_type() != y->get_type()) return false;

    // Symbols - compare by name (since we don't have symbol interning)
    if (is_symbol(x) && is_symbol(y)) {
        return ((a_symbol*)x)->value() == ((a_symbol*)y)->value();
    }

    // Numbers - compare by value
    if (x->get_type() == static_cast<int>(Type::DOUBLE) && y->get_type() == static_cast<int>(Type::DOUBLE)) {
        return ((a_double*)x)->value() == ((a_double*)y)->value();
    }
    if (x->get_type() == static_cast<int>(Type::INT) && y->get_type() == static_cast<int>(Type::INT)) {
        return ((atom<int>*)x)->value() == ((atom<int>*)y)->value();
    }
    if (x->get_type() == static_cast<int>(Type::LONG) && y->get_type() == static_cast<int>(Type::LONG)) {
        return ((atom<long long>*)x)->value() == ((atom<long long>*)y)->value();
    }

    // Characters - compare by value
    if (is_char(x) && is_char(y)) {
        return ((a_char*)x)->value() == ((a_char*)y)->value();
    }

    // Everything else (pairs, strings, procedures) - pointer equality
    return x == y;
}

Object * car(const Object * e) {
    if (!e->is_atom()) {
        return ((ConsCell*)e)->car();
    }
    std::cerr << "car: cannot take car of an atom!" << std::endl;
    return nil;
}

Object * cdr(const Object * e) {
    if (!e->is_atom()) {
        return ((ConsCell*)e)->cdr();
    }
    std::cerr << "cdr: cannot take cdr of an atom!" << std::endl;
    return nil;
}

Object * null(Object * e) {
    if (e == nil) {
        return t;
    }
    return nil;
}

int is_lisp_list(Object * l) {
    if (l == nil) {
        return 1;
    }
    if (l->is_atom()) {
        return 0;
    }
    return is_lisp_list(((ConsCell*)l)->cdr());
}

int is_nonempty_lisp_List(Object * l) {
    if (l->is_atom()) {
        return 0;
    }
    return is_lisp_list(((ConsCell*)l)->cdr());
}

Object * cond(Object* e) {
    if (is_nonempty_lisp_List(e)) {
        Object * firstcase = ((ConsCell*)e)->car();
        if (is_nonempty_lisp_List(firstcase)) {
            Object * condition=((ConsCell*)firstcase)->car();
            if(condition->is_atom()) {
                if(condition == nil)
                    return cond(((ConsCell*)e)->cdr());
                else if (condition == t)
                    return ((ConsCell*)firstcase)->cdr();
                else
                    std::cerr << "cond expects a case first element to be nil of t." << std::endl;
            } else {
                std::cerr << "cond expects a case first element to be nil of t." << std::endl;
            }
        } else {
            std::cerr << "cond expects a List for each case." << std::endl;
        }       
    } else {
        std::cerr << "cond expects a List for evaluation" << std::endl;
    }
    return nil;
}

int length(Object *x) {
    int len = 0;
    if (x == NULL || x == nil) {
        return 0;
    }
    Object *l = x;
    if (l->is_atom()) {
        return 0;
    }

    while (l!=nil && !l->is_atom()) {
        len++;
        l = ((ConsCell*)l)->cdr();

        // Safety limit to prevent infinite loops on circular lists
        if (len > 100000) {
            std::cerr << "ERROR: List too long (possible circular reference)!" << std::endl;
            return -1;
        }
    }
    return len;
}

// Reverse a list
Object* reverse(Object* lst) {
    if (lst == nil || lst->is_atom()) {
        return lst;
    }

    Object* result = nil;
    Object* current = lst;

    while (current != nil && !current->is_atom()) {
        result = cons(car(current), result);
        current = cdr(current);
    }

    return result;
}

// Some Ansi Lisp like manipulation routines;

// first and rest should be inline cdr and car
Object* first(const Object *x) {
    if (x != NULL) {
        if (x->pair()) {
            return ((ConsCell*)x)->car();
        }
    }
    std::cerr << "first: cannot take first of an atom!" << std::endl;
    return nil;
}

Object* rest(const Object* e) {
    if (e != NULL) {
        if (!e->is_atom()) {
            return ((ConsCell*)e)->cdr();
        }
    }
    std::cerr << "rest: cannot take rest of an atom!" << std::endl;
    return nil;
}

Object* second(const Object *x) {
    if (x == NULL || x->is_nil()) {
        return nil;
    }
    Object* r = rest(x);
    if (r == NULL || r->is_nil()) {
        return nil;
    }
    return first(r);
}

Object* third(const Object *x) {
    if (x == NULL || x->is_nil()) {
        return nil;
    }
    Object* r = rest(x);
    if (r == NULL || r->is_nil()) {
        return nil;
    }
    Object* rr = rest(r);
    if (rr == NULL || rr->is_nil()) {
        return nil;
    }
    return first(rr);
}

ConsCell* list(Object *a, Object *b) {
    // Protect inner allocation during outer allocation
    gc_root<ConsCell> inner(gc_new<ConsCell>(b, nil));
    return gc_new<ConsCell>(a, inner.get());
}

a_string* error(std::string message) {
    throw scheme_error(message);
}

a_string* warn(std::string message) {
    a_string* lo = gc_new<a_string>(message);
    //AfxMessageBox("**** WARNING: " + CString(message));
    return lo;
}

Object* set_first(Object *x, Object *y) {
    if (!x->is_atom())  {
        ((ConsCell*)x)->car(y);
        return x;
    } else {
        return error("Attempt to set-car of a non-Pair:");
    }
}

Object* set_rest(Object *x, Object *y) {
    if (!x->is_atom()) {
        ((ConsCell*)x)->cdr(y);
        return x;
    }
    else {
        return error("Attempt to set-cdr of a non-Pair:");
    }
}

double num(Object* x) {
    // Use numeric runtime to convert any numeric type to double
    if (numeric::is_number(x)) {
        return numeric::to_double(x);
    }
    return 0.0;
}

bool pair(Object* x) {
    return x->pair();
}

bool is_list(Object* x) {
    // A proper list is either:
    // 1. nil (empty list), OR
    // 2. A pair whose cdr is also a proper list
    // Use iterative approach to avoid calling rest() on atoms
    while (x != NULL && x != nil && !x->is_nil()) {
        if (!x->pair()) {
            return false;  // Not a pair and not nil = improper list
        }
        // It's a pair, move to the cdr
        x = ((ConsCell*)x)->cdr();
    }
    // Reached nil/NULL, so it's a proper list
    return true;
}

void Base::trace_references(Tracer& tracer)
{
    // Base class has no Object references
}

} } // namespace zombie::runtime
