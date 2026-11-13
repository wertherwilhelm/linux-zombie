#pragma once

#include "Object.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace zombie { namespace bytecode {

// Bring types from zombie::runtime into this namespace
using runtime::Object;
using runtime::Environment;

// Forward declarations
class BytecodeVM;

// ============================================================================
// ExecutionContext - All state available to instructions
// ============================================================================

struct ExecutionContext {
    BytecodeVM* vm;                       // The VM instance (for recursion)
    std::vector<Object*>& value_stack;    // Runtime value stack
    std::vector<Object*>& constant_pool;  // Constant pool (literals)
    Environment* env;                      // Current environment
    size_t& ip;                           // Instruction pointer (mutable!)

    // Stack manipulation helpers
    Object* pop() {
        if (value_stack.empty()) {
            throw std::runtime_error("Stack underflow");
        }
        Object* val = value_stack.back();
        value_stack.pop_back();
        return val;
    }

    void push(Object* val) {
        value_stack.push_back(val);
    }

    Object* peek(size_t offset = 0) const {
        if (offset >= value_stack.size()) {
            throw std::runtime_error("Stack underflow in peek");
        }
        return value_stack[value_stack.size() - 1 - offset];
    }

    size_t stack_size() const {
        return value_stack.size();
    }
};

// ============================================================================
// Instruction - Base class for all bytecode instructions
// ============================================================================

class Instruction {
public:
    virtual ~Instruction() = default;

    // Execute this instruction
    // Returns: true to continue execution, false to halt
    virtual bool execute(ExecutionContext& ctx) = 0;

    // Get opcode name for debugging/disassembly
    virtual std::string name() const = 0;

    // Get opcode byte
    virtual uint8_t opcode() const = 0;

    // How many bytes does this instruction occupy in bytecode?
    virtual size_t size() const = 0;

    // Disassemble instruction for debugging
    // ip = current instruction pointer
    virtual std::string disassemble(size_t ip) const = 0;

    // Helper: Read a 16-bit value from bytecode (big-endian)
    static uint16_t read_u16(const uint8_t* bytecode, size_t offset) {
        return (bytecode[offset] << 8) | bytecode[offset + 1];
    }

    // Helper: Read an 8-bit value
    static uint8_t read_u8(const uint8_t* bytecode, size_t offset) {
        return bytecode[offset];
    }
};

} } // namespace zombie::bytecode
