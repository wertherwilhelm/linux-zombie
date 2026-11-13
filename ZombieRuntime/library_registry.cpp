#include "pch.h"
#include "library_registry.h"
#include <stdexcept>
#include <algorithm>

namespace r7rs {

// ============================================================================
// Library Registration
// ============================================================================

void LibraryRegistry::register_library(gc_ptr<Library> lib) {
    if (!lib) {
        throw std::runtime_error("Cannot register null library");
    }

    const LibraryName& name = lib->name();
    if (name.empty()) {
        throw std::runtime_error("Cannot register library with empty name");
    }

    // Check for duplicate registration - silently skip if already registered
    // This allows multiple scheme_engine instances to coexist
    if (has_library(name)) {
        return;  // Already registered, nothing to do
    }

    libraries_[name] = lib;  // gc_ptr is copyable
}

// ============================================================================
// Library Lookup
// ============================================================================

Library* LibraryRegistry::get_library(const LibraryName& name) {
    auto it = libraries_.find(name);
    if (it != libraries_.end()) {
        return it->second.get();
    }
    return nullptr;
}

const Library* LibraryRegistry::get_library(const LibraryName& name) const {
    auto it = libraries_.find(name);
    if (it != libraries_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<LibraryName> LibraryRegistry::all_libraries() const {
    std::vector<LibraryName> result;
    result.reserve(libraries_.size());

    for (const auto& pair : libraries_) {
        result.push_back(pair.first);
    }

    return result;
}

// ============================================================================
// Dependency Resolution
// ============================================================================

std::vector<Library*> LibraryRegistry::resolve_dependencies(const LibraryName& name) {
    std::vector<Library*> result;
    std::set<LibraryName, LibraryNameCompare> visiting;
    std::set<LibraryName, LibraryNameCompare> visited;

    resolve_dependencies_recursive(name, result, visiting, visited);

    return result;
}

void LibraryRegistry::resolve_dependencies_recursive(
    const LibraryName& name,
    std::vector<Library*>& result,
    std::set<LibraryName, LibraryNameCompare>& visiting,
    std::set<LibraryName, LibraryNameCompare>& visited
) {
    // Check if already visited (dependency already resolved)
    if (visited.find(name) != visited.end()) {
        return;
    }

    // Check for circular dependency
    if (visiting.find(name) != visiting.end()) {
        throw std::runtime_error("Circular dependency detected: " + library_name_to_string(name));
    }

    // Get library
    Library* lib = get_library(name);
    if (!lib) {
        throw std::runtime_error("Library not found: " + library_name_to_string(name));
    }

    // Mark as visiting
    visiting.insert(name);

    // Recursively resolve dependencies
    for (const auto& import : lib->imports()) {
        resolve_dependencies_recursive(import.library, result, visiting, visited);
    }

    // Mark as visited and add to result
    visiting.erase(name);
    visited.insert(name);
    result.push_back(lib);
}

bool LibraryRegistry::has_circular_dependency(const LibraryName& name) const {
    try {
        // Use const_cast to call non-const version
        // This is safe because resolve_dependencies doesn't modify the registry
        auto& mutable_this = const_cast<LibraryRegistry&>(*this);
        mutable_this.resolve_dependencies(name);
        return false;
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        return msg.find("Circular dependency") != std::string::npos;
    }
}

} // namespace r7rs
