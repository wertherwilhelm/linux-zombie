#include "pch.h"
#include "continuation_support.h"
#include "toy_vm.h"
#include "bytecode_vm.h"  // Need full BytecodeFrame definition for Continuation's shared_ptr<deque<BytecodeFrame>>
#include "continuation.h"
#include "cons_cell.h"
#include "procedure.h"
#include "closure.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include <iostream>

namespace toy {

// Bring in types from zombie::runtime
using zombie::runtime::Continuation;
using zombie::runtime::Procedure;
using zombie::runtime::Closure;
using zombie::runtime::first;
using zombie::runtime::cons;
using zombie::runtime::car;
using zombie::runtime::cdr;
using zombie::runtime::nil;

ContinuationSupport::ContinuationSupport(VM* vm)
    : m_vm(vm), m_continuation_invoked(false) {
}

// ============================================================================
// handle_callcc - Called when evaluating (call/cc func) or
//                 (call-with-current-Continuation func)
// ============================================================================

void ContinuationSupport::handle_callcc(Frame& frame, Object* args) {
    // (call/cc (lambda (k) ...))
    // We need to:
    // 1. Evaluate the function argument
    // 2. Capture the current Continuation
    // 3. Call the function with the Continuation as its argument

    Object* func_expr = first(args);

    // Change frame type to EVAL_CALLCC so we know to capture Continuation
    // after evaluating the function
    frame.type = EVAL_CALLCC;

    if (m_vm->debug_mode) {
        std::cout << "  -> CALL/CC: will capture Continuation!" << std::endl;
        std::cout << "     Current stack depth: " << m_vm->stack.size() << std::endl;
    }

    // Push a frame to evaluate the function argument
    // When it returns, step_eval_callcc will be called
    m_vm->stack.push_back(Frame(EVAL_ATOM, func_expr, frame.env));
}

// ============================================================================
// try_invoke_continuation - Check if proc is a Continuation and invoke it
// ============================================================================

bool ContinuationSupport::try_invoke_continuation(Object* proc, Object* args) {
    // Check if it's a Continuation Object
    Continuation* cont = dynamic_cast<Continuation*>(proc);
    if (!cont) {
        return false;  // Not a Continuation
    }

    // Invoking a Continuation!
    // Get the value to return (first argument)
    Object* return_value = first(args);

    if (m_vm->debug_mode) {
        std::cout << "  -> Invoking Continuation with value: ";
        std::cout << return_value;
        std::cout << std::endl;
        std::cout << "     Restoring stack depth: ";
        std::cout << cont->saved_stack.size();
        std::cout << std::endl;
    }

    // Restore the Continuation
    restore_continuation(cont, return_value);

    return true;  // Successfully invoked
}

// ============================================================================
// capture_continuation - Create a Continuation Object from current VM state
// ============================================================================

Object* ContinuationSupport::capture_continuation(Environment* env) {
    // Capture the stack BELOW the current frame
    // (The current frame is the EVAL_CALLCC frame which will be replaced)
    std::vector<Frame> saved_stack;

    // Copy all frames except the topmost (current EVAL_CALLCC frame)
    for (size_t i = 0; i < m_vm->stack.size() - 1; i++) {
        saved_stack.push_back(m_vm->stack[i]);
    }

    if (m_vm->debug_mode) {
        std::cout << "     Continuation created with stack depth: ";
        std::cout << saved_stack.size();
        std::cout << ", wind depth: " << m_wind_stack.size() << std::endl;
    }

    // Create and return the Continuation Object (with dynamic-wind stack)
    return gc_new<Continuation>(saved_stack, env, m_wind_stack);
}

// ============================================================================
// restore_continuation - Restore VM state from Continuation Object
// ============================================================================

void ContinuationSupport::restore_continuation(Object* cont_obj, Object* value) {
    Continuation* cont = dynamic_cast<Continuation*>(cont_obj);
    if (!cont) {
        throw std::runtime_error("restore_continuation: not a Continuation Object");
    }

    if (m_vm->debug_mode) {
        std::cout << "     Restoring Continuation..." << std::endl;
        std::cout << "     Current: stack=" << m_vm->stack.size()
                  << ", wind=" << m_wind_stack.size() << std::endl;
        std::cout << "     Target:  stack=" << cont->saved_stack.size()
                  << ", wind=" << cont->saved_wind_stack.size() << std::endl;
    }

    // Execute dynamic-wind before/after thunks as we jump to the target Continuation
    // This ensures proper R5RS semantics for dynamic-wind with call/cc
    rewind_to(cont->saved_wind_stack);

    // Replace the entire VM stack with the saved stack
    m_vm->stack = cont->saved_stack;

    // Put the return value in the accumulator
    m_vm->accumulator = value;

    // Set flag so restored frames know a Continuation was invoked
    // This is important for EVAL_ARGS and EVAL_BEGIN to handle escape properly
    m_continuation_invoked = true;

    if (m_vm->debug_mode) {
        std::cout << "     Continuation restored, stack depth now: ";
        std::cout << m_vm->stack.size() << std::endl;
    }
}

// ============================================================================
// Dynamic-wind support
// ============================================================================

void ContinuationSupport::enter_wind(Object* before_thunk, Object* after_thunk) {
    // Call the before thunk
    call_thunk(before_thunk);

    // Push the wind frame onto the stack
    m_wind_stack.push_back(WindFrame(before_thunk, after_thunk));

    if (m_vm->debug_mode) {
        std::cout << "  -> Entered dynamic-wind (depth=" << m_wind_stack.size() << ")" << std::endl;
    }
}

void ContinuationSupport::exit_wind() {
    if (m_wind_stack.empty()) {
        throw std::runtime_error("exit_wind: wind stack is empty");
    }

    // Get the top wind frame
    WindFrame frame = m_wind_stack.back();
    m_wind_stack.pop_back();

    if (m_vm->debug_mode) {
        std::cout << "  -> Exiting dynamic-wind (depth=" << m_wind_stack.size() << ")" << std::endl;
    }

    // Call the after thunk
    call_thunk(frame.after_thunk);
}

// ============================================================================
// Helper: Execute appropriate before/after thunks when jumping between continuations
// ============================================================================

void ContinuationSupport::rewind_to(const std::vector<WindFrame>& target_wind_stack) {
    // Find the common ancestor between current and target wind stacks
    size_t common_depth = 0;
    while (common_depth < m_wind_stack.size() &&
           common_depth < target_wind_stack.size() &&
           m_wind_stack[common_depth].before_thunk == target_wind_stack[common_depth].before_thunk &&
           m_wind_stack[common_depth].after_thunk == target_wind_stack[common_depth].after_thunk) {
        common_depth++;
    }

    if (m_vm->debug_mode) {
        std::cout << "  -> Rewinding from depth " << m_wind_stack.size()
                  << " to " << target_wind_stack.size()
                  << " (common=" << common_depth << ")" << std::endl;
    }

    // Unwind: Call after thunks for frames we're exiting (in reverse order)
    for (size_t i = m_wind_stack.size(); i > common_depth; --i) {
        if (m_vm->debug_mode) {
            std::cout << "     Calling after thunk (exiting depth " << (i-1) << ")" << std::endl;
        }
        call_thunk(m_wind_stack[i - 1].after_thunk);
    }

    // Rewind: Call before thunks for frames we're entering (in forward order)
    for (size_t i = common_depth; i < target_wind_stack.size(); ++i) {
        if (m_vm->debug_mode) {
            std::cout << "     Calling before thunk (entering depth " << i << ")" << std::endl;
        }
        call_thunk(target_wind_stack[i].before_thunk);
    }

    // Update the wind stack to match the target
    m_wind_stack = target_wind_stack;
}

// ============================================================================
// Helper: Call a thunk (0-argument Procedure)
// ============================================================================

void ContinuationSupport::call_thunk(Object* thunk) {
    if (!thunk) {
        throw std::runtime_error("call_thunk: null thunk");
    }

    // Try to call it as a Procedure
    Procedure* proc = dynamic_cast<Procedure*>(thunk);
    if (!proc) {
        throw std::runtime_error("call_thunk: thunk is not a Procedure");
    }

    // Apply the thunk with no arguments
    // Note: We need to use the VM's eval mechanism here
    // This is a bit tricky because we're in the middle of VM execution

    // Save state before calling thunk
    Object* saved_acc = m_vm->accumulator;
    size_t saved_stack_depth = m_vm->stack.size();

    if (m_vm->debug_mode) {
        std::cout << "  call_thunk: saving acc ptr=" << (void*)saved_acc
                  << " (type " << (saved_acc ? saved_acc->get_type() : -1) << ")"
                  << " stack_depth=" << saved_stack_depth << std::endl;
        // Check if it's actually a Closure
        Closure* clos = dynamic_cast<Closure*>(saved_acc);
        if (clos) {
            std::cout << "    (is Closure with body: " << clos->m_body << ")" << std::endl;
        } else {
            std::cout << "    (NOT a Closure!)" << std::endl;
        }
    }

    // Push a frame to call the thunk
    m_vm->stack.push_back(toy::Frame(toy::APPLY_PROC, nullptr, m_vm->global_env));
    m_vm->stack.back().proc = thunk;
    m_vm->stack.back().evaled_args = nil;

    // Run until we return to the original stack depth
    // This ensures the thunk runs to completion
    while (m_vm->stack.size() > saved_stack_depth) {
        m_vm->step();
    }

    if (m_vm->debug_mode) {
        std::cout << "  call_thunk: restoring acc=" << saved_acc
                  << " (current was " << m_vm->accumulator << ")" << std::endl;
    }

    // Restore the accumulator (we don't care about the thunk's return value)
    m_vm->accumulator = saved_acc;
}

} // namespace toy
