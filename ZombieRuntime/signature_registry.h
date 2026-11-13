#pragma once

/**
 * @file signature_registry.h
 * @brief Thread-safe compile-time and runtime function signature registry
 *
 * The SignatureRegistry provides:
 * - Compile-time registration via REGISTER_SIGNATURE Macro
 * - Runtime lookup by name
 * - Arity and type signature storage
 * - Thread-safe access with mutex protection
 *
 * Usage:
 *   // At file scope (compile-time registration):
 *   REGISTER_SIGNATURE("square", "double(double)", 1);
 *   REGISTER_SIGNATURE("add", "double(double,double)", 2);
 *
 *   // At runtime:
 *   const SignatureEntry* sig = SignatureRegistry::instance().find("square");
 *   if (sig) {
 *       std::cout << sig->signature << " with arity " << sig->arity << "\n";
 *   }
 *
 *   // List all registered signatures:
 *   auto all = SignatureRegistry::instance().list();
 *   for (const auto& entry : all) {
 *       std::cout << entry.name << ": " << entry.signature << "\n";
 *   }
 */

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstddef>

/**
 * @brief Entry in the signature registry
 *
 * Stores the name, type signature string, and arity of a function.
 */
struct SignatureEntry {
    std::string name;       // Function name: "square", "add", "map"
    std::string signature;  // Type signature: "double(double)", "int(int,int)"
    size_t arity;           // Number of parameters

    SignatureEntry() : arity(0) {}

    SignatureEntry(const std::string& n, const std::string& sig, size_t a)
        : name(n), signature(sig), arity(a) {}
};

/**
 * @brief Thread-safe global registry for function signatures
 *
 * Singleton pattern with mutex protection for thread safety.
 */
class SignatureRegistry {
public:
    /**
     * @brief Get the global singleton instance
     */
    static SignatureRegistry& instance() {
        static SignatureRegistry registry;
        return registry;
    }

    /**
     * @brief Register a function signature
     *
     * @param name Function name
     * @param signature Type signature string (e.g., "double(double,double)")
     * @param arity Number of parameters
     *
     * If a function with this name is already registered, it will be replaced.
     * Thread-safe.
     */
    void register_signature(const std::string& name, const std::string& signature, size_t arity) {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_[name] = SignatureEntry(name, signature, arity);
    }

    /**
     * @brief Find a registered signature by name
     *
     * @param name Function name to look up
     * @return Pointer to entry, or nullptr if not found
     *
     * Thread-safe. The returned pointer is valid until the next modification.
     */
    const SignatureEntry* find(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(name);
        return it == entries_.end() ? nullptr : &it->second;
    }

    /**
     * @brief Check if a signature is registered
     */
    bool has(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.find(name) != entries_.end();
    }

    /**
     * @brief Get all registered signatures
     *
     * @return Vector of all signature entries (copy for thread safety)
     */
    std::vector<SignatureEntry> list() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<SignatureEntry> result;
        result.reserve(entries_.size());
        for (const auto& pair : entries_) {
            result.push_back(pair.second);
        }
        return result;
    }

    /**
     * @brief Get count of registered signatures
     */
    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.size();
    }

    /**
     * @brief Remove a signature by name
     *
     * @return true if removed, false if not found
     */
    bool remove(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_.erase(name) > 0;
    }

    /**
     * @brief Clear all registered signatures
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.clear();
    }

    /**
     * @brief Validate arity matches registered signature
     *
     * @param name Function name
     * @param actual_arity Actual number of arguments
     * @return true if matches (or not registered), false if mismatch
     */
    bool validate_arity(const std::string& name, size_t actual_arity) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = entries_.find(name);
        if (it == entries_.end()) {
            return true;  // Not registered = no validation
        }
        return it->second.arity == actual_arity;
    }

private:
    SignatureRegistry() = default;
    SignatureRegistry(const SignatureRegistry&) = delete;
    SignatureRegistry& operator=(const SignatureRegistry&) = delete;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, SignatureEntry> entries_;
};

/**
 * @brief Helper class for compile-time registration
 *
 * Used by REGISTER_SIGNATURE Macro to register at static initialization time.
 */
class SignatureRegistrar {
public:
    SignatureRegistrar(const char* name, const char* signature, size_t arity) {
        SignatureRegistry::instance().register_signature(name, signature, arity);
    }
};

/**
 * @brief Macro for compile-time signature registration
 *
 * Usage at file scope:
 *   REGISTER_SIGNATURE("square", "double(double)", 1);
 *
 * This creates a static Object that registers the signature during
 * static initialization, before main() runs.
 */
#define REGISTER_SIGNATURE(name, signature, arity) \
    static SignatureRegistrar _sig_registrar_##__COUNTER__(name, signature, arity)

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Parse arity from a signature string
 *
 * Counts commas between parentheses, handling nested parens.
 * "double(double)" → 1
 * "double(double,double)" → 2
 * "void()" → 0
 *
 * @param signature Type signature string
 * @return Estimated arity, or 0 if parse fails
 */
inline size_t parse_arity_from_signature(const std::string& signature) {
    size_t open_paren = signature.find('(');
    size_t close_paren = signature.rfind(')');

    if (open_paren == std::string::npos || close_paren == std::string::npos) {
        return 0;
    }

    std::string args = signature.substr(open_paren + 1, close_paren - open_paren - 1);

    if (args.empty()) {
        return 0;
    }

    size_t arity = 1;
    int depth = 0;
    for (char c : args) {
        if (c == '(') depth++;
        else if (c == ')') depth--;
        else if (c == ',' && depth == 0) arity++;
    }

    return arity;
}
