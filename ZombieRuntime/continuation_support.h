#pragma once

#include "Object.h"
#include "Environment.h"
#include "continuation.h"  // For WindFrame definition
#include <vector>

/**
 * @file continuation_support.h
 * @brief Continuation and dynamic-wind support for the toy VM.
 */

// Forward declarations
namespace toy {
    class VM;
    struct Frame;
}

namespace toy {

// WindFrame is now defined in Continuation.h to avoid circular dependency issues

// ============================================================================
// ContinuationSupport - Isolated module for first-class Continuation support
// ============================================================================
//
// This module encapsulates all Continuation-related logic, making it:
// - Optional: Can be disabled for simpler VM variant
// - Testable: Can be tested in isolation
// - Maintainable: All Continuation logic in one place
// - Extensible: Includes full dynamic-wind support with call/cc
//
// Usage:
//   VM creates a ContinuationSupport instance and delegates Continuation
//   operations to it. The module has access to VM's stack and state.
//
/**
 * @brief Continuation and dynamic-wind support for the toy VM.
 *
 * This module centralizes call/cc capture/restore and dynamic-wind behavior,
 * and provides a small API that the VM calls at well-defined points.
 *
 * call/cc flow (toy VM):
 * 1. Evaluate the argument Procedure.
 * 2. Capture the current Continuation (stack below the call/cc frame).
 * 3. Call the Procedure with the Continuation as its argument.
 *
 * dynamic-wind flow:
 * - enter_wind: run before thunk, push WindFrame.
 * - exit_wind: pop WindFrame, run after thunk.
 * - restore_continuation: rewinds wind stack via before/after thunks.
 */
class ContinuationSupport {
public:
    /**
     * @brief Construct Continuation support bound to a VM.
     * @param vm Owning VM (for stack access).
     */
    explicit ContinuationSupport(VM* vm);

    // -------------------------------------------------------------------------
    // Called from VM's special form dispatch
    // -------------------------------------------------------------------------

    /**
     * @brief Handle call/cc special form.
     *
     * (call/cc (lambda (k) ...))
     * Captures the current Continuation and calls the function with it.
     *
     * @param frame Current VM frame (mutated to EVAL_CALLCC).
     * @param args Arguments to call/cc.
     */
    void handle_callcc(Frame& frame, Object* args);

    // -------------------------------------------------------------------------
    // Called from VM's apply Procedure handler
    // -------------------------------------------------------------------------

    /**
     * @brief Try to invoke a Continuation Object.
     * @param proc Candidate Procedure.
     * @param args Arguments (first is return value).
     * @return True if proc was a Continuation and was invoked.
     */
    bool try_invoke_continuation(Object* proc, Object* args);

    // -------------------------------------------------------------------------
    // Continuation management
    // -------------------------------------------------------------------------

    /**
     * @brief Capture current VM stack as a Continuation Object.
     * @param env Environment to store with the Continuation.
     * @return Continuation Object.
     */
    Object* capture_continuation(Environment* env);

    /**
     * @brief Restore VM stack from Continuation and return the value.
     * @param cont Continuation Object.
     * @param value Value to return from the Continuation.
     */
    void restore_continuation(Object* cont, Object* value);

    // -------------------------------------------------------------------------
    // Flag management
    // -------------------------------------------------------------------------

    /**
     * @brief Check if a Continuation was just invoked.
     *
     * Used by EVAL_ARGS and EVAL_BEGIN to handle escape.
     */
    bool was_continuation_invoked() const { return m_continuation_invoked; }

    /**
     * @brief Clear the Continuation-invoked flag.
     */
    void clear_continuation_flag() { m_continuation_invoked = false; }

    // -------------------------------------------------------------------------
    // Dynamic-wind support
    // -------------------------------------------------------------------------

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
    const std::vector<WindFrame>& get_wind_stack() const { return m_wind_stack; }

private:
    VM* m_vm;                        // Back-reference to VM (for stack access)
    bool m_continuation_invoked;     // Flag: did we just restore from Continuation?

    // Dynamic-wind support
    std::vector<WindFrame> m_wind_stack;  // Stack of active dynamic-wind contexts

    /**
     * @brief Execute before/after thunks when jumping between continuations.
     * @param target_wind_stack Target wind stack from Continuation.
     */
    void rewind_to(const std::vector<WindFrame>& target_wind_stack);

    /**
     * @brief Call a thunk (0-argument Procedure).
     * @param thunk Procedure to invoke.
     */
    void call_thunk(Object* thunk);
};

} // namespace toy
