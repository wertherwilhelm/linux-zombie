#pragma once

/**
 * @file export_registry.h
 * @brief Thread-safe runtime registry for named C++/Scheme function exports
 *
 * The ExportRegistry provides:
 * - Named registration of C++ and Scheme functions
 * - Runtime lookup by name
 * - Mirror registration for cross-language calling
 * - Thread-safe access with mutex protection
 *
 * Usage:
 *   // Register a C++ function
 *   ExportRegistry::instance().register_export("cpp-add", add_fn, origin, traits);
 *
 *   // Look up and call
 *   auto* entry = ExportRegistry::instance().find("cpp-add");
 *   if (entry && entry->proc.is_valid()) {
 *       Object* result = entry->proc.call(args);
 *   }
 *
 *   // List all exports
 *   auto names = ExportRegistry::instance().list_names();
 */

#include "scheme_fn.h"
#include "value.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>

/**
 * @brief Entry in the export registry
 *
 * Stores all metadata about an exported function:
 * - name: The exported name (how to look it up)
 * - proc: The callable scheme_fn wrapper
 * - value: Optional Value wrapper with full provenance
 * - origin: Where this export came from ("cpp:init", "scheme:repl")
 * - traits: Properties ("native", "pure", "hot-path")
 */
struct ExportEntry {
    std::string name;       // Export name
    scheme_fn proc;         // Callable wrapper
    Value value;            // Full provenance wrapper (optional)
    std::string origin;     // Where it came from
    std::string traits;     // Properties

    ExportEntry() = default;

    ExportEntry(const std::string& n, const scheme_fn& p,
                const std::string& o = "", const std::string& t = "")
        : name(n), proc(p), origin(o), traits(t) {}
};

/**
 * @brief Thread-safe global registry for function exports
 *
 * Singleton pattern with mutex protection for thread safety.
 * Supports both direct exports and mirrors for cross-language calling.
 */
class ExportRegistry {
public:
    /**
     * @brief Get the global singleton instance
     */
    static ExportRegistry& instance() {
        static ExportRegistry registry;
        return registry;
    }

    // ========================================================================
    // Export Registration
    // ========================================================================

    /**
     * @brief Register an export by name
     *
     * @param name Export name (how to look it up)
     * @param proc Callable scheme_fn wrapper
     * @param origin Where this export came from
     * @param traits Properties of this export
     *
     * If an export with this name already exists, it will be replaced.
     * Thread-safe.
     */
    void register_export(const std::string& name, const scheme_fn& proc,
                         const std::string& origin = "",
                         const std::string& traits = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        exports_[name] = ExportEntry(name, proc, origin, traits);
    }

    /**
     * @brief Register an export with full Value wrapper
     *
     * Use this when you want to track full provenance via the ledger.
     */
    void register_export(const std::string& name, const scheme_fn& proc,
                         const Value& val,
                         const std::string& origin = "",
                         const std::string& traits = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        ExportEntry entry(name, proc, origin, traits);
        entry.value = val;
        exports_[name] = entry;
    }

    /**
     * @brief Find an export by name
     *
     * @param name Export name to look up
     * @return Pointer to entry, or nullptr if not found
     *
     * Thread-safe. The returned pointer is valid until the next modification.
     */
    const ExportEntry* find(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = exports_.find(name);
        return it == exports_.end() ? nullptr : &it->second;
    }

    /**
     * @brief Get a copy of an export entry (safer for long-term use)
     */
    std::optional<ExportEntry> get(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = exports_.find(name);
        if (it == exports_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    /**
     * @brief Check if an export exists
     */
    bool has(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return exports_.find(name) != exports_.end();
    }

    /**
     * @brief List all export names
     */
    std::vector<std::string> list_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(exports_.size());
        for (const auto& pair : exports_) {
            names.push_back(pair.first);
        }
        return names;
    }

    /**
     * @brief Get all exports
     */
    std::vector<ExportEntry> list_all() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<ExportEntry> entries;
        entries.reserve(exports_.size());
        for (const auto& pair : exports_) {
            entries.push_back(pair.second);
        }
        return entries;
    }

    /**
     * @brief Remove an export by name
     */
    bool remove(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        return exports_.erase(name) > 0;
    }

    /**
     * @brief Clear all exports
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        exports_.clear();
    }

    /**
     * @brief Get count of exports
     */
    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return exports_.size();
    }

    // ========================================================================
    // Mirror Registration (Cross-Language)
    // ========================================================================

    /**
     * @brief Register a mirror (cross-language adapter)
     *
     * Mirrors are used when you want to call a function from the "other side"
     * (C++ calling Scheme or Scheme calling C++).
     */
    void register_mirror(const std::string& name, const Value& mirror) {
        std::lock_guard<std::mutex> lock(mutex_);
        mirrors_[name] = mirror;
    }

    /**
     * @brief Find a mirror by name
     */
    const Value* find_mirror(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = mirrors_.find(name);
        return it == mirrors_.end() ? nullptr : &it->second;
    }

    /**
     * @brief Check if a mirror exists
     */
    bool has_mirror(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return mirrors_.find(name) != mirrors_.end();
    }

    /**
     * @brief List all mirror names
     */
    std::vector<std::string> list_mirror_names() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(mirrors_.size());
        for (const auto& pair : mirrors_) {
            names.push_back(pair.first);
        }
        return names;
    }

    /**
     * @brief Remove a mirror by name
     */
    bool remove_mirror(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        return mirrors_.erase(name) > 0;
    }

    /**
     * @brief Clear all mirrors
     */
    void clear_mirrors() {
        std::lock_guard<std::mutex> lock(mutex_);
        mirrors_.clear();
    }

    // ========================================================================
    // Calling Exports
    // ========================================================================

    /**
     * @brief Call an export by name with raw Scheme arguments
     *
     * @param name Export name
     * @param args Scheme argument list
     * @param env Environment for evaluation (optional)
     * @return Result Object, or nullptr if not found
     */
    Object* call(const std::string& name, Object* args, Environment* env = nullptr) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = exports_.find(name);
        if (it == exports_.end() || !it->second.proc.is_valid()) {
            return nullptr;
        }
        // Note: We need to release the lock before calling to avoid deadlock
        scheme_fn proc = it->second.proc;
        lock.~lock_guard();  // Release lock
        return proc.call(args, env);
    }

private:
    ExportRegistry() = default;
    ExportRegistry(const ExportRegistry&) = delete;
    ExportRegistry& operator=(const ExportRegistry&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ExportEntry> exports_;
    std::unordered_map<std::string, Value> mirrors_;
};

// ============================================================================
// Global Access Functions (Convenience)
// ============================================================================

/**
 * @brief Register an export (convenience function)
 */
inline void register_export(const std::string& name, const scheme_fn& proc,
                            const std::string& origin = "",
                            const std::string& traits = "") {
    ExportRegistry::instance().register_export(name, proc, origin, traits);
}

/**
 * @brief Find an export by name (convenience function)
 */
inline const ExportEntry* find_export(const std::string& name) {
    return ExportRegistry::instance().find(name);
}

/**
 * @brief Call an export by name (convenience function)
 */
inline Object* call_export(const std::string& name, Object* args, Environment* env = nullptr) {
    return ExportRegistry::instance().call(name, args, env);
}

/**
 * @brief List all export names (convenience function)
 */
inline std::vector<std::string> list_exports() {
    return ExportRegistry::instance().list_names();
}
