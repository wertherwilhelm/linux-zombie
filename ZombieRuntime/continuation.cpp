#include "pch.h"
#include "continuation.h"
#include "toy_vm.h"
#include "bytecode_vm.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include "Base.h"

namespace zombie { namespace runtime {

// Toy VM constructors (moved to .cpp to avoid incomplete type issues)
Continuation::Continuation(const std::vector<toy::Frame>& stack, Environment* env)
    : saved_stack(stack)
    , saved_env(env)
    , saved_wind_stack()
    , saved_accumulator(nullptr)
    , vm_type(TOY_VM) {
}

Continuation::Continuation(const std::vector<toy::Frame>& stack, Environment* env,
                          const std::vector<toy::WindFrame>& wind_stack)
    : saved_stack(stack)
    , saved_env(env)
    , saved_wind_stack(wind_stack)
    , saved_accumulator(nullptr)
    , vm_type(TOY_VM) {
}

// Bytecode VM constructor implementation (moved to .cpp to avoid incomplete type issues)
Continuation::Continuation(const std::deque<zombie::bytecode::BytecodeFrame>& frames,
                          Object* accumulator, Environment* env)
    : saved_env(env)
    , saved_wind_stack()
    , saved_bc_frames(std::make_shared<std::deque<zombie::bytecode::BytecodeFrame>>(frames))
    , saved_accumulator(accumulator)
    , vm_type(BYTECODE_VM) {
}

Continuation::Continuation(const std::deque<zombie::bytecode::BytecodeFrame>& frames,
                          Object* accumulator, Environment* env,
                          const std::vector<toy::WindFrame>& wind_stack)
    : saved_env(env)
    , saved_wind_stack(wind_stack)
    , saved_bc_frames(std::make_shared<std::deque<zombie::bytecode::BytecodeFrame>>(frames))
    , saved_accumulator(accumulator)
    , vm_type(BYTECODE_VM) {
}

// Apply - invoke a Continuation (jump back to saved state with value)
Object* Continuation::apply(IEvaluator* evaluator, Object* args, Environment* /*env*/) {
    // Get the value to return (first argument)
    Object* return_value = args->is_nil() ? nil : car(args);

    if (vm_type == TOY_VM) {
        // For toy VM, restore the saved state
        toy::VM* vm = dynamic_cast<toy::VM*>(evaluator);
        if (vm) {
            vm->stack = saved_stack;
            vm->accumulator = return_value;
            vm->global_env = saved_env;
            // The VM will continue execution from the saved state
            return return_value;
        }
        throw std::runtime_error("Continuation invoked with wrong evaluator type (expected toy::VM)");
    } else if (vm_type == BYTECODE_VM) {
        // For bytecode VM, throw special exception to unwind and restart execution
        throw ContinuationInvoked(this, return_value);
    }

    throw std::runtime_error("Unknown VM type for Continuation");
}

Object* Continuation::clone() const {
    if (vm_type == TOY_VM) {
        return gc_new<Continuation>(saved_stack, saved_env, saved_wind_stack);
    } else {
        return gc_new<Continuation>(*saved_bc_frames, saved_accumulator, saved_env, saved_wind_stack);
    }
}

void Continuation::trace_references(Tracer& tracer) {
    if (vm_type == TOY_VM) {
        // Trace all objects in the saved toy VM stack frames
        for (auto& frame : saved_stack) {
            tracer.visit_field(frame.expr);
            tracer.visit_field(frame.proc);
            tracer.visit_field(frame.args);
            tracer.visit_field(frame.evaled_args);
            // CRITICAL: trace frame environment (need to handle cast)
            if (frame.env) {
                Object* env_obj = (Object*)frame.env;
                tracer.visit_field(env_obj);
                frame.env = (Environment*)env_obj;  // Update after potential move
            }
        }
    } else if (vm_type == BYTECODE_VM) {
        // Trace all objects in the saved bytecode VM frames
        if (saved_bc_frames) {
            for (auto& frame : *saved_bc_frames) {
                // Trace constants (iterate by reference to update in-place)
                for (Object*& obj : frame.constants) {
                    tracer.visit_field(obj);
                }
                // Trace value stack (iterate by reference to update in-place)
                for (Object*& obj : frame.stack) {
                    tracer.visit_field(obj);
                }
                // Trace environment
                tracer.visit_field(frame.env);
            }
        }
        // Trace accumulator
        tracer.visit_field(saved_accumulator);
    }

    // Trace wind stack (shared by both toy VM and bytecode VM)
    for (auto& wind_frame : saved_wind_stack) {
        tracer.visit_field(wind_frame.before_thunk);
        tracer.visit_field(wind_frame.after_thunk);
    }

    // Trace saved environment (shared by both VM types)
    if (saved_env) {
        Object* env_obj = (Object*)saved_env;
        tracer.visit_field(env_obj);
        saved_env = (Environment*)env_obj;  // Update after potential move
    }
}

} } // namespace zombie::runtime
