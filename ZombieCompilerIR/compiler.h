#pragma once

#include "Object.h"
#include "Environment.h"
#include "expander.h"
#include "../ZombieBytecode/bytecode_opcodes.h"
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <string>

namespace zombie { namespace bytecode {

// ============================================================================
// CompiledCode - Result of compilation
// ============================================================================

struct CompiledCode {
    std::vector<uint8_t> bytecode;      // Compiled bytecode
    std::vector<Object*> constants;     // Constant pool

    // Debug info (optional)
    std::vector<std::string> source_locations;
};

// ============================================================================
// Compiler - AST → Bytecode
// ============================================================================

class Compiler {
public:
    Compiler();

    // Main compilation entry point
    // Compiles a single Scheme expression to bytecode
    CompiledCode compile(Object* expr, Environment* env);

    // Compile multiple expressions (like begin)
    CompiledCode compile_sequence(Object* exprs, Environment* env);

private:
    // Compilation state
    std::vector<uint8_t> bytecode_;
    std::vector<Object*> constants_;
    Environment* env_;
    Expander* expander_;  // Macro expander (created during compilation)
    bool tail_context_;  // True if currently compiling in tail position

    // Label management for jumps
    struct Label {
        size_t position;   // Bytecode position
        bool bound;        // Has position been determined?
        std::vector<size_t> fixup_locations;  // Locations that need this label

        Label() : position(0), bound(false) {}
    };

    std::vector<Label> labels_;

    // ========================================================================
    // Core compilation methods
    // ========================================================================

    void compile_expr(Object* expr);

    void compile_atom(Object* atom);
    void compile_list(Object* list);

    // ========================================================================
    // Special form compilation
    // ========================================================================

    void compile_quote(Object* args);
    void compile_if(Object* args);
    void compile_lambda(Object* args);
    void compile_define(Object* args);
    void compile_set(Object* args);
    void compile_begin(Object* args);
    void compile_and(Object* args);
    void compile_or(Object* args);
    void compile_callcc(Object* args);
    void compile_dynamic_wind(Object* args);
    void compile_cond(Object* clauses);
    void compile_guard(Object* args);
    void compile_parameterize(Object* args);

    // Function application
    void compile_application(Object* fn, Object* args);

    // ========================================================================
    // Bytecode emission
    // ========================================================================

    void emit(uint8_t byte) {
        bytecode_.push_back(byte);
    }

    void emit_u16(uint16_t value) {
        emit(static_cast<uint8_t>((value >> 8) & 0xFF));
        emit(static_cast<uint8_t>(value & 0xFF));
    }

    void emit_i16(int16_t value) {
        emit_u16(static_cast<uint16_t>(value));
    }

    size_t current_position() const {
        return bytecode_.size();
    }

    // ========================================================================
    // Constant pool management
    // ========================================================================

    uint16_t add_constant(Object* obj) {
        // Check if constant already exists
        for (size_t i = 0; i < constants_.size(); i++) {
            if (constants_[i] == obj) {
                return static_cast<uint16_t>(i);
            }
        }

        // Add new constant
        constants_.push_back(obj);
        if (constants_.size() > 65535) {
            throw std::runtime_error("Constant pool overflow (max 65535 constants)");
        }
        return static_cast<uint16_t>(constants_.size() - 1);
    }

    // ========================================================================
    // Label management
    // ========================================================================

    size_t make_label() {
        labels_.push_back(Label());
        return labels_.size() - 1;
    }

    void bind_label(size_t label_id) {
        if (label_id >= labels_.size()) {
            throw std::runtime_error("Invalid label ID");
        }

        Label& label = labels_[label_id];
        label.position = current_position();
        label.bound = true;

        // Patch all jump instructions that reference this label
        // fixup_loc points to the offset bytes (opcode + 1), so subtract 1 to get opcode position
        for (size_t fixup_loc : label.fixup_locations) {
            int16_t offset = static_cast<int16_t>(label.position - (fixup_loc - 1));
            bytecode_[fixup_loc] = static_cast<uint8_t>((offset >> 8) & 0xFF);
            bytecode_[fixup_loc + 1] = static_cast<uint8_t>(offset & 0xFF);
        }
    }

    void emit_jump(uint8_t opcode, size_t label_id) {
        if (label_id >= labels_.size()) {
            throw std::runtime_error("Invalid label ID");
        }

        emit(opcode);
        size_t opcode_pos = current_position() - 1;  // Position of the opcode we just emitted

        Label& label = labels_[label_id];
        if (label.bound) {
            // Label already bound - compute offset from opcode position
            int16_t offset = static_cast<int16_t>(label.position - opcode_pos);
            emit_i16(offset);
        } else {
            // Label not bound yet - record fixup location (position of offset bytes)
            label.fixup_locations.push_back(current_position());
            emit_u16(0);  // Placeholder
        }
    }

    // ========================================================================
    // Utility methods
    // ========================================================================

    bool is_special_form(Object* expr);
    std::string get_symbol_name(Object* sym);
    size_t list_length(Object* list);
};

} } // namespace zombie::bytecode
