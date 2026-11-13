#pragma once

#include "bytecode_opcodes.h"
#include "bytecode_instruction.h"
#include "instruction_table.h"
#include "i_evaluator.h"
#include "Environment.h"
#include "procedure.h"
#include "closure.h"
#include "cons_cell.h"
#include "continuation.h"
#include "numeric_runtime.h"

/**
 * @file core_instructions.h
 * @brief Core bytecode instruction implementations and registration.
 *
 * Defines the built-in instruction classes and registers them with the
 * instruction table. Some execute() implementations are inline and require
 * `bytecode_vm.h` to be included (guarded by BYTECODE_VM_H_INCLUDED).
 */

// Forward declarations
namespace zombie { namespace bytecode {
    class BytecodeVM;
    struct BytecodeFrame;
} } // namespace zombie::bytecode

namespace zombie { namespace bytecode {

// Bring types from zombie::runtime into this namespace
using runtime::Object;
using runtime::Environment;
using runtime::IEvaluator;
using runtime::Procedure;
using runtime::Closure;
using runtime::Continuation;
using runtime::ConsCell;
using runtime::cons;
using runtime::car;
using runtime::cdr;
using runtime::nil;
using runtime::t;
using runtime::false_obj;

// Note: Opcode enum is defined in bytecode_opcodes.h

// ============================================================================
// HALT - Stop execution
// ============================================================================

/**
 * @brief Halt execution of the current bytecode stream.
 */
class HaltInsn : public Instruction {
public:
    explicit HaltInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& /*ctx*/) override {
        return false;  // Stop execution
    }

    std::string name() const override { return "HALT"; }
    uint8_t opcode() const override { return OP_HALT; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "HALT";
    }
};

// ============================================================================
// RETURN - Return from function
// ============================================================================

/**
 * @brief Return from the current function/frame.
 */
class ReturnInsn : public Instruction {
public:
    explicit ReturnInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& /*ctx*/) override {
        // For now, RETURN just halts (until we implement call frames)
        return false;
    }

    std::string name() const override { return "RETURN"; }
    uint8_t opcode() const override { return OP_RETURN; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "RETURN";
    }
};

// ============================================================================
// POP - Pop value from stack
// ============================================================================

/**
 * @brief Pop and discard the top value from the stack.
 */
class PopInsn : public Instruction {
public:
    explicit PopInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override {
        ctx.pop();
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "POP"; }
    uint8_t opcode() const override { return OP_POP; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "POP";
    }
};

// ============================================================================
// DUP - Duplicate top of stack
// ============================================================================

/**
 * @brief Duplicate the top value on the stack.
 */
class DupInsn : public Instruction {
public:
    explicit DupInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override {
        Object* val = ctx.peek();
        ctx.push(val);
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "DUP"; }
    uint8_t opcode() const override { return OP_DUP; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "DUP";
    }
};

// ============================================================================
// LOAD_CONST - Load constant from pool
// ============================================================================

/**
 * @brief Push a constant pool entry onto the stack.
 */
class LoadConstInsn : public Instruction {
    uint16_t index_;

public:
    explicit LoadConstInsn(const uint8_t* bc) {
        index_ = read_u16(bc, 1);
    }

    bool execute(ExecutionContext& ctx) override {
        if (index_ >= ctx.constant_pool.size()) {
            throw std::runtime_error("LOAD_CONST: constant index out of bounds");
        }
        ctx.push(ctx.constant_pool[index_]);
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "LOAD_CONST"; }
    uint8_t opcode() const override { return OP_LOAD_CONST; }
    size_t size() const override { return 3; }  // opcode + u16 index

    std::string disassemble(size_t /*ip*/) const override {
        return "LOAD_CONST " + std::to_string(index_);
    }
};

// ============================================================================
// LOAD_VAR - Load variable from environment
// ============================================================================

/**
 * @brief Load a variable value from the current environment.
 */
class LoadVarInsn : public Instruction {
    uint16_t name_idx_;

public:
    explicit LoadVarInsn(const uint8_t* bc) {
        name_idx_ = read_u16(bc, 1);
    }

    bool execute(ExecutionContext& ctx) override {
        if (name_idx_ >= ctx.constant_pool.size()) {
            throw std::runtime_error("LOAD_VAR: name index out of bounds");
        }

        Object* name = ctx.constant_pool[name_idx_];
        Object* value = ctx.env->lookup(name);

        if (!value) {
            throw std::runtime_error("LOAD_VAR: undefined variable");
        }

        ctx.push(value);
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "LOAD_VAR"; }
    uint8_t opcode() const override { return OP_LOAD_VAR; }
    size_t size() const override { return 3; }

    std::string disassemble(size_t /*ip*/) const override {
        return "LOAD_VAR " + std::to_string(name_idx_);
    }
};

// ============================================================================
// STORE_VAR - Store variable to environment
// ============================================================================

/**
 * @brief Store a value into the current environment.
 *
 * Used for both define and set! semantics in bytecode.
 */
class StoreVarInsn : public Instruction {
    uint16_t name_idx_;

public:
    explicit StoreVarInsn(const uint8_t* bc) {
        name_idx_ = read_u16(bc, 1);
    }

    bool execute(ExecutionContext& ctx) override {
        if (name_idx_ >= ctx.constant_pool.size()) {
            throw std::runtime_error("STORE_VAR: name index out of bounds");
        }

        Object* name = ctx.constant_pool[name_idx_];
        Object* value = ctx.pop();

        // Try to update existing binding first (for set! semantics)
        // If that fails (variable doesn't exist), create new binding (for define semantics)
        // This is necessary because both define and set! use the same STORE_VAR opcode

        // Note: We can't use lookup() to check existence because it throws on failure
        // Instead, try set() first and fall back to define() if it fails
        try {
            ctx.env->set(name, value);
        } catch (...) {
            // Variable doesn't exist - create it with define()
            ctx.env->define(name, value);
        }

        ctx.push(value);  // Push value back (both define and set! return value)
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "STORE_VAR"; }
    uint8_t opcode() const override { return OP_STORE_VAR; }
    size_t size() const override { return 3; }

    std::string disassemble(size_t /*ip*/) const override {
        return "STORE_VAR " + std::to_string(name_idx_);
    }
};

// ============================================================================
// JUMP - Unconditional jump
// ============================================================================

/**
 * @brief Unconditional relative jump.
 */
class JumpInsn : public Instruction {
    int16_t offset_;

public:
    explicit JumpInsn(const uint8_t* bc) {
        // Read as signed 16-bit offset
        uint16_t unsigned_offset = read_u16(bc, 1);
        offset_ = static_cast<int16_t>(unsigned_offset);
    }

    bool execute(ExecutionContext& ctx) override {
        ctx.ip = static_cast<size_t>(static_cast<int64_t>(ctx.ip) + offset_);
        return true;
    }

    std::string name() const override { return "JUMP"; }
    uint8_t opcode() const override { return OP_JUMP; }
    size_t size() const override { return 3; }

    std::string disassemble(size_t ip) const override {
        int64_t target = static_cast<int64_t>(ip) + offset_;
        return "JUMP " + std::to_string(offset_) + " (to " + std::to_string(target) + ")";
    }
};

// ============================================================================
// JUMP_IF_FALSE - Jump if top of stack is false
// ============================================================================

/**
 * @brief Conditional jump when top of stack is #f.
 */
class JumpIfFalseInsn : public Instruction {
    int16_t offset_;

public:
    explicit JumpIfFalseInsn(const uint8_t* bc) {
        uint16_t unsigned_offset = read_u16(bc, 1);
        offset_ = static_cast<int16_t>(unsigned_offset);
    }

    bool execute(ExecutionContext& ctx) override {
        Object* condition = ctx.pop();

        // R5RS: only #f is false, everything else is true (including '(), 0, etc.)
        if (condition == false_obj) {
            size_t new_ip = static_cast<size_t>(static_cast<int64_t>(ctx.ip) + offset_);
            ctx.ip = new_ip;
        } else {
            ctx.ip += size();
        }
        return true;
    }

    std::string name() const override { return "JUMP_IF_FALSE"; }
    uint8_t opcode() const override { return OP_JUMP_IF_FALSE; }
    size_t size() const override { return 3; }

    std::string disassemble(size_t ip) const override {
        int64_t target = static_cast<int64_t>(ip) + offset_;
        return "JUMP_IF_FALSE " + std::to_string(offset_) + " (to " + std::to_string(target) + ")";
    }
};

// ============================================================================
// CALL - Call function with N arguments
// ============================================================================

/**
 * @brief Call a Procedure with N arguments.
 */
class CallInsn : public Instruction {
    uint8_t nargs_;

public:
    explicit CallInsn(const uint8_t* bc) {
        nargs_ = read_u8(bc, 1);
    }

    bool execute(ExecutionContext& ctx) override;  // Implemented in core_instructions.cpp

    std::string name() const override { return "CALL"; }
    uint8_t opcode() const override { return OP_CALL; }
    size_t size() const override { return 2; }

    std::string disassemble(size_t /*ip*/) const override {
        return "CALL " + std::to_string(nargs_);
    }
};

// ============================================================================
// TAIL_CALL - Tail call optimization (replace current frame)
// ============================================================================

/**
 * @brief Tail call with frame replacement when possible.
 */
class TailCallInsn : public Instruction {
    uint8_t nargs_;

public:
    explicit TailCallInsn(const uint8_t* bc) {
        nargs_ = read_u8(bc, 1);
    }

    bool execute(ExecutionContext& ctx) override;  // Implemented below

    std::string name() const override { return "TAIL_CALL"; }
    uint8_t opcode() const override { return OP_TAIL_CALL; }
    size_t size() const override { return 2; }

    std::string disassemble(size_t /*ip*/) const override {
        return "TAIL_CALL " + std::to_string(nargs_);
    }
};

// ============================================================================
// MAKE_CLOSURE - Create a Closure with captured environment
// ============================================================================

/**
 * @brief Create a Closure capturing the current environment.
 */
class MakeClosureInsn : public Instruction {
    uint16_t template_idx_;

public:
    explicit MakeClosureInsn(const uint8_t* bc) {
        template_idx_ = read_u16(bc, 1);
    }

    bool execute(ExecutionContext& ctx) override {
        // Get Closure template from constant pool
        if (template_idx_ >= ctx.constant_pool.size()) {
            throw std::runtime_error("MAKE_CLOSURE: constant pool index out of bounds");
        }

        Object* template_obj = ctx.constant_pool[template_idx_];
        Closure* tmpl = dynamic_cast<Closure*>(template_obj);

        if (!tmpl) {
            throw std::runtime_error("MAKE_CLOSURE: constant must be a Closure");
        }

        // Create a new Closure with the current environment captured
        // This captures the lexical environment at Closure creation time
        Closure* new_closure = gc_new<Closure>(tmpl->m_parms, tmpl->m_body, ctx.env);

        // Protect from GC while we copy bytecode
        gc_root<Closure> closure_guard(new_closure);

        // IMPORTANT: Copy the compiled bytecode from the template to the new Closure
        if (tmpl->has_bytecode()) {
            new_closure->set_bytecode(tmpl->bytecode(), tmpl->constants());
        }

        ctx.push(new_closure);
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "MAKE_CLOSURE"; }
    uint8_t opcode() const override { return OP_MAKE_CLOSURE; }
    size_t size() const override { return 3; }  // opcode + u16 index

    std::string disassemble(size_t /*ip*/) const override {
        return "MAKE_CLOSURE " + std::to_string(template_idx_);
    }
};

// ============================================================================
// CALLCC - Call with current Continuation (call/cc)
// ============================================================================

/**
 * @brief Capture the current continuation and call a procedure with it.
 */
class CallCCInsn : public Instruction {
public:
    explicit CallCCInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override;  // Implemented in core_instructions.cpp

    std::string name() const override { return "CALLCC"; }
    uint8_t opcode() const override { return OP_CALLCC; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "CALLCC";
    }
};

// ============================================================================
// DYNAMIC_WIND - Execute thunk with before/after guards
// ============================================================================
//
// Stack before: [before-thunk, thunk, after-thunk]
// Stack after: [result]
//
// Execution:
//   1. Pop after, thunk, before from stack
//   2. Call enter_wind(before, after) - calls before, pushes wind frame
//   3. Call thunk and capture result
//   4. Call exit_wind() - pops wind frame, calls after
//   5. Push result onto stack
//
// Note: If Continuation escapes during thunk execution, after thunk
// will be called by restore_continuation's rewind_to() logic

/**
 * @brief Execute dynamic-wind with before/thunk/after procedures.
 */
class DynamicWindInsn : public Instruction {
public:
    explicit DynamicWindInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override;  // Implementation at end of file

    std::string name() const override { return "DYNAMIC_WIND"; }
    uint8_t opcode() const override { return OP_DYNAMIC_WIND; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "DYNAMIC_WIND";
    }
};

// ============================================================================
// ADD - Add two numbers
// ============================================================================

/**
 * @brief Add two numeric values.
 */
class AddInsn : public Instruction {
public:
    explicit AddInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override {
        // Pop two operands
        Object* b = ctx.pop();
        Object* a = ctx.pop();

        // Check if both are numbers
        if (!numeric::is_number(a) || !numeric::is_number(b)) {
            throw std::runtime_error("ADD: operands must be numbers");
        }

        // Compute result using numeric runtime
        Object* result = numeric::add(a, b);
        ctx.push(result);
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "ADD"; }
    uint8_t opcode() const override { return OP_ADD; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "ADD";
    }
};

// ============================================================================
// SUB - Subtract two numbers
// ============================================================================

/**
 * @brief Subtract two numeric values.
 */
class SubInsn : public Instruction {
public:
    explicit SubInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override {
        Object* b = ctx.pop();
        Object* a = ctx.pop();

        if (!numeric::is_number(a) || !numeric::is_number(b)) {
            throw std::runtime_error("SUB: operands must be numbers");
        }

        Object* result = numeric::sub(a, b);
        ctx.push(result);
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "SUB"; }
    uint8_t opcode() const override { return OP_SUB; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "SUB";
    }
};

// ============================================================================
// MUL - Multiply two numbers
// ============================================================================

/**
 * @brief Multiply two numeric values.
 */
class MulInsn : public Instruction {
public:
    explicit MulInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override {
        Object* b = ctx.pop();
        Object* a = ctx.pop();

        if (!numeric::is_number(a) || !numeric::is_number(b)) {
            throw std::runtime_error("MUL: operands must be numbers");
        }

        Object* result = numeric::mul(a, b);
        ctx.push(result);
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "MUL"; }
    uint8_t opcode() const override { return OP_MUL; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "MUL";
    }
};

// ============================================================================
// DIV - Divide two numbers
// ============================================================================

/**
 * @brief Divide two numeric values.
 */
class DivInsn : public Instruction {
public:
    explicit DivInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override {
        Object* b = ctx.pop();
        Object* a = ctx.pop();

        if (!numeric::is_number(a) || !numeric::is_number(b)) {
            throw std::runtime_error("DIV: operands must be numbers");
        }

        // Check for division by zero (compare with 0)
        if (numeric::to_double(b) == 0.0) {
            throw std::runtime_error("DIV: division by zero");
        }

        Object* result = numeric::div(a, b);
        ctx.push(result);
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "DIV"; }
    uint8_t opcode() const override { return OP_DIV; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "DIV";
    }
};

// ============================================================================
// EQ - Numeric equality
// ============================================================================

/**
 * @brief Compare two numeric values for equality.
 */
class EqInsn : public Instruction {
public:
    explicit EqInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override {
        Object* b = ctx.pop();
        Object* a = ctx.pop();

        if (!numeric::is_number(a) || !numeric::is_number(b)) {
            throw std::runtime_error("EQ: operands must be numbers");
        }

        // Return #t or #f
        Object* result = numeric::equal(a, b) ? (Object*)t : (Object*)false_obj;
        ctx.push(result);
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "EQ"; }
    uint8_t opcode() const override { return OP_EQ; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "EQ";
    }
};

// ============================================================================
// LT - Less than
// ============================================================================

/**
 * @brief Compare two numeric values for less-than.
 */
class LtInsn : public Instruction {
public:
    explicit LtInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override {
        Object* b = ctx.pop();
        Object* a = ctx.pop();

        if (!numeric::is_number(a) || !numeric::is_number(b)) {
            throw std::runtime_error("LT: operands must be numbers");
        }

        int cmp_result = numeric::compare(a, b);
        Object* result = (cmp_result < 0) ? (Object*)t : (Object*)false_obj;

        ctx.push(result);
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "LT"; }
    uint8_t opcode() const override { return OP_LT; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "LT";
    }
};

// ============================================================================
// GT - Greater than
// ============================================================================

/**
 * @brief Compare two numeric values for greater-than.
 */
class GtInsn : public Instruction {
public:
    explicit GtInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override {
        Object* b = ctx.pop();
        Object* a = ctx.pop();

        if (!numeric::is_number(a) || !numeric::is_number(b)) {
            throw std::runtime_error("GT: operands must be numbers");
        }

        Object* result = (numeric::compare(a, b) > 0) ? (Object*)t : (Object*)false_obj;
        ctx.push(result);
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "GT"; }
    uint8_t opcode() const override { return OP_GT; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "GT";
    }
};

// ============================================================================
// CONS - Create a pair
// ============================================================================

/**
 * @brief Construct a cons cell from two values.
 */
class ConsInsn : public Instruction {
public:
    explicit ConsInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override {
        // Pop cdr and car (in that order - stack is LIFO)
        Object* cdr_val = ctx.pop();
        Object* car_val = ctx.pop();

        // Create cons cell
        Object* result = cons(car_val, cdr_val);
        ctx.push(result);
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "CONS"; }
    uint8_t opcode() const override { return OP_CONS; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "CONS";
    }
};

// ============================================================================
// CAR - Get first element of pair
// ============================================================================

/**
 * @brief Fetch the car of a pair.
 */
class CarInsn : public Instruction {
public:
    explicit CarInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override {
        Object* pair = ctx.pop();

        if (pair->pair() == 0) {
            throw std::runtime_error("CAR: not a pair");
        }

        Object* result = first(pair);
        ctx.push(result);
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "CAR"; }
    uint8_t opcode() const override { return OP_CAR; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "CAR";
    }
};

// ============================================================================
// CDR - Get rest of pair
// ============================================================================

/**
 * @brief Fetch the cdr of a pair.
 */
class CdrInsn : public Instruction {
public:
    explicit CdrInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override {
        Object* pair = ctx.pop();

        if (pair->pair() == 0) {
            throw std::runtime_error("CDR: not a pair");
        }

        Object* result = rest(pair);
        ctx.push(result);
        ctx.ip += size();
        return true;
    }

    std::string name() const override { return "CDR"; }
    uint8_t opcode() const override { return OP_CDR; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "CDR";
    }
};

// ============================================================================
// VALUES - Return multiple values (R5RS multi-value support)
// ============================================================================

/**
 * @brief Package multiple values for call-with-values.
 */
class ValuesInsn : public Instruction {
    uint8_t count_;

public:
    explicit ValuesInsn(const uint8_t* bc) {
        count_ = read_u8(bc, 1);
    }

    bool execute(ExecutionContext& ctx) override;  // Implemented below

    std::string name() const override { return "VALUES"; }
    uint8_t opcode() const override { return OP_VALUES; }
    size_t size() const override { return 2; }  // opcode + u8 count

    std::string disassemble(size_t /*ip*/) const override {
        return "VALUES " + std::to_string(count_);
    }
};

// ============================================================================
// CALL_WITH_VALUES - Call consumer with values from producer (R5RS)
// ============================================================================

/**
 * @brief Call producer then pass produced values to consumer.
 */
class CallWithValuesInsn : public Instruction {
public:
    explicit CallWithValuesInsn(const uint8_t* /*bc*/) {}

    bool execute(ExecutionContext& ctx) override;  // Implemented below

    std::string name() const override { return "CALL_WITH_VALUES"; }
    uint8_t opcode() const override { return OP_CALL_WITH_VALUES; }
    size_t size() const override { return 1; }

    std::string disassemble(size_t /*ip*/) const override {
        return "CALL_WITH_VALUES";
    }
};

// ============================================================================
// Instruction registration - happens at program startup
// ============================================================================

// Use REGISTER_INSTRUCTION Macro to auto-register all instructions
REGISTER_INSTRUCTION(OP_HALT, HaltInsn, "HALT")
REGISTER_INSTRUCTION(OP_RETURN, ReturnInsn, "RETURN")
REGISTER_INSTRUCTION(OP_POP, PopInsn, "POP")
REGISTER_INSTRUCTION(OP_DUP, DupInsn, "DUP")
REGISTER_INSTRUCTION(OP_LOAD_CONST, LoadConstInsn, "LOAD_CONST")
REGISTER_INSTRUCTION(OP_LOAD_VAR, LoadVarInsn, "LOAD_VAR")
REGISTER_INSTRUCTION(OP_STORE_VAR, StoreVarInsn, "STORE_VAR")
REGISTER_INSTRUCTION(OP_JUMP, JumpInsn, "JUMP")
REGISTER_INSTRUCTION(OP_JUMP_IF_FALSE, JumpIfFalseInsn, "JUMP_IF_FALSE")
REGISTER_INSTRUCTION(OP_CALL, CallInsn, "CALL")
REGISTER_INSTRUCTION(OP_TAIL_CALL, TailCallInsn, "TAIL_CALL")
REGISTER_INSTRUCTION(OP_MAKE_CLOSURE, MakeClosureInsn, "MAKE_CLOSURE")
REGISTER_INSTRUCTION(OP_CALLCC, CallCCInsn, "CALLCC")
REGISTER_INSTRUCTION(OP_DYNAMIC_WIND, DynamicWindInsn, "DYNAMIC_WIND")

// Arithmetic operations
REGISTER_INSTRUCTION(OP_ADD, AddInsn, "ADD")
REGISTER_INSTRUCTION(OP_SUB, SubInsn, "SUB")
REGISTER_INSTRUCTION(OP_MUL, MulInsn, "MUL")
REGISTER_INSTRUCTION(OP_DIV, DivInsn, "DIV")
REGISTER_INSTRUCTION(OP_EQ, EqInsn, "EQ")
REGISTER_INSTRUCTION(OP_LT, LtInsn, "LT")
REGISTER_INSTRUCTION(OP_GT, GtInsn, "GT")

// List operations
REGISTER_INSTRUCTION(OP_CONS, ConsInsn, "CONS")
REGISTER_INSTRUCTION(OP_CAR, CarInsn, "CAR")
REGISTER_INSTRUCTION(OP_CDR, CdrInsn, "CDR")

// Multiple values (R5RS)
REGISTER_INSTRUCTION(OP_VALUES, ValuesInsn, "VALUES")
REGISTER_INSTRUCTION(OP_CALL_WITH_VALUES, CallWithValuesInsn, "CALL_WITH_VALUES")

// ============================================================================
// Inline implementations (need full BytecodeVM definition)
//
// NOTE: These must be included AFTER BytecodeVM is fully defined to avoid
// circular dependency. The implementations reference BytecodeVM methods.
// ============================================================================

// Only include implementations when bytecode_vm.h has been included
#ifdef BYTECODE_VM_H_INCLUDED

// CallInsn::execute implementation
inline bool CallInsn::execute(ExecutionContext& ctx) {
    // Pop arguments in reverse order
    std::vector<Object*> args;
    args.reserve(nargs_);
    for (int i = 0; i < nargs_; i++) {
        args.insert(args.begin(), ctx.pop());
    }

    // Pop function
    Object* fn = ctx.pop();

    // Convert to Scheme list
    Object* arg_list = nil;
    for (auto it = args.rbegin(); it != args.rend(); ++it) {
        arg_list = cons(*it, arg_list);
    }

    // Call Procedure
    Procedure* proc = dynamic_cast<Procedure*>(fn);
    if (!proc) {
        throw std::runtime_error("CALL: not a Procedure");
    }

    // OPTIMIZATION: If in trampolined mode and calling a bytecode Closure,
    // push a new frame instead of recursing through C++ stack
    Closure* clos = dynamic_cast<Closure*>(proc);
    if (ctx.vm->has_frames() && clos && clos->has_bytecode()) {
        // Create call environment with parameters bound to arguments
        Environment* call_env = clos->m_env->extend(clos->m_parms, arg_list);

        // Create new execution frame for the Closure
        BytecodeFrame frame;
        frame.bytecode = clos->bytecode();  // Copy bytecode vector
        frame.constants = clos->constants();
        frame.ip = 0;
        frame.env = call_env;
        frame.type = BytecodeFrame::EVAL;
        frame.return_ip = 0;

        // Push frame onto frame stack (no C++ recursion!)
        ctx.vm->push_frame(std::move(frame));

        // Advance IP past CALL instruction
        ctx.ip += size();
        return true;
    }

    // Fallback: For primitives or non-trampolined mode, use regular apply
    Object* result = proc->apply(reinterpret_cast<IEvaluator*>(ctx.vm), arg_list, ctx.env);

    ctx.push(result);
    ctx.ip += size();
    return true;
}

// TailCallInsn::execute implementation
inline bool TailCallInsn::execute(ExecutionContext& ctx) {
    // Pop arguments in reverse order
    std::vector<Object*> args;
    args.reserve(nargs_);
    for (int i = 0; i < nargs_; i++) {
        args.insert(args.begin(), ctx.pop());
    }

    // Pop function
    Object* fn = ctx.pop();

    // Convert to Scheme list
    Object* arg_list = nil;
    for (auto it = args.rbegin(); it != args.rend(); ++it) {
        arg_list = cons(*it, arg_list);
    }

    // Call Procedure
    Procedure* proc = dynamic_cast<Procedure*>(fn);
    if (!proc) {
        throw std::runtime_error("TAIL_CALL: not a Procedure");
    }

    // TAIL CALL OPTIMIZATION: Instead of pushing a new frame,
    // replace the current frame with the tail call
    Closure* clos = dynamic_cast<Closure*>(proc);
    if (ctx.vm->has_frames() && clos && clos->has_bytecode()) {
        // Get reference to current frame (we'll replace it)
        BytecodeFrame& current_frame = ctx.vm->current_frame();

        // Create call environment with parameters bound to arguments
        Environment* call_env = clos->m_env->extend(clos->m_parms, arg_list);

        // Replace current frame's state with the tail call
        current_frame.bytecode = clos->bytecode();  // Copy bytecode vector
        current_frame.constants = clos->constants();
        current_frame.ip = 0;  // Start from beginning
        current_frame.env = call_env;
        current_frame.stack.clear();  // Clear stack for fresh execution

        // Reset IP to 0 (through ctx reference)
        ctx.ip = 0;

        return true;  // Continue execution
    }

    // Fallback: For primitives, just execute normally (no TCO benefit)
    Object* result = proc->apply(reinterpret_cast<IEvaluator*>(ctx.vm), arg_list, ctx.env);

    ctx.push(result);
    ctx.ip += size();
    return true;
}

// CallCCInsn::execute implementation
inline bool CallCCInsn::execute(ExecutionContext& ctx) {
    // Pop the Procedure
    Object* fn = ctx.pop();
    Procedure* proc = dynamic_cast<Procedure*>(fn);

    if (!proc) {
        throw std::runtime_error("call/cc: argument must be a Procedure");
    }

    // CRITICAL: Advance IP BEFORE capturing Continuation
    // Otherwise the saved Continuation will re-execute CALLCC when restored
    ctx.ip += size();

    // Capture current Continuation (now with IP pointing past CALLCC)
    Continuation* cont = ctx.vm->capture_continuation();

    // Call proc with Continuation as argument
    Object* arg_list = cons(static_cast<Object*>(cont), nil);

    // Call the Procedure with the Continuation
    Object* result = proc->apply(reinterpret_cast<IEvaluator*>(ctx.vm), arg_list, ctx.env);

    // Push result onto stack
    ctx.push(result);

    // IP already advanced above
    return true;
}

// DynamicWindInsn::execute implementation
inline bool DynamicWindInsn::execute(ExecutionContext& ctx) {
    // Pop three procedures from stack (in reverse order)
    Object* after = ctx.pop();
    Object* thunk = ctx.pop();
    Object* before = ctx.pop();

    // Validate arguments
    Procedure* before_proc = dynamic_cast<Procedure*>(before);
    Procedure* thunk_proc = dynamic_cast<Procedure*>(thunk);
    Procedure* after_proc = dynamic_cast<Procedure*>(after);

    if (!before_proc) {
        throw std::runtime_error("dynamic-wind: 'before' must be a Procedure");
    }
    if (!thunk_proc) {
        throw std::runtime_error("dynamic-wind: 'thunk' must be a Procedure");
    }
    if (!after_proc) {
        throw std::runtime_error("dynamic-wind: 'after' must be a Procedure");
    }

    // Cast to BytecodeVM to access wind stack methods
    BytecodeVM* vm = dynamic_cast<BytecodeVM*>(ctx.vm);
    if (!vm) {
        throw std::runtime_error("dynamic-wind: not a BytecodeVM");
    }

    // CRITICAL: Advance IP BEFORE executing thunk
    // If call/cc captures a Continuation inside the thunk, we don't want
    // the restored Continuation to re-execute DYNAMIC_WIND
    ctx.ip += size();

    // Enter the dynamic-wind context (calls before thunk, pushes wind frame)
    vm->enter_wind(before_proc, after_proc);

    // Execute the thunk and capture result
    Object* result = thunk_proc->apply(reinterpret_cast<IEvaluator*>(vm), nil, ctx.env);

    // Exit the dynamic-wind context (pops wind frame, calls after thunk)
    vm->exit_wind();

    // Push result onto stack
    ctx.push(result);

    // IP already advanced above
    return true;
}

// ValuesInsn::execute implementation
inline bool ValuesInsn::execute(ExecutionContext& ctx) {
    // Pop N values from stack (they're in reverse order)
    std::vector<Object*> values;
    values.reserve(count_);

    for (int i = 0; i < count_; i++) {
        values.insert(values.begin(), ctx.pop());
    }

    // For now, push values back onto stack in order
    // The frame's value_count field will track how many values are available
    for (auto val : values) {
        ctx.push(val);
    }

    // Update frame's value_count to indicate multiple values
    // This requires access to BytecodeVM
    BytecodeVM* vm = dynamic_cast<BytecodeVM*>(ctx.vm);
    if (vm && vm->has_frames()) {
        vm->current_frame().value_count = count_;
    }

    ctx.ip += size();
    return true;
}

// CallWithValuesInsn::execute implementation
inline bool CallWithValuesInsn::execute(ExecutionContext& ctx) {
    // Stack layout: [consumer] [producer]
    Object* consumer = ctx.pop();
    Object* producer = ctx.pop();

    // Validate arguments
    Procedure* producer_proc = dynamic_cast<Procedure*>(producer);
    Procedure* consumer_proc = dynamic_cast<Procedure*>(consumer);

    if (!producer_proc) {
        throw std::runtime_error("call-with-values: producer must be a Procedure");
    }
    if (!consumer_proc) {
        throw std::runtime_error("call-with-values: consumer must be a Procedure");
    }

    // Cast to BytecodeVM to access frame methods
    BytecodeVM* vm = dynamic_cast<BytecodeVM*>(ctx.vm);
    if (!vm) {
        throw std::runtime_error("call-with-values: not a BytecodeVM");
    }

    // Advance IP before calling producer (for proper Continuation capture)
    ctx.ip += size();

    // Call producer (0-arity thunk)
    Object* producer_result = producer_proc->apply(reinterpret_cast<IEvaluator*>(vm), nil, ctx.env);

    // Check how many values were produced
    int value_count = 1;  // Default to single value
    if (vm->has_frames()) {
        value_count = vm->current_frame().value_count;
    }

    // Build argument list from produced values
    // Values are on top of the stack (value_count of them)
    std::vector<Object*> args;
    args.reserve(value_count);

    for (int i = 0; i < value_count; i++) {
        args.insert(args.begin(), ctx.pop());
    }

    // Convert to Scheme list
    Object* arg_list = nil;
    for (auto it = args.rbegin(); it != args.rend(); ++it) {
        arg_list = cons(*it, arg_list);
    }

    // Call consumer with the values
    Object* result = consumer_proc->apply(reinterpret_cast<IEvaluator*>(vm), arg_list, ctx.env);

    // Push result and reset value_count
    ctx.push(result);
    if (vm->has_frames()) {
        vm->current_frame().value_count = 1;
    }

    // IP already advanced above
    return true;
}

#endif // BYTECODE_VM_H_INCLUDED

} } // namespace zombie::bytecode
