#pragma once

/**
 * @file ledger.h
 * @brief Skin registry and value provenance management
 *
 * The Ledger is the "wardrobe" of the multiverse - it holds all the different
 * "skins" (behavior sets) that values can wear. Need to make a Scheme value
 * act like a native function? Apply the native skin! Want to see full debug
 * info? Apply the debug skin!
 *
 * Philosophy:
 * - Skins are registered once, reused forever
 * - Applying a skin just swaps the vtable pointer (zero cost!)
 * - Traits are accumulated, not replaced (history matters)
 * - Ledger snapshots enable time-travel debugging
 * - Stable ValueId avoids GC-related pointer invalidation
 *
 * Example:
 *   Ledger ledger;
 *   ledger.register_skin(scheme_skin);
 *   ledger.register_skin(native_skin);
 *
 *   Value v = create_scheme_value();
 *   ledger.apply_skin(v, "native");  // Now it's fast!
 */

#include "value.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <stdexcept>
#include <cstdint>

// Forward declarations
class Object;

// Type alias for stable value identity (avoids pointer invalidation with moving GC)
using ValueId = uint64_t;

// ============================================================================
// Skin - A Named Set of Behaviors
// ============================================================================

/**
 * @brief A "skin" is a named vtable with associated traits
 *
 * Think of skins as costumes for values:
 * - "scheme": Behaves like Scheme (slow, safe, debuggable)
 * - "native": Behaves like C++ (fast, optimized)
 * - "mirror": Cross-language adapter
 * - "dream-42": Genetic variant #42 (experimental!)
 *
 * Each skin has:
 * - A name (for lookup)
 * - A vtable (behavior)
 * - Default traits (properties that come with this skin)
 */
struct Skin {
    std::string name;           // Unique identifier: "scheme", "native", "mirror"
    ValueVTable vt;             // Behavior implementation
    std::string default_traits; // Traits that come with this skin: "safe", "fast", "experimental"

    Skin() = default;

    Skin(const std::string& n, const ValueVTable& vtable, const std::string& traits = "")
        : name(n)
        , vt(vtable)
        , default_traits(traits)
    {}
};

// ============================================================================
// Ledger Snapshot - For Time Travel
// ============================================================================

/**
 * @brief Captures the state of all values at a point in time
 *
 * When you capture a Continuation, you can also capture the ledger state.
 * This lets you restore not just the control flow, but also the provenance
 * and behavior of all values!
 *
 * Used for:
 * - Time-travel debugging (restore to previous state)
 * - Multiverse branching (fork realities)
 * - Comparative analysis (see how values evolved)
 */
struct LedgerSnapshot {
    size_t id;                                      // Unique snapshot ID
    std::string description;                        // "Before optimization", "Dream variant #7"
    std::unordered_map<ValueId, LedgerEntry> entries;  // Map ValueId → ledger entry (GC-safe!)

    LedgerSnapshot() : id(0), description("") {}

    LedgerSnapshot(size_t snapshot_id, const std::string& desc)
        : id(snapshot_id)
        , description(desc)
    {}
};

// ============================================================================
// Ledger - The Skin Registry & Provenance Tracker
// ============================================================================

/**
 * @brief Central registry for skins and value provenance
 *
 * The Ledger is where the magic happens:
 * - Register skins (behavior sets)
 * - Apply skins to values (swap vtables)
 * - Track value provenance (where did it come from?)
 * - Capture/restore snapshots (time travel!)
 *
 * Example usage:
 *   Ledger ledger;
 *
 *   // Register skins
 *   ledger.register_skin(Skin("scheme", scheme_vtable, "safe debuggable"));
 *   ledger.register_skin(Skin("native", native_vtable, "fast optimized"));
 *
 *   // Create a value
 *   Value v = wrap_scheme_object(obj);
 *
 *   // Swap to native skin (faster!)
 *   ledger.apply_skin(v, "native");
 *
 *   // Check what happened
 *   std::cout << v.origin() << "\n";  // "repl:line-5"
 *   std::cout << v.traits() << "\n";  // "safe debuggable fast optimized"
 */
class Ledger {
public:
    Ledger() : next_snapshot_id_(1), next_value_id_(1) {}

    // ========================================================================
    // Skin Registration & Lookup
    // ========================================================================

    /**
     * @brief Register a new skin (behavior set)
     *
     * Skins must have unique names. If a skin with this name already exists,
     * it will be replaced.
     */
    void register_skin(const Skin& skin) {
        skins_[skin.name] = skin;
    }

    /**
     * @brief Find a registered skin by name
     * @return Pointer to skin, or nullptr if not found
     */
    const Skin* find_skin(const std::string& name) const {
        auto it = skins_.find(name);
        return it == skins_.end() ? nullptr : &it->second;
    }

    /**
     * @brief Check if a skin is registered
     */
    bool has_skin(const std::string& name) const {
        return skins_.find(name) != skins_.end();
    }

    /**
     * @brief Get all registered skin names
     */
    std::vector<std::string> list_skins() const {
        std::vector<std::string> names;
        names.reserve(skins_.size());
        for (const auto& pair : skins_) {
            names.push_back(pair.first);
        }
        return names;
    }

    // ========================================================================
    // Skin Application - Transform Values!
    // ========================================================================

    /**
     * @brief Apply a skin to a value (swap behavior)
     *
     * This is where the magic happens! Applying a skin:
     * 1. Swaps the vtable pointer (changes behavior)
     * 2. Accumulates traits (adds skin's traits to existing)
     * 3. Preserves origin (history matters!)
     *
     * Example:
     *   ledger.apply_skin(v, "native");  // Now it's fast!
     *
     * @param v Value to transform
     * @param skin_name Name of skin to apply
     * @throws std::runtime_error if skin not found
     */
    void apply_skin(Value& v, const std::string& skin_name) {
        const Skin* skin = find_skin(skin_name);
        if (!skin) {
            throw std::runtime_error("Skin not found: " + skin_name);
        }

        // Swap vtable (behavior change!)
        v.vt = &skin->vt;

        // Accumulate traits (preserve history)
        if (!v.ledger.traits.empty() && !skin->default_traits.empty()) {
            v.ledger.traits += " ";
        }
        v.ledger.traits += skin->default_traits;

        // Note: We preserve backend and origin - history matters!
    }

    /**
     * @brief Apply a skin and change backend tag
     *
     * Use this when you want to fully transform a value into a different
     * reality (e.g., Scheme → Native conversion).
     */
    void apply_skin_with_backend(Value& v, const std::string& skin_name, BackendTag new_backend) {
        apply_skin(v, skin_name);
        v.ledger.backend = new_backend;
    }

    // ========================================================================
    // Value Identity & Entry Management (NEW - GC-Safe API)
    // ========================================================================

    /**
     * @brief Ensure an Object has a ledger entry, assigning ValueId if needed
     *
     * This is the primary way to register values with the ledger. It:
     * 1. Assigns a stable ValueId if the Object doesn't have one
     * 2. Creates a ledger entry if one doesn't exist
     * 3. Preserves existing origin (doesn't overwrite!)
     * 4. Returns a reference to the entry for updates
     *
     * @param obj Scheme Object to register
     * @param origin Initial origin (only used if entry doesn't exist)
     * @param traits Initial traits (only used if entry doesn't exist)
     * @return Reference to the ledger entry
     */
    LedgerEntry& ensure_entry(Object* obj, const std::string& origin, const std::string& traits = "");

    /**
     * @brief Look up an Object's ledger entry without modifying it
     *
     * Non-mutating query - does not assign ValueId or create entry.
     * Use this when you want to check if an Object is tracked.
     *
     * @param obj Scheme Object to look up
     * @return Pointer to ledger entry, or nullptr if not tracked
     */
    const LedgerEntry* lookup_entry(Object* obj) const;

    /**
     * @brief Remove an Object from the ledger (for cleanup)
     *
     * Call this when an Object is being destroyed (e.g., in ~Object()).
     * Prevents ledger from growing unbounded.
     *
     * @param obj Scheme Object to remove
     */
    void forget(Object* obj);

    // ========================================================================
    // Legacy Provenance API (Deprecated - use ensure_entry/lookup_entry)
    // ========================================================================

    /**
     * @brief Record a value's ledger entry (DEPRECATED)
     * @deprecated Use ensure_entry() instead for GC safety
     *
     * This builds up a database of "who created what" for debugging and
     * analysis. Optional but useful for:
     * - Finding hot paths (count Native backend usage)
     * - Debugging (trace value origins)
     * - Profiling (see which skins are used most)
     */
    void record_value(void* ptr, const LedgerEntry& entry) {
        // Legacy API - not recommended (ptr can become invalid with moving GC)
        // Try to use Object* and ensure_entry() instead
        if (Object* obj = static_cast<Object*>(ptr)) {
            ensure_entry(obj, entry.origin, entry.traits);
        }
    }

    /**
     * @brief Look up a value's provenance (DEPRECATED)
     * @deprecated Use lookup_entry() with Object* instead
     * @return Pointer to ledger entry, or nullptr if not tracked
     */
    const LedgerEntry* lookup_provenance(void* ptr) const {
        // Legacy API - not recommended
        if (Object* obj = static_cast<Object*>(ptr)) {
            return lookup_entry(obj);
        }
        return nullptr;
    }

    // ========================================================================
    // Snapshot System - Time Travel!
    // ========================================================================

    /**
     * @brief Capture current ledger state as a snapshot
     *
     * This enables time-travel debugging! You can:
     * - Capture state before an operation
     * - Try different optimizations
     * - Restore if things go wrong
     * - Compare realities side-by-side
     *
     * @param description Human-readable label: "Before optimization", "Dream #42"
     * @return Snapshot ID (for later restoration)
     */
    size_t capture_snapshot(const std::string& description = "") {
        size_t id = next_snapshot_id_++;

        LedgerSnapshot snapshot(id, description);
        snapshot.entries = provenance_;  // Copy current state

        snapshots_[id] = snapshot;
        return id;
    }

    /**
     * @brief Restore ledger state from a snapshot
     *
     * Time travel! This restores the provenance tracking to a previous state.
     * Combined with Continuation snapshots, you can fully restore a reality!
     *
     * @param snapshot_id ID returned by capture_snapshot
     * @throws std::runtime_error if snapshot not found
     */
    void restore_snapshot(size_t snapshot_id) {
        auto it = snapshots_.find(snapshot_id);
        if (it == snapshots_.end()) {
            throw std::runtime_error("Snapshot not found: " + std::to_string(snapshot_id));
        }

        provenance_ = it->second.entries;  // Restore state
    }

    /**
     * @brief Get snapshot metadata
     */
    const LedgerSnapshot* get_snapshot(size_t snapshot_id) const {
        auto it = snapshots_.find(snapshot_id);
        return it == snapshots_.end() ? nullptr : &it->second;
    }

    /**
     * @brief List all snapshots
     */
    std::vector<size_t> list_snapshots() const {
        std::vector<size_t> ids;
        ids.reserve(snapshots_.size());
        for (const auto& pair : snapshots_) {
            ids.push_back(pair.first);
        }
        return ids;
    }

    // ========================================================================
    // Statistics & Analysis
    // ========================================================================

    /**
     * @brief Count values by backend tag
     *
     * Useful for profiling:
     * - How many values are in Scheme backend? (maybe too many!)
     * - How many in Native? (good for performance!)
     * - How many Dream variants? (experimentation level)
     */
    std::unordered_map<BackendTag, size_t> count_by_backend() const {
        std::unordered_map<BackendTag, size_t> counts;
        for (const auto& pair : provenance_) {
            counts[pair.second.backend]++;
        }
        return counts;
    }

    /**
     * @brief Find all values with a specific trait
     * @return Vector of ValueIds (GC-safe!)
     * @note Returns stable IDs, not pointers (safe across GC)
     */
    std::vector<ValueId> find_by_trait(const std::string& trait) const {
        std::vector<ValueId> results;
        for (const auto& pair : provenance_) {
            if (pair.second.traits.find(trait) != std::string::npos) {
                results.push_back(pair.first);  // pair.first is now ValueId, not void*!
            }
        }
        return results;
    }

    /**
     * @brief Get total number of tracked values
     */
    size_t tracked_count() const {
        return provenance_.size();
    }

private:
    std::unordered_map<std::string, Skin> skins_;           // Registered skins
    std::unordered_map<ValueId, LedgerEntry> provenance_;   // ValueId → ledger entry (GC-safe!)
    std::unordered_map<size_t, LedgerSnapshot> snapshots_;  // Snapshot ID → snapshot
    size_t next_snapshot_id_;                               // For generating unique snapshot IDs
    ValueId next_value_id_;                                 // For generating unique value IDs
};

// ============================================================================
// Global Ledger Instance (Optional)
// ============================================================================

/**
 * @brief Global ledger for convenience
 *
 * You can use this or create your own Ledger instances.
 * Multiple ledgers = multiple realities!
 */
extern Ledger* g_ledger;

/**
 * @brief Initialize global ledger
 */
inline void init_global_ledger() {
    if (!g_ledger) {
        g_ledger = new Ledger();
    }
}

/**
 * @brief Cleanup global ledger
 */
inline void shutdown_global_ledger() {
    if (g_ledger) {
        delete g_ledger;
        g_ledger = nullptr;
    }
}
