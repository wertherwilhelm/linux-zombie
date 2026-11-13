#pragma once

#include "continuation.h"  // For toy::WindFrame
#include <vector>
#include <stdexcept>

/**
 * @file wind_stack_utils.h
 * @brief Shared dynamic-wind algorithms for multiple evaluators.
 */

// Use Object type from zombie::runtime namespace
using zombie::runtime::Object;

/**
 * @brief Shared dynamic-wind utilities.
 *
 * Provides evaluator-agnostic helpers that implement R5RS dynamic-wind
 * semantics. The VM supplies the wind stack and a thunk executor.
 */
namespace wind_stack_utils {

// ============================================================================
// Shared Wind Stack Algorithms
// ============================================================================
//
// These utilities implement the R5RS dynamic-wind protocol using pure
// algorithms that work with any evaluator. The calling VM provides:
//   - thunk_caller: Function to execute a 0-argument Procedure
//   - wind_stack: Reference to the VM's wind stack
//
// This allows code reuse between toy_vm and bytecode_vm while maintaining
// proper separation of concerns.
//
// R5RS Semantics:
//   (dynamic-wind before thunk after)
//
//   1. Call before thunk
//   2. Push wind frame onto stack
//   3. Execute thunk (may escape via call/cc)
//   4. Pop wind frame
//   5. Call after thunk
//
//   When continuations are invoked:
//   - Rewind algorithm ensures proper before/after thunk execution
//   - After thunks called in LIFO order when unwinding
//   - Before thunks called in FIFO order when rewinding
// ============================================================================

/**
 * @brief Execute before thunk and push wind frame onto stack.
 *
 * Template parameter ThunkCaller: Callable with signature void(Object*).
 * Examples: lambda, function pointer, std::function<void(Object*)>.
 *
 * @tparam ThunkCaller Callable type used to invoke a 0-arg thunk.
 * @param wind_stack The VM's wind stack (modified in place).
 * @param before_thunk Procedure to call before entering dynamic extent.
 * @param after_thunk Procedure to call after exiting dynamic extent.
 * @param thunk_caller VM-specific callable that executes a thunk.
 */
template<typename ThunkCaller>
void enter_wind(
    std::vector<toy::WindFrame>& wind_stack,
    Object* before_thunk,
    Object* after_thunk,
    ThunkCaller thunk_caller
) {
    // Call the before thunk
    thunk_caller(before_thunk);

    // Push the wind frame onto the stack
    wind_stack.push_back(toy::WindFrame(before_thunk, after_thunk));
}

/**
 * @brief Pop wind frame and execute after thunk.
 *
 * @tparam ThunkCaller Callable type used to invoke a 0-arg thunk.
 * @param wind_stack The VM's wind stack (modified in place).
 * @param thunk_caller VM-specific callable that executes a thunk.
 * @throws std::runtime_error if wind stack is empty.
 */
template<typename ThunkCaller>
void exit_wind(
    std::vector<toy::WindFrame>& wind_stack,
    ThunkCaller thunk_caller
) {
    if (wind_stack.empty()) {
        throw std::runtime_error("exit_wind: wind stack is empty");
    }

    // Get the top wind frame
    toy::WindFrame frame = wind_stack.back();
    wind_stack.pop_back();

    // Call the after thunk
    thunk_caller(frame.after_thunk);
}

/**
 * @brief Rewind wind stack when jumping between continuations.
 *
 * Executes appropriate before/after thunks to maintain R5RS semantics.
 *
 * Algorithm:
 *   1. Find common depth (same thunks by pointer identity)
 *   2. Unwind: call after thunks in reverse order for exiting frames
 *   3. Rewind: call before thunks in forward order for entering frames
 *   4. Update current stack to match target
 *
 * Example:
 *   Current: [A, B, C]
 *   Target:  [A, D, E]
 *   Common depth: 1 (only A is common)
 *
 *   Actions:
 *     1. Call C.after (exit C)
 *     2. Call B.after (exit B)
 *     3. Call D.before (enter D)
 *     4. Call E.before (enter E)
 *
 *   Result: [A, D, E]
 *
 * @tparam ThunkCaller Callable type used to invoke a 0-arg thunk.
 * @param current_wind_stack The VM's current wind stack (modified in place).
 * @param target_wind_stack The target wind stack (from Continuation).
 * @param thunk_caller VM-specific callable that executes a thunk.
 */
template<typename ThunkCaller>
void rewind_to(
    std::vector<toy::WindFrame>& current_wind_stack,
    const std::vector<toy::WindFrame>& target_wind_stack,
    ThunkCaller thunk_caller
) {
    #ifdef DEBUG_WIND_STACK
    std::cout << "[rewind_to] Current depth: " << current_wind_stack.size()
              << ", Target depth: " << target_wind_stack.size() << std::endl;
    #endif

    // Find the common ancestor between current and target wind stacks
    // We compare by pointer identity (same thunk objects = same wind frame)
    size_t common_depth = 0;
    while (common_depth < current_wind_stack.size() &&
           common_depth < target_wind_stack.size() &&
           current_wind_stack[common_depth].before_thunk == target_wind_stack[common_depth].before_thunk &&
           current_wind_stack[common_depth].after_thunk == target_wind_stack[common_depth].after_thunk) {
        common_depth++;
    }

    #ifdef DEBUG_WIND_STACK
    std::cout << "[rewind_to] Common depth: " << common_depth << std::endl;
    #endif

    // Unwind: Call after thunks for frames we're exiting (in reverse order)
    // This maintains the invariant that after thunks run in LIFO order
    for (size_t i = current_wind_stack.size(); i > common_depth; --i) {
        #ifdef DEBUG_WIND_STACK
        std::cout << "[rewind_to] Calling AFTER thunk (exit)" << std::endl;
        #endif
        thunk_caller(current_wind_stack[i - 1].after_thunk);
    }

    // Rewind: Call before thunks for frames we're entering (in forward order)
    // This maintains the invariant that before thunks run in FIFO order
    for (size_t i = common_depth; i < target_wind_stack.size(); ++i) {
        #ifdef DEBUG_WIND_STACK
        std::cout << "[rewind_to] Calling BEFORE thunk (enter)" << std::endl;
        #endif
        thunk_caller(target_wind_stack[i].before_thunk);
    }

    // Update the wind stack to match the target
    current_wind_stack = target_wind_stack;
}

} // namespace wind_stack_utils
