#pragma once

#include "Environment.h"
#include "gc_object.h"
#include "gc_tracer.h"
#include "gc_ptr.h"
#include <string>
#include <vector>
#include <set>
#include <map>

// ============================================================================
// R7RS Library System
// ============================================================================

/**
 * @defgroup r7rs_library_system R7RS Library System
 * @brief R7RS library (module) system.
 *
 * Implements:
 * - Library definition (define-library)
 * - Import declarations (import, only, except, prefix, rename)
 * - Export declarations (export)
 * - Namespace isolation
 * - Standard R7RS libraries
 *
 * @details
 * Architecture:
 * - Name (hierarchical): (scheme base) -> ["scheme", "base"]
 * - Import specs: dependencies on other libraries
 * - Export list: publicly visible symbols
 * - Environment: isolated namespace for definitions
 * - Begin blocks: forms to evaluate during library initialization
 *
 * @code
 * (define-library (myapp math)
 *   (import (scheme base))
 *   (export square cube)
 *   (begin
 *     (define (square x) (* x x))
 *     (define (cube x) (* x x x))))
 * @endcode
 */
namespace r7rs {

// Bring in types from zombie::runtime
using zombie::runtime::Object;
using zombie::runtime::Environment;
using zombie::runtime::is_nil;
using zombie::runtime::car;
using zombie::runtime::cdr;

// ============================================================================
// Library Name - Hierarchical identifier like (scheme base)
// ============================================================================

using LibraryName = std::vector<std::string>;

// Convert library name to string for display: (scheme base) -> "scheme.Base"
std::string library_name_to_string(const LibraryName& name);

// Parse library name from list: '(scheme base) -> LibraryName
LibraryName parse_library_name(Object* name_list);

// Comparison for map keys
struct LibraryNameCompare {
    bool operator()(const LibraryName& a, const LibraryName& b) const {
        return a < b;  // Lexicographic comparison
    }
};

// ============================================================================
// Import Specification - How to import from another library
// ============================================================================

struct ImportSpec {
    enum Type {
        SIMPLE,    // (import (lib))
        ONLY,      // (import (only (lib) sym1 sym2))
        EXCEPT,    // (import (except (lib) sym1 sym2))
        PREFIX,    // (import (prefix (lib) prefix:))
        RENAME     // (import (rename (lib) (old new) ...))
    };

    Type type;
    LibraryName library;
    std::set<std::string> symbols;              // For ONLY/EXCEPT
    std::string prefix;                         // For PREFIX
    std::map<std::string, std::string> renames; // For RENAME: old -> new

    // Constructors for each type
    static ImportSpec make_simple(const LibraryName& lib) {
        ImportSpec spec;
        spec.type = SIMPLE;
        spec.library = lib;
        return spec;
    }

    static ImportSpec make_only(const LibraryName& lib, const std::set<std::string>& syms) {
        ImportSpec spec;
        spec.type = ONLY;
        spec.library = lib;
        spec.symbols = syms;
        return spec;
    }

    static ImportSpec make_except(const LibraryName& lib, const std::set<std::string>& syms) {
        ImportSpec spec;
        spec.type = EXCEPT;
        spec.library = lib;
        spec.symbols = syms;
        return spec;
    }

    static ImportSpec make_prefix(const LibraryName& lib, const std::string& pfx) {
        ImportSpec spec;
        spec.type = PREFIX;
        spec.library = lib;
        spec.prefix = pfx;
        return spec;
    }

    static ImportSpec make_rename(const LibraryName& lib,
                                  const std::map<std::string, std::string>& rn) {
        ImportSpec spec;
        spec.type = RENAME;
        spec.library = lib;
        spec.renames = rn;
        return spec;
    }
};

// ============================================================================
// Export Specification - What to export from this library
// ============================================================================

struct ExportSpec {
    std::string internal_name;   // Name in library's namespace
    std::string external_name;   // Name visible to importers (may differ)

    ExportSpec(const std::string& name)
        : internal_name(name), external_name(name) {}

    ExportSpec(const std::string& internal, const std::string& external)
        : internal_name(internal), external_name(external) {}
};

// ============================================================================
// Library - R7RS library/module
// ============================================================================
//
// **GC Safety:** Library is a gc_object that contains GC-managed objects:
// - env_: Isolated environment for library definitions
// - begin_blocks_: Forms to evaluate during initialization
//
// Both are traced by trace_references() to ensure they're not collected
// and are updated correctly by moving GC.
//

class Library : public gc_object {
private:
    LibraryName name_;
    std::vector<ImportSpec> imports_;
    std::vector<ExportSpec> exports_;
    Environment* env_;                    // GC-managed, traced
    bool initialized_;
    std::vector<Object*> begin_blocks_;  // GC-managed forms, traced

public:
    // Constructors
    Library() : env_(nullptr), initialized_(false) {}

    explicit Library(const LibraryName& name)
        : name_(name), env_(nullptr), initialized_(false) {}

    Library(const std::string& name1, const std::string& name2)
        : name_({name1, name2}), env_(nullptr), initialized_(false) {}

    // Library name
    const LibraryName& name() const { return name_; }
    void set_name(const LibraryName& n) { name_ = n; }
    std::string name_string() const { return library_name_to_string(name_); }

    // Import management
    void add_import(const ImportSpec& spec) { imports_.push_back(spec); }
    void add_import(ImportSpec&& spec) { imports_.push_back(std::move(spec)); }
    const std::vector<ImportSpec>& imports() const { return imports_; }

    // Export management
    void add_export(const std::string& symbol) {
        exports_.emplace_back(symbol);
    }

    void add_export(const std::string& internal, const std::string& external) {
        exports_.emplace_back(internal, external);
    }

    void add_export(const ExportSpec& spec) {
        exports_.push_back(spec);
    }

    const std::vector<ExportSpec>& exports() const { return exports_; }

    bool exports_symbol(const std::string& symbol) const {
        for (const auto& exp : exports_) {
            if (exp.external_name == symbol) {
                return true;
            }
        }
        return false;
    }

    // Environment access
    Environment* environment() const { return env_; }

    void create_environment() {
        if (!env_) {
            env_ = gc_new<Environment>();
        }
    }

    // Initialization
    bool is_initialized() const { return initialized_; }
    void set_initialized(bool init) { initialized_ = init; }

    // Initialize library: process imports, create environment
    // parent_env: environment to use as parent (contains primitives)
    // Returns true on success, false on error
    bool initialize(Environment* parent_env, std::string& error_out);

    // Evaluate begin blocks in library's environment
    // evaluator: function pointer or lambda to evaluate expressions
    // Returns true on success, false on error
    template<typename EvalFunc>
    bool evaluate_begin_blocks(EvalFunc evaluator, std::string& error_out) {
        if (!env_) {
            error_out = "Library not initialized";
            return false;
        }

        // Evaluate each begin block
        for (Object* forms : begin_blocks_) {
            // forms is a list of definitions
            Object* current = forms;
            while (current && !is_nil(current)) {
                Object* form = car(current);
                try {
                    evaluator(form, env_);
                } catch (const std::exception& e) {
                    error_out = "Error evaluating form in library " + name_string() + ": " + e.what();
                    return false;
                } catch (...) {
                    error_out = "Unknown error evaluating form in library " + name_string();
                    return false;
                }
                current = cdr(current);
            }
        }

        return true;
    }

    // Begin block management
    void add_begin_block(Object* forms) { begin_blocks_.push_back(forms); }
    const std::vector<Object*>& begin_blocks() const { return begin_blocks_; }

    // Utilities
    void clear() {
        imports_.clear();
        exports_.clear();
        env_ = nullptr;
        begin_blocks_.clear();
        initialized_ = false;
    }

    size_t export_count() const { return exports_.size(); }
    size_t import_count() const { return imports_.size(); }

    // GC tracing
    void trace_references(Tracer& tracer) override {
        // Trace library environment
        if (env_) {
            tracer.visit_field(env_);
        }

        // Trace all begin block forms
        for (auto& block : begin_blocks_) {
            if (block) {
                tracer.visit_field(block);
            }
        }
    }

    // Validation
    bool validate_exports(std::string& errors_out) const;
    bool validate_imports(std::string& errors_out) const;
    bool validate(std::string& errors_out) const;

    // Symbol lookup (checks if symbol is defined in library)
    bool has_symbol(const std::string& name) const;
    Object* get_symbol(const std::string& name) const;
    void define_symbol(const std::string& name, Object* value);
};

} // namespace r7rs
