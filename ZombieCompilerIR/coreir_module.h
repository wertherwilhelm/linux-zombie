#pragma once

#include "coreir.h"
#include "coreir_verifier.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace coreir {

// ============================================================================
// CoreIR Module - Container for multiple functions
// ============================================================================

class Module {
private:
    std::string module_name;
    std::vector<CoreIRFunction> functions;
    std::map<std::string, size_t> symbol_table;  // name -> index in functions

public:
    // Constructors
    Module() : module_name("anonymous") {}
    explicit Module(const std::string& name) : module_name(name) {}

    // Module metadata
    const std::string& name() const { return module_name; }
    void set_name(const std::string& n) { module_name = n; }

    // Function management
    void add_function(CoreIRFunction&& f);
    void add_function(const std::string& name,
                     std::vector<std::string> params,
                     std::unique_ptr<Stmt> body);

    // Function lookup
    CoreIRFunction* get_function(const std::string& name);
    const CoreIRFunction* get_function(const std::string& name) const;

    bool has_function(const std::string& name) const {
        return symbol_table.find(name) != symbol_table.end();
    }

    // Access all functions
    const std::vector<CoreIRFunction>& get_functions() const { return functions; }
    size_t function_count() const { return functions.size(); }

    // Verification
    bool verify(std::string& errors_out) const;
    bool verify() const;  // Returns true/false without error details

    // Utilities
    void clear();
    std::vector<std::string> function_names() const;
};

} // namespace coreir
