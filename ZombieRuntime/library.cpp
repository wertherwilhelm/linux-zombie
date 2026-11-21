#include "zombie_runtime_types.h"
#include "library.h"
#include "library_registry.h"
#include "Base.h"
#include "atom.h"
#include "Lisp.h"
#include <sstream>

namespace r7rs {

// ============================================================================
// Library Name Utilities
// ============================================================================

std::string library_name_to_string(const LibraryName& name) {
    if (name.empty()) {
        return "<anonymous>";
    }

    std::ostringstream oss;
    oss << "(";
    for (size_t i = 0; i < name.size(); ++i) {
        if (i > 0) oss << " ";
        oss << name[i];
    }
    oss << ")";
    return oss.str();
}

LibraryName parse_library_name(Object* name_list) {
    LibraryName result;

    // Name must be a list of symbols
    if (is_nil(name_list)) {
        throw std::runtime_error("Library name cannot be empty");
    }

    Object* current = name_list;
    while (!is_nil(current)) {
        if (!current->pair()) {
            throw std::runtime_error("Library name must be a proper list");
        }

        Object* elem = car(current);
        if (!elem || elem->get_type() != static_cast<int>(Type::SYMBOL)) {
            throw std::runtime_error("Library name components must be symbols");
        }

        // Extract symbol name
        a_symbol* sym = static_cast<a_symbol*>(elem);
        std::string component = sym->getName();
        result.push_back(component);

        current = cdr(current);
    }

    return result;
}

// ============================================================================
// Library Implementation
// ============================================================================

bool Library::validate_exports(std::string& errors_out) const {
    bool valid = true;

    // Check that all exported symbols are defined in the library
    for (const auto& exp : exports_) {
        if (!has_symbol(exp.internal_name)) {
            if (!errors_out.empty()) {
                errors_out += "\n";
            }
            errors_out += "Library " + name_string() + ": ";
            errors_out += "exported symbol '" + exp.internal_name + "' is not defined";
            valid = false;
        }
    }

    return valid;
}

bool Library::validate_imports(std::string& errors_out) const {
    // Import validation will be done by LibraryRegistry when resolving dependencies
    // For now, just check that import specs are well-formed
    for (const auto& import : imports_) {
        if (import.library.empty()) {
            if (!errors_out.empty()) {
                errors_out += "\n";
            }
            errors_out += "Library " + name_string() + ": ";
            errors_out += "import has empty library name";
            return false;
        }
    }
    return true;
}

bool Library::validate(std::string& errors_out) const {
    bool valid = true;

    if (!validate_imports(errors_out)) {
        valid = false;
    }

    if (!validate_exports(errors_out)) {
        valid = false;
    }

    return valid;
}

bool Library::has_symbol(const std::string& name) const {
    if (!env_) {
        return false;
    }

    // Check if symbol is bound in library's environment
    // lookup() throws exception if not found, so we need to catch it
    try {
        Object* result = env_->lookup(name.c_str());
        return result != nullptr && result != nil;
    } catch (const std::exception&) {
        return false;
    }
}

Object* Library::get_symbol(const std::string& name) const {
    if (!env_) {
        return nullptr;
    }

    // lookup() throws exception if not found
    try {
        return env_->lookup(name.c_str());
    } catch (const std::exception&) {
        return nullptr;
    }
}

void Library::define_symbol(const std::string& name, Object* value) {
    if (!env_) {
        create_environment();
    }

    env_->define(name.c_str(), value);
}

// ============================================================================
// Library Initialization
// ============================================================================

bool Library::initialize(Environment* parent_env, std::string& error_out) {
    if (initialized_) {
        return true;  // Already initialized
    }

    // Track libraries currently being initialized to detect circular dependencies
    static std::set<LibraryName, LibraryNameCompare> initializing;

    // Check for circular dependency
    if (initializing.find(name_) != initializing.end()) {
        error_out = "Circular dependency detected: library " + name_string() +
                    " is already being initialized (forms a dependency cycle)";
        return false;
    }

    // Mark this library as currently initializing
    initializing.insert(name_);

    // RAII guard to ensure library is removed from initializing set
    struct InitGuard {
        std::set<LibraryName, LibraryNameCompare>& set;
        const LibraryName& name;
        InitGuard(std::set<LibraryName, LibraryNameCompare>& s, const LibraryName& n) : set(s), name(n) {}
        ~InitGuard() { set.erase(name); }
    };
    InitGuard guard(initializing, name_);

    // 1. Create environment with parent
    if (!env_) {
        // Create new environment with vars=nil, vals=nil, parent=parent_env
        env_ = gc_new<Environment>(name_string(), nil, nil, parent_env);
    } else if (parent_env) {
        env_->set_parent(parent_env);
    }

    // 2. Process imports - bind symbols from imported libraries
    LibraryRegistry& registry = LibraryRegistry::instance();

    for (const auto& import_spec : imports_) {
        // Get the imported library
        Library* imported_lib = registry.get_library(import_spec.library);
        if (!imported_lib) {
            error_out = "Library " + name_string() + ": ";
            error_out += "cannot find imported library " + library_name_to_string(import_spec.library);
            return false;
        }

        // Make sure the imported library is initialized
        if (!imported_lib->is_initialized()) {
            std::string init_error;
            if (!imported_lib->initialize(parent_env, init_error)) {
                error_out = "Library " + name_string() + ": ";
                error_out += "failed to initialize imported library " + library_name_to_string(import_spec.library);
                error_out += ": " + init_error;
                return false;
            }
        }

        // Bind the imported symbols into this library's own environment.
        resolve_import(import_spec, imported_lib,
                       [this](const std::string& name, Object* value) { define_symbol(name, value); });
    }

    // 3. Evaluate begin blocks
    // Note: This is done externally by the caller after initialize() returns
    // The begin blocks are stored in begin_blocks_ and can be evaluated using
    // a Lisp evaluator in the library's environment

    // 4. Mark as initialized
    initialized_ = true;
    return true;
}

void resolve_import(const ImportSpec& spec, Library* source,
                    const std::function<void(const std::string&, Object*)>& bind) {
    for (const auto& exp : source->exports()) {
        std::string name;  // the name to bind under, or skip this export
        switch (spec.type) {
        case ImportSpec::SIMPLE:
            name = exp.external_name;
            break;
        case ImportSpec::ONLY:
            if (spec.symbols.find(exp.external_name) == spec.symbols.end()) continue;
            name = exp.external_name;
            break;
        case ImportSpec::EXCEPT:
            if (spec.symbols.find(exp.external_name) != spec.symbols.end()) continue;
            name = exp.external_name;
            break;
        case ImportSpec::PREFIX:
            name = spec.prefix + exp.external_name;
            break;
        case ImportSpec::RENAME: {
            auto it = spec.renames.find(exp.external_name);
            name = (it != spec.renames.end()) ? it->second : exp.external_name;
            break;
        }
        }
        Object* value = source->get_symbol(exp.internal_name);
        if (value) bind(name, value);
    }
}

} // namespace r7rs
