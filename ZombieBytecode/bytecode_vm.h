#pragma once

#define BYTECODE_VM_H_INCLUDED  // For core_instructions.h inline implementations

#include "i_evaluator.h"
#include "i_debugger.h"
#include "continuation.h"
#include "bytecode_instruction.h"
#include "instruction_table.h"
#include "Environment.h"
#include "wind_stack_utils.h"
#include <vector>
#include <deque>
#include <memory>
#include <iostream>

/**
 * @file bytecode_vm.h
 * @brief Stack-based bytecode VM with call/cc and dynamic-wind support.
 */

namespace zombie { namespace bytecode {

// Bring types from zombie::runtime into this namespace
using runtime::Object;
using runtime::Environment;
using runtime::IEvaluator;
using runtime::Continuation;
using runtime::IDebugger;

// ============================================================================
// BytecodeFrame - Execution frame for trampolined bytecode execution
// ============================================================================

/**
 * @brief Execution frame for trampolined bytecode evaluation.
 */
struct BytecodeFrame {
    // Bytecode being executed (owned by frame for lifetime safety)
    std::vector<uint8_t> bytecode;
    std::vector<Object*> constants;

    // Execution state
    size_t ip;                      // Instruction pointer
    std::vector<Object*> stack;     // Value stack (local to this frame)
    Environment* env;                // Lexical environment
    int value_count;                // Number of values from last operation (for multi-values, default=1)

    // Frame type
    enum Type {
        EVAL,       // Evaluating bytecode
        RETURN      // Returning a value
    };
    Type type;

    // Return address (for debugging/stack traces)
    size_t return_ip;

    // Helper to get bytecode pointer
    const uint8_t* bytecode_ptr() const { return bytecode.data(); }
    size_t bytecode_length() const { return bytecode.size(); }
};

// ============================================================================
// BytecodeVM - Stack-based bytecode virtual machine implementing IEvaluator
// ============================================================================

/**
 * @brief Bytecode virtual machine implementing IEvaluator.
 *
 * Supports continuations (call/cc) and R5RS dynamic-wind semantics.
 */
class BytecodeVM : public IEvaluator {
public:
    explicit BytecodeVM(Environment* env, bool debug = false);
    ~BytecodeVM() override = default;

    // ========================================================================
    // IEvaluator interface
    // ========================================================================

    Object* eval(Object* expr, Environment* env) override;
    Environment* get_environment() override { return global_env_; }

    // ========================================================================
    // Bytecode execution
    // ========================================================================

    // Execute compiled bytecode with constant pool
    Object* execute(const std::vector<uint8_t>& bytecode,
                    const std::vector<Object*>& constants,
                    Environment* env);

    // Execute bytecode from raw pointer (for embedded/compiled code)
    Object* execute_direct(const uint8_t* bytecode,
                          size_t length,
                          const std::vector<Object*>& constants,
                          Environment* env);

    // ========================================================================
    // VM state access (for ExecutionContext)
    // ========================================================================

    std::vector<Object*>& value_stack() { return value_stack_; }
    const std::vector<Object*>& value_stack() const { return value_stack_; }

    // ========================================================================
    // Debugging and diagnostics
    // ========================================================================

    void set_debug(bool debug) { debug_ = debug; }
    bool debug() const { return debug_; }

    void set_trace(bool trace) { trace_ = trace; }
    bool trace() const { return trace_; }

    // Pluggable debugger support
    void set_debugger(IDebugger* debugger) { debugger_ = debugger; }
    IDebugger* get_debugger() const { return debugger_; }

    // Disassemble bytecode
    void disassemble(const std::vector<uint8_t>& bytecode,
                    const std::vector<Object*>& constants,
                    std::ostream& out = std::cout);

    // Show current stack state
    void print_stack(std::ostream& out = std::cout) const;

    // GC support - trace references in VM state
    void trace_references(Tracer& tracer);

    // ========================================================================
    // Continuation support
    // ========================================================================

    /**
     * @brief Capture current Continuation (for call/cc).
     * @return Continuation Object capturing VM state.
     */
    Continuation* capture_continuation();

    /**
     * @brief Restore a Continuation (jump to saved state).
     * @param cont Continuation to restore.
     */
    void restore_continuation(Continuation* cont);

    // Push a new execution frame
    void push_frame(BytecodeFrame frame);

    // Check if we're in trampolined mode (frames active)
    bool has_frames() const { return !frames_.empty(); }

    // Get reference to current frame (for tail call optimization)
    BytecodeFrame& current_frame() {
        if (frames_.empty()) {
            throw std::runtime_error("current_frame: no frames");
        }
        return frames_.back();
    }

    // ========================================================================
    // Dynamic-wind support (R5RS compliance)
    // ========================================================================

    /**
     * @brief Enter a dynamic-wind context.
     * @param before_thunk Thunk to call before entering.
     * @param after_thunk Thunk to call after exiting.
     */
    void enter_wind(Object* before_thunk, Object* after_thunk);

    /**
     * @brief Exit a dynamic-wind context.
     */
    void exit_wind();

    /**
     * @brief Get current wind stack (for debugging/inspection).
     */
    const std::vector<toy::WindFrame>& get_wind_stack() const { return wind_stack_; }

private:
    Environment* global_env_;
    bool debug_;    // Debug mode (verbose output)
    bool trace_;    // Trace execution (print each instruction)
    IDebugger* debugger_;  // Pluggable debugger (nullptr = no debugging)

    // Runtime state (legacy - used when not in trampolined mode)
    std::vector<Object*> value_stack_;  // Value stack

    // Trampolined execution state
    std::deque<BytecodeFrame> frames_;  // Frame stack (for Continuation support - deque doesn't invalidate references)
    Object* accumulator_;                // Return value register

    // Dynamic-wind state
    std::vector<toy::WindFrame> wind_stack_;  // Stack of active dynamic-wind contexts

    // Execution loops
    Object* run(const uint8_t* bytecode,
                size_t length,
                const std::vector<Object*>& constants,
                Environment* env);

    // Trampolined execution (for Continuation support)
    Object* run_trampolined();

    // ========================================================================
    // Dynamic-wind helpers
    // ========================================================================

    /**
     * @brief Rewind wind stack when restoring Continuation.
     * @param target_wind_stack Target wind stack from Continuation.
     */
    void rewind_to(const std::vector<toy::WindFrame>& target_wind_stack);

    /**
     * @brief Call a 0-argument Procedure (thunk).
     * @param thunk Procedure to invoke.
     * @param env Environment for evaluation.
     */
    void call_thunk(Object* thunk, Environment* env);
};

} } // namespace zombie::bytecode

// Include inline implementations now that BytecodeVM is fully defined
#include "core_instructions.h"
