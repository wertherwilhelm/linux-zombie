#pragma once

#include <cstdint>

/**
 * @file bytecode_opcodes.h
 * @brief Bytecode opcode definitions (shared between Compiler and VM)
 * @author chrislk@gmail.com
 * @date 2026-01
 *
 * This header contains only the opcode enum and size helpers.
 * It can be safely included by the compiler without pulling in VM implementation.
 * This breaks the circular dependency: Compiler → Opcodes ← VM
 */

namespace zombie { namespace bytecode {

// ============================================================================
// Core Opcode Definitions
// ============================================================================

enum Opcode : uint8_t {
    // Control flow
    OP_HALT           = 0x00,  // Stop execution
    OP_RETURN         = 0x01,  // Return from function

    // Stack manipulation
    OP_POP            = 0x02,  // Pop value from stack
    OP_DUP            = 0x03,  // Duplicate top of stack

    // Constants and variables
    OP_LOAD_CONST     = 0x10,  // Load constant from pool [u16 index]
    OP_LOAD_VAR       = 0x11,  // Load variable from environment [u16 name_idx]
    OP_STORE_VAR      = 0x12,  // Store variable to environment [u16 name_idx]
    OP_DEFINE_VAR     = 0x13,  // Define new variable [u16 name_idx]

    // Control flow
    OP_JUMP           = 0x20,  // Unconditional jump [i16 offset]
    OP_JUMP_IF_FALSE  = 0x21,  // Jump if top of stack is false [i16 offset]
    OP_JUMP_IF_TRUE   = 0x22,  // Jump if top of stack is true [i16 offset]

    // Function calls
    OP_CALL           = 0x30,  // Call function [u8 nargs]
    OP_TAIL_CALL      = 0x31,  // Tail call [u8 nargs]
    OP_MAKE_CLOSURE   = 0x32,  // Create Closure [u16 code_idx]
    OP_CALLCC         = 0x33,  // Call with current Continuation
    OP_DYNAMIC_WIND   = 0x34,  // Dynamic-wind (before thunk after)

    // Primitive operations
    OP_ADD            = 0x40,  // Add two numbers
    OP_SUB            = 0x41,  // Subtract
    OP_MUL            = 0x42,  // Multiply
    OP_DIV            = 0x43,  // Divide
    OP_EQ             = 0x44,  // Numeric equality
    OP_LT             = 0x45,  // Less than
    OP_GT             = 0x46,  // Greater than

    // List operations
    OP_CONS           = 0x50,  // Cons two values
    OP_CAR            = 0x51,  // Car of pair
    OP_CDR            = 0x52,  // Cdr of pair

    // Multiple values (R5RS values/call-with-values)
    OP_VALUES         = 0x60,  // Return multiple values [u8 count]
    OP_CALL_WITH_VALUES = 0x61,  // Call with values from producer

    // User-defined instructions
    OP_USER_START     = 0xF0   // Reserve 0xF0-0xFF for user extensions
};

// ============================================================================
// Opcode Size Helpers
// ============================================================================

/**
 * @brief Get the size of an instruction in bytes
 * @param opcode The opcode to check
 * @return Size in bytes (1 + operand bytes)
 */
inline size_t get_instruction_size(Opcode opcode) {
    switch (opcode) {
        // 1-byte instructions (no operands)
        case OP_HALT:
        case OP_RETURN:
        case OP_POP:
        case OP_DUP:
        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_EQ:
        case OP_LT:
        case OP_GT:
        case OP_CONS:
        case OP_CAR:
        case OP_CDR:
            return 1;

        // 2-byte instructions (1-byte operand)
        case OP_CALL:
        case OP_TAIL_CALL:
        case OP_VALUES:
            return 2;

        // 3-byte instructions (2-byte operand)
        case OP_LOAD_CONST:
        case OP_LOAD_VAR:
        case OP_STORE_VAR:
        case OP_DEFINE_VAR:
        case OP_JUMP:
        case OP_JUMP_IF_FALSE:
        case OP_JUMP_IF_TRUE:
        case OP_MAKE_CLOSURE:
            return 3;

        default:
            return 1;  // Unknown opcode, assume 1 byte
    }
}

} } // namespace zombie::bytecode
