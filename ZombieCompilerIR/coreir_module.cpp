#include "pch.h"
#include "coreir_module.h"
#include <stdexcept>
#include <sstream>

namespace coreir {

// ============================================================================
// Function Management
// ============================================================================

void Module::add_function(CoreIRFunction&& f) {
    // Check for duplicate function name
    if (symbol_table.find(f.name) != symbol_table.end()) {
        throw std::runtime_error("Module '" + module_name +
                               "': Duplicate function name '" + f.name + "'");
    }

    // Add to symbol table
    symbol_table[f.name] = functions.size();

    // Move function into vector
    functions.push_back(std::move(f));
}

void Module::add_function(const std::string& name,
                         std::vector<std::string> params,
                         std::unique_ptr<Stmt> body) {
    // Build CoreIRFunction and move it in
    CoreIRFunction func(name, std::move(params), std::move(body));
    add_function(std::move(func));
}

// ============================================================================
// Function Lookup
// ============================================================================

CoreIRFunction* Module::get_function(const std::string& name) {
    auto it = symbol_table.find(name);
    if (it == symbol_table.end()) {
        return nullptr;
    }
    return &functions[it->second];
}

const CoreIRFunction* Module::get_function(const std::string& name) const {
    auto it = symbol_table.find(name);
    if (it == symbol_table.end()) {
        return nullptr;
    }
    return &functions[it->second];
}

// ============================================================================
// Verification
// ============================================================================

bool Module::verify(std::string& errors_out) const {
    CoreIRVerifier verifier;
    std::ostringstream all_errors;
    bool has_errors = false;

    for (const auto& func : functions) {
        verifier.clear();
        if (!verifier.verify(func)) {
            has_errors = true;
            all_errors << "Function '" << func.name << "':\n";
            all_errors << verifier.get_errors() << "\n\n";
        }
    }

    if (has_errors) {
        errors_out = all_errors.str();
        return false;
    }

    return true;
}

bool Module::verify() const {
    std::string errors;
    return verify(errors);
}

// ============================================================================
// Utilities
// ============================================================================

void Module::clear() {
    functions.clear();
    symbol_table.clear();
}

std::vector<std::string> Module::function_names() const {
    std::vector<std::string> names;
    names.reserve(functions.size());
    for (const auto& func : functions) {
        names.push_back(func.name);
    }
    return names;
}

} // namespace coreir
