#pragma once

#include "bytecode_instruction.h"
#include <memory>
#include <unordered_map>
#include <functional>

/**
 * @file instruction_table.h
 * @brief Registry and factory for bytecode instruction decoding.
 */

namespace zombie { namespace bytecode {

// ============================================================================
// InstructionTable - Modular registry for bytecode instructions
// ============================================================================

/**
 * @brief Factory function type for instruction decoding.
 *
 * The factory receives a pointer to the current bytecode instruction and
 * returns a heap-allocated Instruction instance.
 */
using InstructionFactory = std::function<std::unique_ptr<Instruction>(const uint8_t*)>;

/**
 * @brief Global registry of bytecode instructions.
 *
 * Stores a mapping from opcode -> factory and opcode -> name. Used by the VM
 * disassembler and decoding logic to construct instruction instances from
 * bytecode streams.
 */
class InstructionTable {
public:
    /**
     * @brief Singleton access.
     */
    static InstructionTable& instance() {
        static InstructionTable table;
        return table;
    }

    /**
     * @brief Register a new instruction type.
     *
     * @param opcode The bytecode opcode value.
     * @param factory Function that creates an Instruction from bytecode.
     * @param name Instruction name for debugging/disassembly.
     *
     * If an opcode is re-registered, the last registration wins.
     */
    void register_instruction(uint8_t opcode,
                              InstructionFactory factory,
                              const std::string& name) {
        factories_[opcode] = factory;
        names_[opcode] = name;
    }

    /**
     * @brief Decode bytecode and create instruction instance.
     *
     * @param bytecode Pointer to instruction start (opcode at bytecode[0]).
     * @return Newly constructed Instruction.
     * @throws std::runtime_error if opcode is unknown.
     */
    std::unique_ptr<Instruction> decode(const uint8_t* bytecode) const {
        uint8_t opcode = bytecode[0];
        auto it = factories_.find(opcode);
        if (it == factories_.end()) {
            throw std::runtime_error("Unknown opcode: " + std::to_string(opcode));
        }
        return it->second(bytecode);
    }

    /**
     * @brief Get instruction name by opcode.
     *
     * @param opcode Opcode to look up.
     * @return Instruction name or \"UNKNOWN\" if not registered.
     */
    std::string name(uint8_t opcode) const {
        auto it = names_.find(opcode);
        return it != names_.end() ? it->second : "UNKNOWN";
    }

    /**
     * @brief Check if opcode is registered.
     *
     * @param opcode Opcode to test.
     * @return true if opcode has a factory registered.
     */
    bool has_opcode(uint8_t opcode) const {
        return factories_.find(opcode) != factories_.end();
    }

    /**
     * @brief Get all registered opcodes (for debugging/introspection).
     *
     * @return Vector of opcode values.
     */
    std::vector<uint8_t> opcodes() const {
        std::vector<uint8_t> result;
        for (const auto& pair : factories_) {
            result.push_back(pair.first);
        }
        return result;
    }

private:
    /**
     * @brief Private constructor for singleton.
     */
    InstructionTable() = default;

    std::unordered_map<uint8_t, InstructionFactory> factories_;
    std::unordered_map<uint8_t, std::string> names_;
};

// ============================================================================
// Helper Macro for registering instructions
// ============================================================================

/**
 * @brief Register an instruction class at static initialization time.
 *
 * Example:
 * @code
 * REGISTER_INSTRUCTION(0x01, LoadConstInsn, "LOAD_CONST");
 * @endcode
 */
#define REGISTER_INSTRUCTION(opcode_val, cls, insn_name) \
    namespace { \
        struct cls##Registrar { \
            cls##Registrar() { \
                InstructionTable::instance().register_instruction( \
                    opcode_val, \
                    [](const uint8_t* bc) -> std::unique_ptr<Instruction> { \
                        return std::make_unique<cls>(bc); \
                    }, \
                    insn_name \
                ); \
            } \
        }; \
        static cls##Registrar cls##_registrar_inst; \
    }

} } // namespace zombie::bytecode
