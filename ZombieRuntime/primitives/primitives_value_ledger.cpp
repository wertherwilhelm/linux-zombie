#include "pch.h"
/**
 * @file primitives_value_ledger.cpp
 * @brief Implementation of Value Ledger primitives
 */

#include "../pch.h"
#include "primitives_value_ledger.h"
#include "../primitive_base.h"
#include "../value.h"
#include "../ledger.h"
#include "../mirror.h"
#include "../scheme_skin.h"
#include "../cons_cell.h"
#include "../constants.h"
#include "../Environment.h"
#include "../atom.h"
#include "../numeric_runtime.h"
#include <string>

namespace primitives {

// Global ledger instance (initialized in constants.cpp or similar)
extern Ledger* g_value_ledger;

// ============================================================================
// Helper Functions
// ============================================================================

static Object* make_symbol(const std::string& name) {
    return intern_symbol(name);
}

static Object* make_string_obj(const std::string& str) {
    return gc_new<a_string>(str);
}

static Object* make_int_obj(long value) {
    return numeric::make_integer(value);
}

// Build association list: ((key1 . val1) (key2 . val2) ...)
static Object* make_alist(const std::vector<std::pair<Object*, Object*>>& pairs) {
    Object* result = nil;
    for (auto it = pairs.rbegin(); it != pairs.rend(); ++it) {
        Object* pair = cons(it->first, it->second);
        result = cons(pair, result);
    }
    return result;
}

// ============================================================================
// Primitive Implementations
// ============================================================================

Object* prim_value_info(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("value-info: requires 1 argument");
    }

    Object* value = car(args);

    // Look up existing ledger entry (read-only, doesn't assign ID)
    extern Ledger* g_value_ledger;
    const LedgerEntry* entry = nullptr;
    if (g_value_ledger) {
        entry = g_value_ledger->lookup_entry(value);
    }

    // Build association list
    std::vector<std::pair<Object*, Object*>> info;

    // Backend
    std::string backend_name;
    if (entry) {
        switch (entry->backend) {
            case BackendTag::Scheme:   backend_name = "scheme"; break;
            case BackendTag::Native:   backend_name = "native"; break;
            case BackendTag::Bytecode: backend_name = "bytecode"; break;
            case BackendTag::Dream:    backend_name = "dream"; break;
        }
    } else {
        backend_name = "scheme";  // Default for untracked objects
    }
    info.push_back({make_symbol("backend"), make_symbol(backend_name)});

    // Origin
    std::string origin = entry ? entry->origin : "untracked";
    info.push_back({make_symbol("origin"), make_string_obj(origin)});

    // Traits
    std::string traits = entry ? entry->traits : "";
    info.push_back({make_symbol("traits"), make_string_obj(traits)});

    // Callable? - Check if it's a Procedure type in Scheme
    bool callable = false;
    if (value) {
        callable = (value->instance_of() == static_cast<int>(Base::atype::PROCEDURE) ||
                   value->instance_of() == static_cast<int>(Base::atype::CLOSURE) ||
                   value->instance_of() == static_cast<int>(Base::atype::PRIMITIVE));
    }
    info.push_back({make_symbol("callable"), callable ? t : static_cast<Object*>(false_obj)});

    return make_alist(info);
}

Object* prim_value_id(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("value-id: requires 1 argument");
    }

    Object* value = car(args);

    // Ensure the value has a ValueId (assigns one if needed)
    extern Ledger* g_value_ledger;
    if (g_value_ledger && value) {
        // ensure_entry assigns ValueId if not already assigned
        g_value_ledger->ensure_entry(value, "value-id:query", "");
        return make_int_obj(static_cast<long>(value->value_id()));
    }

    // Fallback if no ledger (return 0 = not tracked)
    return make_int_obj(0);
}

Object* prim_use_skin(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("use-skin: requires 2 arguments (value skin-name)");
    }

    Object* value = car(args);
    Object* rest = cdr(args);

    if (!rest || is_nil(rest)) {
        throw std::runtime_error("use-skin: requires skin-name argument");
    }

    Object* skin_name_obj = car(rest);

    // Get skin name as string
    std::string skin_name;
    if (skin_name_obj->get_type() == static_cast<int>(Type::SYMBOL)) {
        skin_name = static_cast<a_symbol*>(skin_name_obj)->getName();
    } else if (skin_name_obj->get_type() == static_cast<int>(Type::STRING)) {
        skin_name = static_cast<a_string*>(skin_name_obj)->value();
    } else {
        throw std::runtime_error("use-skin: skin-name must be a symbol or string");
    }

    // Wrap value
    Value v = wrap_scheme_object(value, "use-skin:transform");

    // Apply skin
    if (!g_value_ledger) {
        throw std::runtime_error("use-skin: global ledger not initialized");
    }

    g_value_ledger->apply_skin(v, skin_name);

    // Return the value (modified in-place conceptually)
    return value;
}

Object* prim_value_backend(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("value-backend: requires 1 argument");
    }

    Object* value = car(args);

    // Look up existing ledger entry (read-only, doesn't assign ID)
    extern Ledger* g_value_ledger;
    const LedgerEntry* entry = nullptr;
    if (g_value_ledger) {
        entry = g_value_ledger->lookup_entry(value);
    }

    if (entry) {
        switch (entry->backend) {
            case BackendTag::Scheme:   return make_symbol("scheme");
            case BackendTag::Native:   return make_symbol("native");
            case BackendTag::Bytecode: return make_symbol("bytecode");
            case BackendTag::Dream:    return make_symbol("dream");
            default:                   return make_symbol("unknown");
        }
    } else {
        return make_symbol("scheme");  // Default for untracked objects
    }
}

Object* prim_value_origin(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("value-origin: requires 1 argument");
    }

    Object* value = car(args);

    // Look up existing ledger entry (read-only, doesn't assign ID)
    extern Ledger* g_value_ledger;
    const LedgerEntry* entry = nullptr;
    if (g_value_ledger) {
        entry = g_value_ledger->lookup_entry(value);
    }

    std::string origin = entry ? entry->origin : "untracked";
    return make_string_obj(origin);
}

Object* prim_value_traits(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("value-traits: requires 1 argument");
    }

    Object* value = car(args);

    // Look up existing ledger entry (read-only, doesn't assign ID)
    extern Ledger* g_value_ledger;
    const LedgerEntry* entry = nullptr;
    if (g_value_ledger) {
        entry = g_value_ledger->lookup_entry(value);
    }

    std::string traits = entry ? entry->traits : "";
    return make_string_obj(traits);
}

Object* prim_capture_snapshot(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("capture-snapshot: requires 1 argument (description)");
    }

    Object* desc_obj = car(args);

    std::string description;
    if (desc_obj->get_type() == static_cast<int>(Type::STRING)) {
        description = static_cast<a_string*>(desc_obj)->value();
    } else if (desc_obj->get_type() == static_cast<int>(Type::SYMBOL)) {
        description = static_cast<a_symbol*>(desc_obj)->getName();
    } else {
        throw std::runtime_error("capture-snapshot: description must be a string or symbol");
    }

    if (!g_value_ledger) {
        throw std::runtime_error("capture-snapshot: global ledger not initialized");
    }

    size_t snapshot_id = g_value_ledger->capture_snapshot(description);

    return make_int_obj(static_cast<long>(snapshot_id));
}

Object* prim_restore_snapshot(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("restore-snapshot: requires 1 argument (snapshot-id)");
    }

    Object* id_obj = car(args);

    if (!numeric::is_number(id_obj)) {
        throw std::runtime_error("restore-snapshot: snapshot-id must be an integer");
    }

    long snapshot_id = static_cast<long>(numeric::to_int64(id_obj));

    if (!g_value_ledger) {
        throw std::runtime_error("restore-snapshot: global ledger not initialized");
    }

    g_value_ledger->restore_snapshot(static_cast<size_t>(snapshot_id));

    std::cout << "DEBUG restore-snapshot: returning t = " << (void*)t << std::endl;
    return t;
}

Object* prim_is_mirror(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("mirror?: requires 1 argument");
    }

    Object* value = car(args);
    Value v = wrap_scheme_object(value, "mirror?:query");

    // Check if value has "mirror" trait and is callable
    bool is_mirror = v.is_callable() && (v.traits().find("mirror") != std::string::npos);

    return is_mirror ? t : static_cast<Object*>(false_obj);
}

Object* prim_list_skins(Object* args, Environment* env) {
    if (!g_value_ledger) {
        throw std::runtime_error("list-skins: global ledger not initialized");
    }

    std::vector<std::string> skin_names = g_value_ledger->list_skins();

    Object* result = nil;
    for (auto it = skin_names.rbegin(); it != skin_names.rend(); ++it) {
        result = cons(make_symbol(*it), result);
    }

    return result;
}

Object* prim_backend_stats(Object* args, Environment* env) {
    if (!g_value_ledger) {
        throw std::runtime_error("backend-stats: global ledger not initialized");
    }

    auto counts = g_value_ledger->count_by_backend();

    std::vector<std::pair<Object*, Object*>> stats;

    // Build alist: ((scheme . count) (native . count) ...)
    stats.push_back({make_symbol("scheme"), make_int_obj(static_cast<long>(counts[BackendTag::Scheme]))});
    stats.push_back({make_symbol("native"), make_int_obj(static_cast<long>(counts[BackendTag::Native]))});
    stats.push_back({make_symbol("bytecode"), make_int_obj(static_cast<long>(counts[BackendTag::Bytecode]))});
    stats.push_back({make_symbol("dream"), make_int_obj(static_cast<long>(counts[BackendTag::Dream]))});

    return make_alist(stats);
}

Object* prim_find_by_trait(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("find-by-trait: requires 1 argument (trait-string)");
    }

    Object* trait_obj = car(args);

    std::string trait;
    if (trait_obj->get_type() == static_cast<int>(Type::STRING)) {
        trait = static_cast<a_string*>(trait_obj)->value();
    } else if (trait_obj->get_type() == static_cast<int>(Type::SYMBOL)) {
        trait = static_cast<a_symbol*>(trait_obj)->getName();
    } else {
        throw std::runtime_error("find-by-trait: trait must be a string or symbol");
    }

    if (!g_value_ledger) {
        throw std::runtime_error("find-by-trait: global ledger not initialized");
    }

    auto value_ids = g_value_ledger->find_by_trait(trait);

    // Convert ValueIds to Scheme integers (GC-safe!)
    Object* result = nil;
    gc_root<Object> result_guard(result);
    for (auto it = value_ids.rbegin(); it != value_ids.rend(); ++it) {
        Object* id_obj = make_int_obj(static_cast<long>(*it));
        result = cons(id_obj, result);
    }

    return result;
}

// ============================================================================
// Registration
// ============================================================================

void register_value_ledger_primitives(Environment* env) {
    // Note: Value ledger primitives are already installed by install_primitives()
    // via PrimitiveValueLedger::install(). This registry function is kept for
    // compatibility with embedders who explicitly install value-ledger group.
    // No-op: primitives already installed via OOP classes in install_primitives()
}

} // namespace primitives

// ============================================================================
// Auto-Registration with primitive_registry (OPTIONAL GROUP)
// ============================================================================

namespace {
    struct ValueLedgerGroupRegistrar {
        ValueLedgerGroupRegistrar() {
            primitive_registry::instance().register_group("value-ledger", primitives::register_value_ledger_primitives);
        }
    };
    static ValueLedgerGroupRegistrar value_ledger_registrar;
}
