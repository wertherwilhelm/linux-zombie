/**
 * @file mirror.cpp
 * @brief Implementation of cross-language mirrors
 */

#include "pch.h"
#include "mirror.h"
#include "scheme_engine.h"
#include "embedding_api.h"
#include "Object.h"
#include "Environment.h"
#include "Lisp.h"
#include "procedure.h"
#include "Base.h"
#include <sstream>

// ============================================================================
// Global Instances
// ============================================================================

extern Ledger* g_ledger;  // Defined in ledger.cpp
MirrorRegistry* g_mirrors = nullptr;

// Alias for primitives (they use g_value_ledger name)
namespace primitives {
    extern Ledger* g_value_ledger;
}

// Point to the same instance
Ledger* primitives::g_value_ledger = nullptr;

// ============================================================================
// SchemeMirror Implementation
// ============================================================================

SchemeMirror::SchemeMirror(Object* proc, Environment* env)
    : proc_(proc)
    , env_(env)
{
    if (!proc) {
        throw std::runtime_error("SchemeMirror: null Procedure");
    }
}

void* SchemeMirror::invoke(const std::vector<void*>& args) {
    if (!proc_) {
        throw std::runtime_error("SchemeMirror::invoke: null Procedure");
    }

    // Convert void* args to Object* (Scheme values)
    Object* arg_list = nil;  // Build argument list in reverse
    for (auto it = args.rbegin(); it != args.rend(); ++it) {
        Object* arg = static_cast<Object*>(*it);
        arg_list = cons(arg, arg_list);
    }

    // Call the Scheme Procedure using embedding engine
    try {
        scheme_engine* engine = embedding::get_default_engine();
        if (engine) {
            ::Procedure* scheme_proc = static_cast<::Procedure*>(proc_);
            // Use provided env_ if available, otherwise engine's global env
            Environment* eval_env = env_ ? env_ : engine->get_environment();
            Object* result = scheme_proc->apply(engine->get_evaluator(), arg_list, eval_env);
            return static_cast<void*>(result);
        } else {
            throw std::runtime_error("SchemeMirror::invoke: no evaluator available");
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("SchemeMirror::invoke failed: ") + e.what());
    }
}

// ============================================================================
// VTable Implementations
// ============================================================================

// Helper: Convert Object* to string
static std::string object_to_string(void* ptr) {
    if (!ptr) return "#<null>";
    Object* obj = static_cast<Object*>(ptr);
    std::ostringstream ss;
    ss << obj;
    return ss.str();
}

// Helper: Check if Object* is truthy
static bool object_is_truthy(void* ptr) {
    if (!ptr) return false;
    Object* obj = static_cast<Object*>(ptr);
    return truth(obj);
}

// Helper: Destroy Object (no-op for GC-managed objects)
static void object_destroy(void* ptr) {
    // Objects are GC-managed, so we don't manually delete them
    // This is just a placeholder for non-GC values
}

// Scheme mirror vtable
const ValueVTable scheme_mirror_vtable = {
    object_to_string,  // to_string
    object_is_truthy,  // is_truthy
    [](void* fn_ptr, const std::vector<void*>& args) -> void* {
        SchemeMirror* mirror = static_cast<SchemeMirror*>(fn_ptr);
        return mirror->invoke(args);
    },
    [](void* ptr) {
        // Cleanup SchemeMirror wrapper (not the Scheme Object itself!)
        SchemeMirror* mirror = static_cast<SchemeMirror*>(ptr);
        delete mirror;
    },
    "SchemeMirror"  // type_name
};

// Native mirror vtable
const ValueVTable native_mirror_vtable = {
    [](void* ptr) -> std::string {
        return "#<native-mirror>";
    },
    [](void* ptr) -> bool {
        return true;  // Functions are always truthy
    },
    [](void* fn_ptr, const std::vector<void*>& args) -> void* {
        // This would need template magic to work properly
        // For now, throw - actual implementation in make_native_mirror
        throw std::runtime_error("Native mirror invoke not yet implemented");
    },
    [](void* ptr) {
        // Cleanup native mirror wrapper
        // Note: This is tricky because we don't know the template type
        // Real implementation would need type erasure
    },
    "NativeMirror"  // type_name
};

// ============================================================================
// Factory Functions
// ============================================================================

Value make_scheme_mirror(Object* proc, Environment* env) {
    // Allocate mirror on heap
    SchemeMirror* mirror = new SchemeMirror(proc, env);

    Value v;
    v.ptr = static_cast<void*>(mirror);
    v.vt = &scheme_mirror_vtable;
    v.ledger = LedgerEntry(
        BackendTag::Scheme,
        "scheme-mirror",
        "callable mirror scheme"
    );

    return v;
}
