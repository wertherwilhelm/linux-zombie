#pragma once

#include "library.h"
#include "gc_ptr.h"
#include <map>
#include <vector>

/**
 * @file library_registry.h
 * @brief Global registry for R7RS libraries.
 *
 * Manages library registration, lookup, and dependency resolution.
 *
 * @code
 * auto& registry = LibraryRegistry::instance();
 * registry.register_library(std::move(lib));
 * Library* lib = registry.get_library({"scheme", "base"});
 * @endcode
 *
 * @note Not thread-safe (single-threaded interpreter).
 */

namespace r7rs {

/**
 * @brief Singleton registry for all libraries in the system.
 */
class LibraryRegistry {
private:
    std::map<LibraryName, gc_ptr<Library>, LibraryNameCompare> libraries_;

    // Singleton - private constructor
    LibraryRegistry() = default;

    // Prevent copy/move
    LibraryRegistry(const LibraryRegistry&) = delete;
    LibraryRegistry& operator=(const LibraryRegistry&) = delete;

public:
    /**
     * @brief Singleton access.
     * @return Registry instance.
     */
    static LibraryRegistry& instance() {
        static LibraryRegistry registry;
        return registry;
    }

    /**
     * @brief Register a library.
     * @param lib Library to register.
     */
    void register_library(gc_ptr<Library> lib);

    /**
     * @brief Lookup a library by name.
     * @param name Library name.
     * @return Library pointer or nullptr if not found.
     */
    Library* get_library(const LibraryName& name);

    /**
     * @brief Lookup a library by name (const).
     * @param name Library name.
     * @return Library pointer or nullptr if not found.
     */
    const Library* get_library(const LibraryName& name) const;

    /**
     * @brief Check if a library exists.
     * @param name Library name.
     * @return True if registered.
     */
    bool has_library(const LibraryName& name) const {
        return libraries_.find(name) != libraries_.end();
    }

    /**
     * @brief Get all registered library names.
     * @return Vector of library names.
     */
    std::vector<LibraryName> all_libraries() const;

    /**
     * @brief Clear all libraries (for testing).
     */
    void clear() {
        libraries_.clear();
    }

    /**
     * @brief Get number of registered libraries.
     * @return Count of libraries.
     */
    size_t library_count() const {
        return libraries_.size();
    }

    /**
     * @brief Resolve dependencies in dependency order (dependencies first).
     * @param name Root library name.
     * @return Vector of libraries in dependency order.
     */
    std::vector<Library*> resolve_dependencies(const LibraryName& name);

    /**
     * @brief Check for circular dependencies.
     * @param name Root library name.
     * @return True if a cycle is detected.
     */
    bool has_circular_dependency(const LibraryName& name) const;

private:
    // Helper for dependency resolution
    void resolve_dependencies_recursive(
        const LibraryName& name,
        std::vector<Library*>& result,
        std::set<LibraryName, LibraryNameCompare>& visiting,
        std::set<LibraryName, LibraryNameCompare>& visited
    );
};

} // namespace r7rs
