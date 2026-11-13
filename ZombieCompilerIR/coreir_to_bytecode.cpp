#include "pch.h"
#include "coreir_to_bytecode.h"
#include "core_instructions.h"
#include "closure.h"
#include "Base.h"
#include "atom.h"
#include "constants.h"
#include "numeric_runtime.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include <stdexcept>
#include <sstream>

using namespace zombie::bytecode;

namespace coreir {

// ============================================================================
// Constructor
// ============================================================================

CoreIRToBytecode::CoreIRToBytecode()
    : stack_depth_(0) {
}

// ============================================================================
// Compilation Reset
// ============================================================================

void CoreIRToBytecode::reset() {
    bytecode_.clear();
    constants_.clear();
    local_vars_.clear();
    stack_depth_ = 0;
}

// ============================================================================
// Constant Pool Management
// ============================================================================

uint16_t CoreIRToBytecode::add_constant(Object* obj) {
    // Check if constant already exists
    for (size_t i = 0; i < constants_.size(); i++) {
        if (constants_[i] == obj) {
            return static_cast<uint16_t>(i);
        }
    }

    // Add new constant
    uint16_t index = static_cast<uint16_t>(constants_.size());
    constants_.push_back(obj);
    return index;
}

uint16_t CoreIRToBytecode::add_string_constant(const std::string& str) {
    // Create string Object and add to pool
    Object* str_obj = gc_new<a_string>(str);
    return add_constant(str_obj);
}

uint16_t CoreIRToBytecode::add_symbol_constant(const std::string& name) {
    // Create symbol Object and add to pool (symbols are used for variable names)
    Object* sym_obj = gc_new<a_symbol>(name);
    return add_constant(sym_obj);
}

// ============================================================================
// Bytecode Emission
// ============================================================================

void CoreIRToBytecode::emit_byte(uint8_t byte) {
    bytecode_.push_back(byte);
}

void CoreIRToBytecode::emit_u16(uint16_t value) {
    // Emit in big-endian order to match read_u16 in bytecode_instruction.h
    emit_byte(static_cast<uint8_t>((value >> 8) & 0xFF));  // High byte FIRST
    emit_byte(static_cast<uint8_t>(value & 0xFF));         // Low byte SECOND
}

void CoreIRToBytecode::emit_i16(int16_t value) {
    emit_u16(static_cast<uint16_t>(value));
}

size_t CoreIRToBytecode::emit_placeholder_jump() {
    size_t offset = bytecode_.size();
    emit_i16(0);  // Placeholder for jump distance
    return offset;
}

void CoreIRToBytecode::patch_jump(size_t offset, int16_t jump_distance) {
    // IMPORTANT: Use same byte order as emit_u16 (big-endian: high byte first)
    bytecode_[offset] = static_cast<uint8_t>((jump_distance >> 8) & 0xFF);  // High byte
    bytecode_[offset + 1] = static_cast<uint8_t>(jump_distance & 0xFF);     // Low byte
}

// ============================================================================
// Local Variable Management
// ============================================================================

int CoreIRToBytecode::allocate_local(const std::string& name) {
    int index = stack_depth_;
    local_vars_[name] = index;
    stack_depth_++;
    return index;
}

int CoreIRToBytecode::get_local_index(const std::string& name) {
    auto it = local_vars_.find(name);
    if (it != local_vars_.end()) {
        return it->second;
    }
    return -1;  // Not a local variable (must be global)
}

// ============================================================================
// Primitive Opcode Mapping
// ============================================================================

uint8_t CoreIRToBytecode::get_primitive_opcode(const std::string& op) {
    if (op == "+") return OP_ADD;
    if (op == "-") return OP_SUB;
    if (op == "*") return OP_MUL;
    if (op == "/") return OP_DIV;
    if (op == "=") return OP_EQ;
    if (op == "<") return OP_LT;
    if (op == ">") return OP_GT;
    if (op == "cons") return OP_CONS;
    if (op == "car") return OP_CAR;
    if (op == "cdr") return OP_CDR;

    // Primitives without bytecode opcodes (<=, >=, eq?, eqv?, equal?)
    // Return 0xFF to indicate "not a bytecode Primitive"
    // Caller should compile these as function calls instead
    return 0xFF;
}

// ============================================================================
// Compile Atom
// ============================================================================

void CoreIRToBytecode::compile_atom(const Atom& atom) {
    if (atom.isVar()) {
        const auto& var = atom.asVar();

        // Check if it's a local variable
        int local_idx = get_local_index(var.name);
        if (local_idx >= 0) {
            // Load from local slot
            // IMPORTANT: Use symbol, not string, so environment lookup works correctly
            uint16_t name_idx = add_symbol_constant(var.name);
            emit_byte(OP_LOAD_VAR);
            emit_u16(name_idx);
        } else {
            // Load from global environment
            // IMPORTANT: Use symbol, not string, so environment lookup works correctly
            uint16_t name_idx = add_symbol_constant(var.name);
            emit_byte(OP_LOAD_VAR);
            emit_u16(name_idx);
        }
    } else if (atom.isLit()) {
        const auto& lit = atom.asLit();
        uint16_t const_idx = add_constant(lit.value);
        emit_byte(OP_LOAD_CONST);
        emit_u16(const_idx);
    }
}

// ============================================================================
// Compile Primitive Operations
// ============================================================================

void CoreIRToBytecode::compile_primitive(const std::string& op,
                                         const std::vector<Atom>& args) {
    // Special case: unary minus (- x) should be compiled as (- 0 x)
    if (op == "-" && args.size() == 1) {
        // Push 0
        uint16_t zero_idx = add_constant(numeric::make_integer(0));
        emit_byte(OP_LOAD_CONST);
        emit_u16(zero_idx);

        // Push argument
        compile_atom(args[0]);

        // Emit SUB instruction
        emit_byte(OP_SUB);
        return;
    }

    // Check if this Primitive has a bytecode opcode
    uint8_t opcode = get_primitive_opcode(op);

    if (opcode == 0xFF) {
        // No bytecode opcode - compile as function call instead
        // Load the Primitive function from global environment
        uint16_t name_idx = add_symbol_constant(op);
        emit_byte(OP_LOAD_VAR);
        emit_u16(name_idx);

        // Load arguments onto stack
        for (const auto& arg : args) {
            compile_atom(arg);
        }

        // Emit CALL instruction
        emit_byte(OP_CALL);
        emit_byte(static_cast<uint8_t>(args.size()));
    } else {
        // Has bytecode opcode - compile as Primitive instruction
        // Load arguments onto stack
        for (const auto& arg : args) {
            compile_atom(arg);
        }

        // Emit Primitive instruction
        emit_byte(opcode);
    }
}

// ============================================================================
// Compile Expression
// ============================================================================

void CoreIRToBytecode::compile_expr(const Expr& expr) {
    if (expr.isPrim()) {
        const auto& prim = expr.asPrim();
        compile_primitive(prim.op, prim.args);
    }
    else if (expr.isCall()) {
        const auto& call = expr.asCall();

        // Load function onto stack
        compile_atom(call.func);

        // Load arguments onto stack
        for (const auto& arg : call.args) {
            compile_atom(arg);
        }

        // Emit call instruction
        uint8_t nargs = static_cast<uint8_t>(call.args.size());
        if (call.is_tail_call) {
            emit_byte(OP_TAIL_CALL);
        } else {
            emit_byte(OP_CALL);
        }
        emit_byte(nargs);
    }
    else if (expr.isGetGlobal()) {
        const auto& getg = expr.asGetGlobal();
        uint16_t name_idx = add_symbol_constant(getg.name);
        emit_byte(OP_LOAD_VAR);
        emit_u16(name_idx);
    }
    else if (expr.isSetGlobal()) {
        const auto& setg = expr.asSetGlobal();

        // Load value onto stack
        compile_atom(setg.value);

        // Store to global variable
        uint16_t name_idx = add_symbol_constant(setg.name);
        emit_byte(OP_STORE_VAR);
        emit_u16(name_idx);
    }
    else if (expr.isSetLocal()) {
        const auto& setl = expr.asSetLocal();

        // Load value onto stack
        compile_atom(setl.value);

        // Store to local variable (uses same instruction as SetGlobal for now)
        uint16_t name_idx = add_symbol_constant(setl.name);
        emit_byte(OP_STORE_VAR);
        emit_u16(name_idx);
    }
    else if (expr.isLambda()) {
        // Lambda compilation: recursively compile body, create Closure template
        const auto& lambda = expr.asLambda();

        // Create a Closure Object that will serve as template
        // Convert params to Scheme list
        Object* params_list = nil;
        for (auto it = lambda.params.rbegin(); it != lambda.params.rend(); ++it) {
            Object* param_sym = gc_new<a_symbol>(*it);
            params_list = cons(param_sym, params_list);
        }

        // Create Closure template (body will be set to bytecode)
        Closure* tmpl = gc_new<Closure>(params_list, nil, nullptr);

        // Save current compilation state
        auto saved_bytecode = bytecode_;
        auto saved_constants = constants_;
        auto saved_local_vars = local_vars_;
        int saved_stack_depth = stack_depth_;

        // Reset for compiling lambda body
        bytecode_.clear();
        constants_.clear();
        local_vars_.clear();
        stack_depth_ = 0;

        // Allocate parameter slots (parameters are passed on the stack)
        for (const auto& param : lambda.params) {
            allocate_local(param);
        }

        // Compile the lambda body (body is a Stmt, not Expr)
        compile_stmt(*lambda.body);

        // Note: compile_stmt for Return already emits OP_RETURN

        // Store compiled bytecode in the Closure template
        tmpl->set_bytecode(bytecode_, constants_);

        // Restore compilation state
        bytecode_ = saved_bytecode;
        constants_ = saved_constants;
        local_vars_ = saved_local_vars;
        stack_depth_ = saved_stack_depth;

        // Add Closure template to constant pool
        // DEBUG: Verify the Closure before adding
        if (!tmpl) {
            throw std::runtime_error("ERROR: Lambda compilation created null Object!");
        }
        if (tmpl->instance_of() != static_cast<int>(Base::atype::CLOSURE)) {
            std::stringstream ss;
            ss << "ERROR: Lambda compilation created non-Closure Object! instance_of() returned "
               << tmpl->instance_of() << " (expected " << static_cast<int>(Base::atype::CLOSURE) << " for CLOSURE)";
            throw std::runtime_error(ss.str());
        }
        if (!tmpl->has_bytecode()) {
            throw std::runtime_error("ERROR: Closure template has no bytecode!");
        }

        uint16_t idx = add_constant(tmpl);

        // DEBUG: Verify it was added correctly
        if (idx >= constants_.size() || constants_[idx] != tmpl) {
            throw std::runtime_error("ERROR: Closure not added to constant pool correctly!");
        }

        // Emit MAKE_CLOSURE instruction
        emit_byte(OP_MAKE_CLOSURE);
        emit_u16(idx);
    }
    else if (expr.isValues()) {
        const auto& vals = expr.asValues();

        // Load all values onto stack
        for (const auto& val : vals.values) {
            compile_atom(val);
        }

        // Emit VALUES instruction with count
        emit_byte(OP_VALUES);
        emit_byte(static_cast<uint8_t>(vals.values.size()));
    }
    else if (expr.isCallWithValues()) {
        const auto& cwv = expr.asCallWithValues();

        // Load producer and consumer onto stack
        compile_atom(cwv.producer);
        compile_atom(cwv.consumer);

        // Emit CALL_WITH_VALUES instruction
        emit_byte(OP_CALL_WITH_VALUES);
    }
}

// ============================================================================
// Compile Statement
// ============================================================================

void CoreIRToBytecode::compile_stmt(const Stmt& stmt) {
    if (stmt.isLet()) {
        const auto& let = stmt.asLet();

        // Compile the expression
        compile_expr(*let.expr);

        // Store to local variable in environment
        uint16_t name_idx = add_symbol_constant(let.name);
        emit_byte(OP_STORE_VAR);
        emit_u16(name_idx);

        // Track local variable slot
        allocate_local(let.name);

        // Compile Continuation
        compile_stmt(*let.next);

        // After Continuation, clean up local variable
        stack_depth_--;
        local_vars_.erase(let.name);
        // Note: Don't emit POP - STORE_VAR consumes the value from stack
    }
    else if (stmt.isIf()) {
        const auto& if_stmt = stmt.asIf();

        // Compile condition
        compile_atom(if_stmt.cond);

        // Emit conditional jump
        emit_byte(OP_JUMP_IF_FALSE);
        size_t false_jump_offset = emit_placeholder_jump();

        // Compile then branch
        compile_stmt(*if_stmt.then_branch);

        // Emit jump over else branch
        emit_byte(OP_JUMP);
        size_t end_jump_offset = emit_placeholder_jump();

        // Patch false jump to here (start of else branch)
        // IMPORTANT: Same fix as above - jump offset is calculated from instruction, not operand
        int16_t false_jump_distance = static_cast<int16_t>(bytecode_.size() - false_jump_offset + 1);
        patch_jump(false_jump_offset, false_jump_distance);

        // Compile else branch
        size_t else_start = bytecode_.size();
        compile_stmt(*if_stmt.else_branch);
        size_t else_end = bytecode_.size();

        // Patch end jump to here (after else branch)
        // IMPORTANT: end_jump_offset points to the 2-byte operand, but the VM calculates
        // the jump from the instruction itself (1 byte before the operand).
        // So we need: distance = bytecode_.size() - (end_jump_offset - 1)
        //           = bytecode_.size() - end_jump_offset + 1
        int16_t end_jump_distance = static_cast<int16_t>(bytecode_.size() - end_jump_offset + 1);

        patch_jump(end_jump_offset, end_jump_distance);
    }
    else if (stmt.isReturn()) {
        const auto& ret = stmt.asReturn();

        // Load return value onto stack
        compile_atom(ret.value);

        // Emit return instruction
        emit_byte(OP_RETURN);
    }
}

// ============================================================================
// Compile Function
// ============================================================================

CompiledFunction CoreIRToBytecode::compile_function(const CoreIRFunction& func) {
    // Reset compilation state
    reset();

    // Allocate parameter slots
    for (const auto& param : func.params) {
        allocate_local(param);
    }

    // Compile function body
    compile_stmt(*func.body);

    // Return compiled function
    CompiledFunction compiled(func.name, func.params);
    compiled.bytecode = std::move(bytecode_);
    compiled.constants = std::move(constants_);
    return compiled;
}

// ============================================================================
// Compile Module
// ============================================================================

CompiledModule CoreIRToBytecode::compile_module(const Module& mod) {
    CompiledModule compiled_mod(mod.name());

    // Compile each function in the module
    const auto& functions = mod.get_functions();
    for (const auto& func : functions) {
        CompiledFunction compiled_func = compile_function(func);
        compiled_mod.add_function(std::move(compiled_func));
    }

    return compiled_mod;
}

} // namespace coreir
