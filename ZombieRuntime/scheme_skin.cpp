/**
 * @file scheme_skin.cpp
 * @brief Implementation of Scheme skin for Value system
 */

#include "pch.h"
#include "scheme_skin.h"
#include "scheme_engine.h"
#include "embedding_api.h"
#include "ledger.h"
#include "Object.h"
#include "atom.h"
#include "cons_cell.h"
#include "Base.h"
#include "Lisp.h"
#include "constants.h"
#include "Environment.h"
#include "procedure.h"
#include <sstream>
#include <stdexcept>

// ============================================================================
// Helper Functions
// ============================================================================

static std::string scheme_object_to_string(void* ptr) {
    if (!ptr) return "#<null>";

    Object* obj = static_cast<Object*>(ptr);
    std::ostringstream ss;
    ss << obj;
    return ss.str();
}

static bool scheme_object_is_truthy(void* ptr) {
    if (!ptr) return false;

    Object* obj = static_cast<Object*>(ptr);
    return truth(obj);  // Scheme truth: only #f is false
}

static void* scheme_object_invoke(void* fn_ptr, const std::vector<void*>& args) {
    if (!fn_ptr) {
        throw std::runtime_error("Cannot invoke null Scheme Object");
    }

    Object* obj = static_cast<Object*>(fn_ptr);

    // Check if it's actually callable
    Base* b = dynamic_cast<Base*>(obj);
    if (!obj->pair() && (!b || (b->instance_of() != static_cast<int>(Base::atype::PROCEDURE) &&
                                  b->instance_of() != static_cast<int>(Base::atype::CLOSURE) &&
                                  b->instance_of() != static_cast<int>(Base::atype::PRIMITIVE)))) {
        throw std::runtime_error("Scheme Object is not callable");
    }

    // Build argument list
    Object* arg_list = nil;
    for (auto it = args.rbegin(); it != args.rend(); ++it) {
        Object* arg = static_cast<Object*>(*it);
        arg_list = cons(arg, arg_list);
    }

    // Apply Procedure using embedding engine
    scheme_engine* engine = embedding::get_default_engine();
    if (engine) {
        Procedure* proc = static_cast<Procedure*>(obj);
        Object* result = proc->apply(engine->get_evaluator(), arg_list, engine->get_environment());
        return static_cast<void*>(result);
    } else {
        throw std::runtime_error("No Scheme evaluator available");
    }
}

static void scheme_object_destroy(void* ptr) {
    // Scheme objects are GC-managed, so we don't manually delete
    // This is a no-op, but we keep it for API consistency
}

// ============================================================================
// Scheme VTable
// ============================================================================

const ValueVTable scheme_vtable = {
    scheme_object_to_string,   // to_string
    scheme_object_is_truthy,   // is_truthy
    scheme_object_invoke,      // invoke
    scheme_object_destroy,     // destroy
    "SchemeObject"             // type_name
};

// ============================================================================
// Scheme Skin
// ============================================================================

static Skin g_scheme_skin_instance(
    "scheme",
    scheme_vtable,
    "safe debuggable interpreted"
);

const Skin& get_scheme_skin() {
    return g_scheme_skin_instance;
}

// ============================================================================
// Wrapper Functions
// ============================================================================

Value wrap_scheme_object(Object* obj, const std::string& origin) {
    Value v;
    v.ptr = static_cast<void*>(obj);
    v.vt = &scheme_vtable;

    // Use global ledger if available to track provenance
    // This ensures origin is set only once (on first wrap)
    extern Ledger* g_ledger;
    if (g_ledger && obj) {
        // ensure_entry preserves existing origin - only sets it on first touch!
        LedgerEntry& entry = g_ledger->ensure_entry(obj, origin, "safe debuggable");
        v.ledger = entry;  // Copy canonical entry from ledger
    } else {
        // Fallback if no ledger (shouldn't happen in normal use)
        v.ledger = LedgerEntry(BackendTag::Scheme, origin, "safe debuggable");
    }

    return v;
}

Object* unwrap_scheme_object(const Value& v) {
    if (v.is_null()) {
        throw std::runtime_error("Cannot unwrap null Value");
    }

    if (!is_scheme_value(v)) {
        throw std::runtime_error("Value does not contain a Scheme Object");
    }

    return static_cast<Object*>(v.ptr);
}

bool is_scheme_value(const Value& v) {
    return v.vt == &scheme_vtable || v.backend() == BackendTag::Scheme;
}

// ============================================================================
// Type-Specific Wrappers
// ============================================================================

Value wrap_int(long value, const std::string& origin) {
    Object* obj = gc_new<atom<long long>>(value);
    return wrap_scheme_object(obj, origin);
}

Value wrap_double(double value, const std::string& origin) {
    Object* obj = gc_new<atom<double>>(value);
    return wrap_scheme_object(obj, origin);
}

Value wrap_string(const std::string& str, const std::string& origin) {
    Object* obj = gc_new<a_string>(str);
    return wrap_scheme_object(obj, origin);
}

Value wrap_bool(bool value, const std::string& origin) {
    Object* obj = value ? static_cast<Object*>(t) : static_cast<Object*>(false_obj);  // Scheme's #t and #f
    return wrap_scheme_object(obj, origin);
}

Value wrap_nil(const std::string& origin) {
    return wrap_scheme_object(nil, origin);
}

// ============================================================================
// Conversion Helpers
// ============================================================================

Object* values_to_list(const std::vector<Value>& values) {
    Object* result = nil;

    // Protect accumulator from GC during list building
    gc_root<Object> result_guard(result);

    // Build list in reverse order
    for (auto it = values.rbegin(); it != values.rend(); ++it) {
        Object* elem = unwrap_scheme_object(*it);
        result = cons(elem, result);
    }

    return result;
}

std::vector<Value> list_to_values(Object* list, const std::string& origin) {
    std::vector<Value> result;

    Object* current = list;
    while (current && !is_nil(current)) {
        if (!current->pair()) {
            throw std::runtime_error("list_to_values: not a proper list");
        }

        Object* elem = car(current);
        result.push_back(wrap_scheme_object(elem, origin));

        current = cdr(current);
    }

    return result;
}
