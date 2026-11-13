#include "pch.h"
#include "bytecode_vm.h"
#include "core_instructions.h"
#include "compiler.h"
#include "expander.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include "continuation.h"
#include "i_debugger.h"
#include <iomanip>
namespace zombie { namespace bytecode {

// ContinuationInvoked is defined in continuation.h (zombie::runtime); it must
// be the same type that Continuation::apply throws or the catches below will
// never match.
using zombie::runtime::ContinuationInvoked;

// ============================================================================
// Constructor / Destructor
// ============================================================================

BytecodeVM::BytecodeVM(Environment* env, bool debug)
    : global_env_(env)
    , debug_(debug)
    , trace_(false)
    , debugger_(nullptr)
    , accumulator_(nullptr) {
}

// ============================================================================
// IEvaluator interface - eval()
// ============================================================================

Object* BytecodeVM::eval(Object* expr, Environment* env) {
    // Record wind stack depth before evaluation
    // This is the target depth we should return to after eval completes
    size_t initial_wind_depth = wind_stack_.size();

    if (debug_) {
        std::cout << "[eval] Called with wind_stack depth: " << initial_wind_depth << std::endl;
    }

    // Expand macros and desugar syntax (let*, letrec, case, do, etc.)
    Expander expander(env);
    Object* expanded = expander.expand(expr);

    // Compile expanded expression to bytecode
    Compiler compiler;
    auto compiled = compiler.compile(expanded, env);

    if (debug_) {
        std::cout << "\n=== Compilation ===" << std::endl;
        disassemble(compiled.bytecode, compiled.constants);
        std::cout << "==================\n" << std::endl;
    }

    // Execute compiled bytecode
    // Catch Continuation invocation and handle non-local exit
    Object* result = nullptr;
    try {
        result = execute(compiled.bytecode, compiled.constants, env);
    } catch (const ContinuationInvoked& ci) {
        // Restore Continuation state
        restore_continuation(ci.cont);

        // The Continuation should now have restored frames
        // Push the return value and continue execution
        if (!frames_.empty()) {
            // Push return value onto the current frame's stack
            frames_.back().stack.push_back(ci.return_value);
            // Continue execution from restored state
            result = run_trampolined();
        } else {
            // No frames - just return the value
            result = ci.return_value;
        }
    }

    // CRITICAL: Clean up wind frames back to initial depth
    // When a Continuation is restored and executed, rewind_to() enters the
    // saved wind frames (calling before thunks). But when execution completes,
    // we don't return through the DYNAMIC_WIND instruction, so exit_wind()
    // is never called. We must manually clean up back to the initial depth.
    if (debug_ && wind_stack_.size() > initial_wind_depth) {
        std::cout << "[eval cleanup] Wind stack depth: " << wind_stack_.size()
                  << ", initial: " << initial_wind_depth
                  << ", cleaning up " << (wind_stack_.size() - initial_wind_depth) << " frames" << std::endl;
    }

    // IMPORTANT: Save accumulator before cleanup
    // Calling exit_wind() executes after thunks which may modify the accumulator
    Object* saved_accumulator = accumulator_;

    while (wind_stack_.size() > initial_wind_depth) {
        exit_wind();
    }

    // Restore accumulator after cleanup
    accumulator_ = saved_accumulator;

    return result;
}

// ============================================================================
// Bytecode execution
// ============================================================================

Object* BytecodeVM::execute(const std::vector<uint8_t>& bytecode,
                            const std::vector<Object*>& constants,
                            Environment* env) {
    return execute_direct(bytecode.data(), bytecode.size(), constants, env);
}

Object* BytecodeVM::execute_direct(const uint8_t* bytecode,
                                   size_t length,
                                   const std::vector<Object*>& constants,
                                   Environment* env) {
    if (debug_) {
        std::cout << "\n=== BytecodeVM::execute ===" << std::endl;
        std::cout << "Bytecode length: " << length << " bytes" << std::endl;
        std::cout << "Constant pool size: " << constants.size() << std::endl;
        std::cout << "Frame depth: " << frames_.size() << std::endl;
        std::cout << "========================\n" << std::endl;
    }

    // IMPORTANT: Always use trampolined frames for proper call/cc support
    // We MUST use trampolined execution even for nested calls so that
    // continuations can capture the full computation context.

    // Create a new frame for this execution
    BytecodeFrame new_frame;
    new_frame.bytecode.assign(bytecode, bytecode + length);  // Copy bytecode for lifetime safety
    new_frame.constants = constants;
    new_frame.ip = 0;
    new_frame.env = env;
    new_frame.value_count = 1;  // Default to single value (standard R5RS behavior)
    new_frame.type = BytecodeFrame::EVAL;
    new_frame.return_ip = 0;

    // Check if this is a nested call (Closure execution)
    bool is_nested = !frames_.empty();

    if (is_nested) {
        // Nested call - push frame and let the outer trampoline handle it
        // We need to execute this frame synchronously and return its result

        size_t initial_depth = frames_.size();
        frames_.push_back(std::move(new_frame));

        if (debug_) {
            std::cout << "Nested call - executing synchronously at depth " << frames_.size() << "\n";
        }

        // Execute frames until we return to the initial depth
        InstructionTable& insn_table = InstructionTable::instance();

        try {
            while (!frames_.empty() && frames_.size() > initial_depth) {
                size_t frame_idx = frames_.size() - 1;
                BytecodeFrame& current_frame = frames_[frame_idx];

                // Track why we exit the instruction loop
                bool frame_complete = false;
                bool new_frame_pushed = false;

                // Execute instructions in current frame
                while (current_frame.ip < current_frame.bytecode_length()) {
                    ExecutionContext ctx{
                        this,
                        current_frame.stack,
                        current_frame.constants,
                        current_frame.env,
                        current_frame.ip
                    };

                    auto insn = insn_table.decode(current_frame.bytecode_ptr() + current_frame.ip);

                    if (trace_) {
                        std::cout << std::setw(4) << std::setfill('0') << current_frame.ip << ": "
                                  << insn->disassemble(current_frame.ip) << std::endl;
                    }

                    bool should_continue = insn->execute(ctx);

                    if (!should_continue) {
                        // RETURN instruction - frame complete
                        frame_complete = true;
                        break;
                    }

                    // Check if new frame was pushed (nested call within this call)
                    if (frames_.size() > frame_idx + 1) {
                        // New frame pushed, need to execute it before continuing
                        new_frame_pushed = true;
                        break;
                    }
                }

                // Check if frame reached end of bytecode
                if (current_frame.ip >= current_frame.bytecode_length()) {
                    frame_complete = true;
                }

                // Pop frame if it's complete (not if we just pushed a new frame)
                if (frame_complete && !new_frame_pushed && frames_.size() > initial_depth) {
                    Object* result = current_frame.stack.empty() ? nil : current_frame.stack.back();
                    frames_.pop_back();

                    // If we're back to initial depth, return the result
                    if (frames_.size() == initial_depth) {
                        return result;
                    }

                    // Otherwise push result to parent frame
                    if (!frames_.empty()) {
                        frames_.back().stack.push_back(result);
                    }
                }
            }
        } catch (const ContinuationInvoked&) {
            // Continuation invoked during nested execution
            // Propagate it up - the top-level eval() will handle it
            // (frames_ left as-is: restore_continuation replaces it wholesale)
            throw;
        } catch (...) {
            // Any other unwind (GuardEscape, scheme_error, runtime errors)
            // must not leave this execution's frames behind: a later
            // re-entry (e.g. a guard's handler closure) would run on top of
            // them and corrupt the caller's stack
            while (frames_.size() > initial_depth) {
                frames_.pop_back();
            }
            throw;
        }

        // Shouldn't reach here, but return accumulator as fallback
        return accumulator_ ? accumulator_ : nil;
    }

    // Top-level execution - use full trampolined mode
    size_t entry_depth = frames_.size();
    frames_.push_back(std::move(new_frame));

    if (debug_) {
        std::cout << "Top-level execution - using run_trampolined()\n";
    }

    try {
        return run_trampolined();
    } catch (const ContinuationInvoked&) {
        throw;  // restore_continuation replaces frames_ wholesale
    } catch (...) {
        // Keep frames_ balanced on abnormal exits so the next eval on this
        // VM does not mistake leftovers for a nested execution
        while (frames_.size() > entry_depth) {
            frames_.pop_back();
        }
        throw;
    }
}

// ============================================================================
// Main execution loop
// ============================================================================

Object* BytecodeVM::run(const uint8_t* bytecode,
                        size_t length,
                        const std::vector<Object*>& constants,
                        Environment* env) {
    // IMPORTANT: Use a local value stack for each execution context
    // This allows recursion to work correctly - each recursive call
    // gets its own stack that doesn't interfere with the caller's stack
    std::vector<Object*> local_stack;

    // Instruction pointer
    size_t ip = 0;

    // Create execution context
    ExecutionContext ctx{
        this,
        local_stack,
        const_cast<std::vector<Object*>&>(constants),
        env,
        ip
    };

    // Get instruction table
    InstructionTable& insn_table = InstructionTable::instance();

    // Execution loop
    try {
        while (ip < length) {
            // Decode instruction
            auto insn = insn_table.decode(bytecode + ip);

            if (trace_) {
                std::cout << std::setw(4) << std::setfill('0') << ip << ": "
                          << insn->disassemble(ip) << std::endl;
            }

            // Execute instruction
            bool should_continue = insn->execute(ctx);

            if (!should_continue) {
                // Instruction requested halt
                break;
            }

            // Check for infinite loop (safety)
            if (ip >= length) {
                throw std::runtime_error("Instruction pointer out of bounds");
            }
        }
    } catch (const ContinuationInvoked&) {
        // Continuation invoked - propagate up to eval() for handling
        throw;
    }

    if (debug_) {
        std::cout << "\n=== Execution complete ===" << std::endl;
        std::cout << "Value stack (" << local_stack.size() << "):" << std::endl;
        for (size_t i = 0; i < local_stack.size(); i++) {
            std::cout << "  [" << i << "] ";
            if (local_stack[i]) local_stack[i]->print(std::cout);
            else std::cout << "NULL";
            std::cout << std::endl;
        }
        std::cout << "========================\n" << std::endl;
    }

    // Return top of stack (or nil if empty)
    return local_stack.empty() ? nil : local_stack.back();
}

// ============================================================================
// Debugging and diagnostics
// ============================================================================

void BytecodeVM::disassemble(const std::vector<uint8_t>& bytecode,
                             const std::vector<Object*>& constants,
                             std::ostream& out) {
    out << "=== Disassembly ===" << std::endl;
    out << "Constant pool (" << constants.size() << "):" << std::endl;
    for (size_t i = 0; i < constants.size(); i++) {
        out << "  [" << i << "] ";
        if (constants[i]) {
            constants[i]->print(out);
        } else {
            out << "nil";
        }
        out << std::endl;
    }

    out << "\nBytecode (" << bytecode.size() << " bytes):" << std::endl;

    InstructionTable& insn_table = InstructionTable::instance();
    size_t ip = 0;

    while (ip < bytecode.size()) {
        auto insn = insn_table.decode(bytecode.data() + ip);

        out << std::setw(4) << std::setfill('0') << ip << ": "
            << insn->disassemble(ip) << std::endl;

        ip += insn->size();
    }

    out << "===================" << std::endl;
}

void BytecodeVM::print_stack(std::ostream& out) const {
    out << "Value stack (" << value_stack_.size() << "):" << std::endl;
    if (value_stack_.empty()) {
        out << "  (empty)" << std::endl;
    } else {
        for (size_t i = 0; i < value_stack_.size(); i++) {
            out << "  [" << i << "] ";
            if (value_stack_[i]) {
                value_stack_[i]->print(out);
            } else {
                out << "nil";
            }
            out << std::endl;
        }
    }
}

// ============================================================================
// Continuation support
// ============================================================================

void BytecodeVM::push_frame(BytecodeFrame frame) {
    frames_.push_back(std::move(frame));
}

::Continuation* BytecodeVM::capture_continuation() {
    // Capture current frame stack, accumulator, and wind stack
    if (debug_) {
        std::cout << "[capture_continuation] Wind stack depth: " << wind_stack_.size() << std::endl;
    }
    return gc_new<::Continuation>(frames_, accumulator_, global_env_, wind_stack_);
}

void BytecodeVM::restore_continuation(::Continuation* cont) {
    if (!cont || cont->vm_type != ::Continuation::BYTECODE_VM) {
        throw std::runtime_error("Invalid Continuation for bytecode VM");
    }

    if (debug_) {
        std::cout << "\n=== Restoring Continuation ===" << std::endl;
        std::cout << "Current wind stack depth: " << wind_stack_.size() << std::endl;
        std::cout << "Target wind stack depth: " << cont->saved_wind_stack.size() << std::endl;
    }

    // Rewind wind stack (calls appropriate before/after thunks)
    rewind_to(cont->saved_wind_stack);

    if (debug_) {
        std::cout << "After rewind, wind stack depth: " << wind_stack_.size() << std::endl;
        std::cout << "============================\n" << std::endl;
    }

    // Restore frame stack and accumulator
    if (cont->saved_bc_frames) {
        frames_ = *(cont->saved_bc_frames);
    } else {
        frames_.clear();
    }
    accumulator_ = cont->saved_accumulator;
}

// ============================================================================
// Dynamic-wind support
// ============================================================================

void BytecodeVM::call_thunk(Object* thunk, Environment* env) {
    if (!thunk) {
        throw std::runtime_error("call_thunk: null thunk");
    }

    Procedure* proc = dynamic_cast<Procedure*>(thunk);
    if (!proc) {
        throw std::runtime_error("call_thunk: not a Procedure");
    }

    // Call with no arguments using Procedure::apply()
    // Note: The return value is discarded (thunks are called for side effects)
    proc->apply(this, nil, env);
}

void BytecodeVM::enter_wind(Object* before_thunk, Object* after_thunk) {
    if (debug_) {
        std::cout << "[enter_wind] Wind stack depth: " << wind_stack_.size()
                  << " -> " << (wind_stack_.size() + 1) << std::endl;
    }
    // Delegate to shared algorithm
    wind_stack_utils::enter_wind(wind_stack_, before_thunk, after_thunk,
        [this](Object* thunk) { call_thunk(thunk, global_env_); });
}

void BytecodeVM::exit_wind() {
    if (debug_) {
        std::cout << "[exit_wind] Wind stack depth: " << wind_stack_.size()
                  << " -> " << (wind_stack_.size() - 1) << std::endl;
    }
    // Delegate to shared algorithm
    wind_stack_utils::exit_wind(wind_stack_,
        [this](Object* thunk) { call_thunk(thunk, global_env_); });
}

void BytecodeVM::rewind_to(const std::vector<toy::WindFrame>& target_wind_stack) {
    // Delegate to shared algorithm
    wind_stack_utils::rewind_to(wind_stack_, target_wind_stack,
        [this](Object* thunk) { call_thunk(thunk, global_env_); });
}

// ============================================================================
// Trampolined execution loop
// ============================================================================

Object* BytecodeVM::run_trampolined() {
    InstructionTable& insn_table = InstructionTable::instance();

    try {
        while (!frames_.empty()) {
        // Get current frame index (safer than reference which can be invalidated)
        size_t frame_idx = frames_.size() - 1;

        if (debug_) {
            std::cout << "\\n=== Frame " << frame_idx << " ===" << std::endl;
            std::cout << "Type: " << (frames_[frame_idx].type == BytecodeFrame::EVAL ? "EVAL" : "RETURN") << std::endl;
            std::cout << "IP: " << frames_[frame_idx].ip << " / " << frames_[frame_idx].bytecode_length() << std::endl;
            std::cout << "Stack size: " << frames_[frame_idx].stack.size() << std::endl;
        }

        // If this is a RETURN frame, just pop it and continue
        if (frames_[frame_idx].type == BytecodeFrame::RETURN) {
            accumulator_ = frames_[frame_idx].stack.empty() ? nil : frames_[frame_idx].stack.back();
            frames_.pop_back();

            // Push result to parent frame's stack (if any)
            if (!frames_.empty()) {
                frames_.back().stack.push_back(accumulator_);
            }
            continue;
        }

        // Execute bytecode in current frame
        bool frame_complete = false;
        while (frames_[frame_idx].ip < frames_[frame_idx].bytecode_length() && !frame_complete) {
            // Create execution context for this frame
            // IMPORTANT: Access frame through index, not reference (reference can be invalidated)
            ExecutionContext ctx{
                this,
                frames_[frame_idx].stack,
                frames_[frame_idx].constants,
                frames_[frame_idx].env,
                frames_[frame_idx].ip
            };

            // Decode instruction
            auto insn = insn_table.decode(frames_[frame_idx].bytecode_ptr() + frames_[frame_idx].ip);

            if (trace_) {
                std::cout << std::setw(4) << std::setfill('0') << frames_[frame_idx].ip << ": "
                          << insn->disassemble(frames_[frame_idx].ip) << std::endl;
            }

            // Debug hook - check if we should break
            if (debugger_) {
                DebugContext debug_ctx;
                debug_ctx.instruction_pointer = static_cast<int>(frames_[frame_idx].ip);
                debug_ctx.current_env = frames_[frame_idx].env;
                debug_ctx.value_stack = &frames_[frame_idx].stack;
                debug_ctx.vm = this;
                debug_ctx.current_expr = nullptr;  // Would need source mapping for this
                debug_ctx.source_file = "<bytecode>";
                debug_ctx.source_line = 0;  // Would need source mapping for this

                // Build call stack from frames
                for (size_t i = 0; i < frames_.size(); i++) {
                    DebugFrame df;
                    df.function_name = "<bytecode>";
                    df.env = frames_[i].env;
                    df.instruction_pointer = static_cast<int>(frames_[i].ip);
                    df.current_expr = nullptr;
                    df.source_location = "<unknown>";
                    debug_ctx.call_stack.push_back(df);
                }

                // Call debugger hook - returns false if should pause
                if (!debugger_->on_step(debug_ctx)) {
                    // Debugger paused execution - wait for resume
                    while (debugger_->is_paused()) {
                        // Spin wait (debugger will set mode to non-paused when user continues)
                    }
                }
            }

            // Execute instruction
            bool should_continue = insn->execute(ctx);

            if (!should_continue) {
                // Instruction requested halt (RETURN instruction)
                frame_complete = true;
                break;
            }

            // Check for frame push (function call pushed new frame)
            if (frames_.size() > frame_idx + 1) {
                // New frame was pushed, switch to it
                break;
            }

            // Safety check
            if (frames_[frame_idx].ip >= frames_[frame_idx].bytecode_length()) {
                throw std::runtime_error("Instruction pointer out of bounds");
            }
        }

        // If frame is complete, pop it
        if (frame_complete || frames_[frame_idx].ip >= frames_[frame_idx].bytecode_length()) {
            accumulator_ = frames_[frame_idx].stack.empty() ? nil : frames_[frame_idx].stack.back();
            frames_.pop_back();

            // Push result to parent frame's stack (if any)
            if (!frames_.empty()) {
                frames_.back().stack.push_back(accumulator_);
            }
        }
        }  // end while
    } catch (const ContinuationInvoked& ci) {
        // Continuation invoked during trampolined execution
        // Restore the saved state and push return value
        restore_continuation(ci.cont);
        if (!frames_.empty()) {
            frames_.back().stack.push_back(ci.return_value);
            // Recursively continue trampolined execution
            return run_trampolined();
        }
        return ci.return_value;
    }

    if (debug_) {
        std::cout << "\\n=== Trampolined execution complete ===" << std::endl;
        std::cout << "Accumulator: ";
        if (accumulator_) accumulator_->print(std::cout);
        else std::cout << "nil";
        std::cout << "\\n================================\\n" << std::endl;
    }

    return accumulator_;
}

// ============================================================================
// GC support
// ============================================================================

void BytecodeVM::trace_references(Tracer& tracer) {
    // Trace global environment (need to handle cast)
    if (global_env_) {
        Object* env_obj = (Object*)global_env_;
        tracer.visit_field(env_obj);
        global_env_ = (Environment*)env_obj;  // Update after potential move
    }

    // Trace all objects on value stack (legacy) - iterate by reference
    for (Object*& obj : value_stack_) {
        tracer.visit_field(obj);
    }

    // Trace trampolined execution state
    tracer.visit_field(accumulator_);

    for (auto& frame : frames_) {
        // Trace constants in frame - iterate by reference to update in-place
        for (Object*& obj : frame.constants) {
            tracer.visit_field(obj);
        }
        // Trace value stack in frame - iterate by reference to update in-place
        for (Object*& obj : frame.stack) {
            tracer.visit_field(obj);
        }
        // Trace environment in frame
        tracer.visit_field(frame.env);
    }

    // Trace wind stack (dynamic-wind before/after thunks) - iterate by reference
    for (auto& wind_frame : wind_stack_) {
        tracer.visit_field(wind_frame.before_thunk);
        tracer.visit_field(wind_frame.after_thunk);
    }
}

} } // namespace zombie::bytecode
