#pragma once

/**
 * @file value.h
 * @brief Universal Value representation with provenance and swappable behavior
 *
 * This is the foundation of the Multiverse Workbench - a system where values
 * can exist in multiple realities (Scheme, Native, Bytecode, Dream) simultaneously,
 * each with full provenance tracking and the ability to swap between behaviors.
 *
 * Philosophy:
 * - Every value knows where it came from (origin, backend)
 * - Behavior is swappable via "skins" (vtable sets)
 * - Cross-language calls are seamless via "mirrors"
 * - Time-travel debugging via Continuation snapshots
 *
 * @see docs/future_work/Value_Ledger_Sketch.md
 * @see docs/future_work/Multiverse_Workbench_Vision.md
 */

#include <string>
#include <vector>
#include <cstddef>
#include <stdexcept>

// Forward declarations
namespace zombie { namespace runtime {
class Object;  // Existing Scheme Object type
}}
using zombie::runtime::Object;
class gc_object;  // GC Base class

// ============================================================================
// Backend Tags - Which Reality Does This Value Live In?
// ============================================================================

/**
 * @brief Identifies which backend/reality this value originated from
 *
 * - Scheme: Traditional interpreted Scheme (slow, safe, debuggable)
 * - Native: JIT-compiled or C++ native code (fast, optimized)
 * - Bytecode: Bytecode VM execution (balanced speed/safety)
 * - Dream: Genetic/speculative variants (experimental, evolving)
 */
enum class BackendTag {
    Scheme,     // Pure Scheme interpreter
    Native,     // Native C++ / JIT compiled
    Bytecode,   // Bytecode VM
    Dream       // Genetic variants / speculative execution
};

// Convert backend tag to string for debugging
inline const char* backend_tag_name(BackendTag tag) {
    switch (tag) {
        case BackendTag::Scheme:   return "Scheme";
        case BackendTag::Native:   return "Native";
        case BackendTag::Bytecode: return "Bytecode";
        case BackendTag::Dream:    return "Dream";
        default:                   return "Unknown";
    }
}

// ============================================================================
// Virtual Table - Swappable Behavior (Skins)
// ============================================================================

/**
 * @brief Function table for polymorphic operations on values
 *
 * This is a "skin" - a swappable set of behaviors. The same value can
 * have different skins applied at runtime, changing how it behaves:
 *
 * - Scheme skin: Uses Scheme semantics (slow, debuggable)
 * - Native skin: Direct C++ calls (fast, optimized)
 * - Mirror skin: Cross-language adapter
 *
 * Think of it as Ruby's method_missing or Smalltalk's doesNotUnderstand,
 * but at compile-time with zero overhead!
 */
struct ValueVTable {
    // String representation for display/debugging
    std::string (*to_string)(void* ptr);

    // Truthiness check (for conditionals)
    bool (*is_truthy)(void* ptr);

    // Invoke if this value is callable (nullptr if not callable)
    // Returns result as void* (caller must cast)
    void* (*invoke)(void* fn_ptr, const std::vector<void*>& args);

    // Cleanup/destruction
    void (*destroy)(void* ptr);

    // Type identity (for dynamic checks)
    const char* type_name;
};

// ============================================================================
// Ledger Entry - Provenance Tracking
// ============================================================================

/**
 * @brief Tracks the origin and properties of a value
 *
 * This is like a "birth certificate" for values. It answers:
 * - Where did this value come from? (origin)
 * - Which reality is it in? (backend)
 * - What properties does it have? (traits)
 * - Is it part of a Continuation snapshot? (snapshot_id)
 *
 * Used for:
 * - Debugging: "Why is this value wrong?" → Check its origin!
 * - Optimization: "Where are hot paths?" → Count Native backend values
 * - Time-travel: "What state was this in?" → Restore from snapshot
 */
struct LedgerEntry {
    BackendTag backend;           // Which reality: Scheme/Native/Bytecode/Dream
    std::string origin;           // Where created: "repl:line-5", "native:init", "dream:mutant-42"
    std::string traits;           // Freeform tags: "mutable", "pure", "mirror", "hot-path"
    size_t continuation_snapshot_id;  // Optional: for time-travel debugging (0 = no snapshot)

    // Constructor
    LedgerEntry()
        : backend(BackendTag::Scheme)
        , origin("unknown")
        , traits("")
        , continuation_snapshot_id(0)
    {}

    LedgerEntry(BackendTag b, const std::string& o, const std::string& t = "")
        : backend(b)
        , origin(o)
        , traits(t)
        , continuation_snapshot_id(0)
    {}
};

// ============================================================================
// Value - Universal Type
// ============================================================================

/**
 * @brief Universal value representation with swappable behavior
 *
 * A Value is:
 * - A pointer to the actual data (ptr)
 * - A vtable for behavior (vt)
 * - A ledger entry for provenance (ledger)
 *
 * This allows:
 * - Cross-language interop (C++ ↔ Scheme seamlessly)
 * - Runtime behavior swapping ("use-skin")
 * - Full provenance tracking (debugging, profiling)
 * - Time-travel debugging (Continuation snapshots)
 *
 * Example usage:
 *   Value v;
 *   v.ptr = my_scheme_object;
 *   v.vt = &scheme_skin;
 *   v.ledger = LedgerEntry(BackendTag::Scheme, "repl:line-1");
 *
 *   // Later, swap to native skin for performance:
 *   ledger.apply_skin(v, "native");
 */
struct Value {
    void* ptr;                   // Pointer to actual data (Object*, native type, etc.)
    const ValueVTable* vt;       // Behavior (skin)
    LedgerEntry ledger;          // Provenance (where/how/why)

    // Default constructor - null value
    Value()
        : ptr(nullptr)
        , vt(nullptr)
        , ledger()
    {}

    // Construct with all fields
    Value(void* p, const ValueVTable* v, const LedgerEntry& l)
        : ptr(p)
        , vt(v)
        , ledger(l)
    {}

    // Query operations (safe, check for nullptr)

    bool is_null() const { return ptr == nullptr; }

    bool is_callable() const { return vt && vt->invoke != nullptr; }

    std::string to_string() const {
        if (!ptr || !vt || !vt->to_string) return "#<null>";
        return vt->to_string(ptr);
    }

    bool is_truthy() const {
        if (!ptr || !vt || !vt->is_truthy) return false;
        return vt->is_truthy(ptr);
    }

    // Invoke if callable (throws if not callable)
    void* invoke(const std::vector<void*>& args) const {
        if (!is_callable()) {
            throw std::runtime_error("Value is not callable");
        }
        return vt->invoke(ptr, args);
    }

    // Cleanup
    void destroy() {
        if (ptr && vt && vt->destroy) {
            vt->destroy(ptr);
            ptr = nullptr;
        }
    }

    // Provenance queries
    BackendTag backend() const { return ledger.backend; }
    const std::string& origin() const { return ledger.origin; }
    const std::string& traits() const { return ledger.traits; }
};

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * @brief Create a null Value (sentinel)
 */
inline Value make_null_value() {
    return Value();
}

/**
 * @brief Check if two values point to the same underlying data
 */
inline bool value_eq(const Value& a, const Value& b) {
    return a.ptr == b.ptr;
}

/**
 * @brief Check if value is from a specific backend
 */
inline bool is_backend(const Value& v, BackendTag tag) {
    return v.backend() == tag;
}

/**
 * @brief Check if value has a specific trait
 */
inline bool has_trait(const Value& v, const std::string& trait) {
    return v.traits().find(trait) != std::string::npos;
}
