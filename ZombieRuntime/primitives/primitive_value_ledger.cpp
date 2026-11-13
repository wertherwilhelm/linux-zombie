#include "pch.h"
/**
 * @file primitive_value_ledger.cpp
 * @brief OOP implementation of Value Ledger primitives
 */

#include "../pch.h"
#include "primitive_value_ledger.h"
#include "primitives_value_ledger.h"  // For existing implementations
#include "../Base.h"
#include "../atom.h"
#include "../cons_cell.h"
#include "../../GarbageCollector/gc_ptr.h"

// ============================================================================
// Construction
// ============================================================================

PrimitiveValueLedger::PrimitiveValueLedger(ID id, int min_args, int max_args)
    : PrimitiveBase(min_args, max_args)
    , m_id(id)
{
    // Set Primitive name based on ID
    switch (id) {
        case VALUE_INFO:       m_name = "value-info"; break;
        case VALUE_ID:         m_name = "value-id"; break;
        case VALUE_BACKEND:    m_name = "value-backend"; break;
        case VALUE_ORIGIN:     m_name = "value-origin"; break;
        case VALUE_TRAITS:     m_name = "value-traits"; break;
        case USE_SKIN:         m_name = "use-skin"; break;
        case CAPTURE_SNAPSHOT: m_name = "capture-snapshot"; break;
        case RESTORE_SNAPSHOT: m_name = "restore-snapshot"; break;
        case IS_MIRROR:        m_name = "mirror?"; break;
        case LIST_SKINS:       m_name = "list-skins"; break;
        case BACKEND_STATS:    m_name = "backend-stats"; break;
        case FIND_BY_TRAIT:    m_name = "find-by-trait"; break;
    }
}

// ============================================================================
// Apply
// ============================================================================

Object* PrimitiveValueLedger::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Check arity first
    if (!check_arity(args)) {
        return arity_error("");
    }

    // Delegate to existing implementations in primitives namespace
    switch (m_id) {
        case VALUE_INFO:
            return primitives::prim_value_info(args, env);

        case VALUE_ID:
            return primitives::prim_value_id(args, env);

        case VALUE_BACKEND:
            return primitives::prim_value_backend(args, env);

        case VALUE_ORIGIN:
            return primitives::prim_value_origin(args, env);

        case VALUE_TRAITS:
            return primitives::prim_value_traits(args, env);

        case USE_SKIN:
            return primitives::prim_use_skin(args, env);

        case CAPTURE_SNAPSHOT:
            return primitives::prim_capture_snapshot(args, env);

        case RESTORE_SNAPSHOT:
            return primitives::prim_restore_snapshot(args, env);

        case IS_MIRROR:
            return primitives::prim_is_mirror(args, env);

        case LIST_SKINS:
            return primitives::prim_list_skins(args, env);

        case BACKEND_STATS:
            return primitives::prim_backend_stats(args, env);

        case FIND_BY_TRAIT:
            return primitives::prim_find_by_trait(args, env);

        default:
            return error("Unknown Value Ledger Primitive");
    }
}

// ============================================================================
// Registration
// ============================================================================

void PrimitiveValueLedger::install(Environment* env) {
    // GC Safety: Use macro to protect name string before allocating primitive
    #define DEF(name, ...) do { \
        gc_root<a_string> _n(gc_new<a_string>(name)); \
        env->define(_n.get(), gc_new<PrimitiveValueLedger>(__VA_ARGS__)); \
    } while(0)

    // Query primitives
    DEF("value-info",    VALUE_INFO, 1, 1);
    DEF("value-id",      VALUE_ID, 1, 1);
    DEF("value-backend", VALUE_BACKEND, 1, 1);
    DEF("value-origin",  VALUE_ORIGIN, 1, 1);
    DEF("value-traits",  VALUE_TRAITS, 1, 1);

    // Transformation primitives
    DEF("use-skin", USE_SKIN, 2, 2);

    // Time-travel primitives
    DEF("capture-snapshot", CAPTURE_SNAPSHOT, 1, 1);
    DEF("restore-snapshot", RESTORE_SNAPSHOT, 1, 1);

    // Utility primitives
    DEF("mirror?",       IS_MIRROR, 1, 1);
    DEF("list-skins",    LIST_SKINS, 0, 0);
    DEF("backend-stats", BACKEND_STATS, 0, 0);
    DEF("find-by-trait", FIND_BY_TRAIT, 1, 1);

    #undef DEF
}
