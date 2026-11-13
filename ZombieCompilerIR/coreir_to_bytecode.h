#pragma once

#include "coreir.h"
#include "coreir_module.h"
#include <vector>
#include <map>
#include <string>
#include <cstdint>

namespace coreir {

// ============================================================================
// CompiledFunction - Bytecode + constants for a single function
// ============================================================================

struct CompiledFunction {
    std::string name;
    std::vector<std::string> params;
    std::vector<uint8_t> bytecode;
    std::vector<Object*> constants;

    CompiledFunction(const std::string& n, std::vector<std::string> p)
        : name(n), params(std::move(p)) {}
};

// ============================================================================
// CompiledModule - Collection of compiled functions
// ============================================================================

struct CompiledModule {
    std::string name;
    std::vector<CompiledFunction> functions;

    explicit CompiledModule(const std::string& n) : name(n) {}

    void add_function(CompiledFunction func) {
        functions.push_back(std::move(func));
    }

    const CompiledFunction* find_function(const std::string& name) const {
        for (const auto& func : functions) {
            if (func.name == name) {
                return &func;
            }
        }
        return nullptr;
    }
};

// ============================================================================
// CoreIRToBytecode - Translator from CoreIR to bytecode
// ============================================================================

class CoreIRToBytecode {
public:
    CoreIRToBytecode();

    // Compile a single function to bytecode
    CompiledFunction compile_function(const CoreIRFunction& func);

    // Compile an entire module to bytecode
    CompiledModule compile_module(const Module& mod);

private:
    // Current compilation state
    std::vector<uint8_t> bytecode_;
    std::vector<Object*> constants_;
    std::map<std::string, int> local_vars_;  // Local variable indices (on stack)
    int stack_depth_;

    // Reset compilation state
    void reset();

    // Constant pool management
    uint16_t add_constant(Object* obj);
    uint16_t add_string_constant(const std::string& str);
    uint16_t add_symbol_constant(const std::string& name);

    // Bytecode emission
    void emit_byte(uint8_t byte);
    void emit_u16(uint16_t value);
    void emit_i16(int16_t value);
    size_t emit_placeholder_jump();  // Returns offset of jump instruction
    void patch_jump(size_t offset, int16_t jump_distance);

    // Code generation for each construct
    void compile_atom(const Atom& atom);
    void compile_expr(const Expr& expr);
    void compile_stmt(const Stmt& stmt);

    // Primitive operation compilation
    void compile_primitive(const std::string& op, const std::vector<Atom>& args);

    // Helper: Get Primitive opcode
    uint8_t get_primitive_opcode(const std::string& op);

    // Helper: Allocate local variable slot
    int allocate_local(const std::string& name);

    // Helper: Get local variable index
    int get_local_index(const std::string& name);
};

} // namespace coreir
