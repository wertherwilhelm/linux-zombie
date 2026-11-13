#include "pch.h"
#include "constants.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include "gc_context.h"
#include "ledger.h"
#include "mirror.h"
#include "scheme_skin.h"
#include "last_words.h"
#include "atom.h"
#include "InputPort.h"
#include "OutputPort.h"
#include <map>
#include <mutex>

// Forward declaration for primitives namespace
namespace primitives {
    extern Ledger* g_value_ledger;
}

namespace zombie { namespace runtime {

// ============================================================================
// Symbol Interning Table
// ============================================================================

// Global symbol table - maps symbol names to unique symbol objects
// Thread-safe access via symbol_table_mutex
// IMPORTANT: Use std::map (not unordered_map) because we register GC roots
// at element addresses. std::map guarantees reference stability after insertion,
// while unordered_map rehashes and invalidates addresses.
static std::map<std::string, a_symbol*> symbol_table;
static std::mutex symbol_table_mutex;

// R5RS Scheme singletons - initialized in init_lisp_constants()
empty_list_class* empty_list = nullptr;
boolean_false_class* false_obj = nullptr;
__nil_and_t_lisp_class* t = nullptr;

// Backward compatibility alias - points to empty_list
// DEPRECATED: Migrate code to use empty_list or false_obj explicitly
empty_list_class* nil = nullptr;

// Track whether constants have been initialized (for idempotency)
static bool constants_initialized = false;

void init_lisp_constants()
{
    // Idempotent: only initialize once
    // This is critical for supporting multiple scheme_engine instances
    // which all share the same nil/#t/#f singletons
    if (constants_initialized) {
        return;
    }

    // Initialize GC if not already done
    if (!gc_context::current()) {
        initialize_gc();
    }

    // Allocate the three R5RS singleton values through GC
    empty_list = gc_new<empty_list_class>();
    false_obj = gc_new<boolean_false_class>();
    t = gc_new<__nil_and_t_lisp_class>();

    // Backward compatibility: nil -> empty_list
    // This maintains list operations but BREAKS Common Lisp-style nil=false
    nil = empty_list;

    // Register as permanent GC roots (never collect these)
    // Use slot-based registration for compatibility with moving collectors
    IGarbageCollector* gc = gc_context::current();
    gc->register_persistent_root(reinterpret_cast<gc_object**>(&empty_list));
    gc->register_persistent_root(reinterpret_cast<gc_object**>(&false_obj));
    gc->register_persistent_root(reinterpret_cast<gc_object**>(&t));

    // Initialize standard I/O ports through GC
    init_stdin_port();
    init_stdout_port();

    // Initialize Value Ledger and Multiverse system
    if (!g_ledger) {
        g_ledger = new Ledger();

        // Set the alias pointer for primitives namespace (defined in mirror.cpp)
        primitives::g_value_ledger = g_ledger;

        // Register default skins
        g_ledger->register_skin(get_scheme_skin());

        // TODO: Register native, bytecode, and dream skins when available
    }

    // Initialize Mirror registry
    if (!g_mirrors) {
        init_global_mirrors();
    }

    // Let the dead speak: record epitaphs for collected objects
    zombie::runtime::necromancy::install_obituary_hook();

    // Mark as initialized
    constants_initialized = true;
}

void init()
{
    init_lisp_constants();
}

Object* lnot(Object* o)
{
    // R5RS semantics: (not obj) returns #t if obj is #f, #f otherwise
    // In particular: (not '()) => #f  (empty list is truthy!)
    if (o == false_obj) {
        return t;
    } else {
        return false_obj;
    }
}

// ============================================================================
// Symbol Interning Implementation
// ============================================================================

a_symbol* intern_symbol(const std::string& name) {
    // Thread-safe lookup/insertion
    std::lock_guard<std::mutex> lock(symbol_table_mutex);

    // Check if symbol already exists
    auto it = symbol_table.find(name);
    if (it != symbol_table.end()) {
        // Found existing symbol - return it
        return it->second;
    }

    // Symbol doesn't exist - create new one
    a_symbol* new_symbol = gc_new<a_symbol>(name);

    // Insert into symbol table FIRST (creates stable slot in std::map)
    // std::map guarantees reference stability after insertion
    auto [inserted_it, success] = symbol_table.insert({name, new_symbol});

    // Now register the stable slot address as a persistent GC root
    // (symbols are never collected - they live for the lifetime of the program)
    IGarbageCollector* gc = gc_context::current();
    if (gc) {
        gc->register_persistent_root(reinterpret_cast<gc_object**>(&inserted_it->second));
    }

    return new_symbol;
}

} } // namespace zombie::runtime
