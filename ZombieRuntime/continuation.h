#pragma once

#include "Object.h"
#include "procedure.h"
#include "Environment.h"
#include <vector>
#include <deque>
#include <iostream>
#include <memory>

/**
 * @file Continuation.h
 * @brief First-class continuations and dynamic-wind frames.
 */

// Need complete Frame definition for std::vector<Frame>
#include "toy_vm.h"

namespace zombie { namespace bytecode {
    struct BytecodeFrame;
} } // namespace zombie::bytecode

// WindFrame needs to be fully defined here because Continuation uses std::vector<WindFrame>
namespace toy {
    /**
     * @brief Represents an active dynamic-wind context.
     *
     * Stores the before/after thunks that should be called when entering or
     * exiting the dynamic extent of the wind.
     */
    struct WindFrame {
        Object* before_thunk;   ///< Thunk to call when entering.
        Object* after_thunk;    ///< Thunk to call when exiting.

        WindFrame(Object* before, Object* after)
            : before_thunk(before), after_thunk(after) {}
    };
}

namespace zombie { namespace runtime {

class Continuation;

/**
 * @brief Internal exception thrown when a bytecode-VM continuation is invoked.
 *
 * Unwinds the C++ stack to the VM execution loop, which catches it and
 * restores the captured frames. Must be the single shared definition: a
 * throw and catch of structurally identical but distinct types (e.g. a
 * redeclaration in another namespace) will NOT match.
 */
struct ContinuationInvoked {
    Continuation* cont;
    Object* return_value;

    ContinuationInvoked(Continuation* c, Object* val) : cont(c), return_value(val) {}
};

/**
 * @brief First-class Continuation captured by call/cc.
 *
 * Captures VM execution state at a point in time. When invoked, it restores
 * that state and returns the provided value as the result of the original
 * call/cc. For R5RS compliance, it also captures the dynamic-wind stack so
 * before/after thunks are replayed correctly on non-local control transfers.
 *
 * Continuations are callable (procedures), so they extend Procedure.
 */
class Continuation : public Procedure {
public:
    /// Toy VM state (used when vm_type == TOY_VM).
    std::vector<toy::Frame> saved_stack;
    /// Saved environment for both VM types.
    Environment* saved_env;
    /// Captured dynamic-wind stack.
    std::vector<toy::WindFrame> saved_wind_stack;  // For dynamic-wind support

    /// Bytecode VM state (shared_ptr to avoid incomplete type issues).
    std::shared_ptr<std::deque<zombie::bytecode::BytecodeFrame>> saved_bc_frames;
    /// Saved accumulator for bytecode VM.
    Object* saved_accumulator;

    /**
     * @brief Which VM type this Continuation targets.
     */
    enum VMType { TOY_VM, BYTECODE_VM } vm_type;

    /**
     * @brief Construct a Continuation for the toy VM.
     * @param stack Saved toy VM stack.
     * @param env Saved environment.
     */
    Continuation(const std::vector<toy::Frame>& stack, Environment* env);

    /**
     * @brief Construct a Continuation for the toy VM with wind stack.
     * @param stack Saved toy VM stack.
     * @param env Saved environment.
     * @param wind_stack Captured dynamic-wind stack.
     */
    Continuation(const std::vector<toy::Frame>& stack, Environment* env,
                const std::vector<toy::WindFrame>& wind_stack);

    /**
     * @brief Construct a Continuation for the bytecode VM.
     * @param frames Saved bytecode frames.
     * @param accumulator Saved accumulator.
     * @param env Saved environment.
     */
    Continuation(const std::deque<zombie::bytecode::BytecodeFrame>& frames,
                Object* accumulator, Environment* env);  // Implemented in .cpp to avoid incomplete type issues

    /**
     * @brief Construct a Continuation for the bytecode VM with wind stack.
     * @param frames Saved bytecode frames.
     * @param accumulator Saved accumulator.
     * @param env Saved environment.
     * @param wind_stack Captured dynamic-wind stack.
     */
    Continuation(const std::deque<zombie::bytecode::BytecodeFrame>& frames,
                Object* accumulator, Environment* env,
                const std::vector<toy::WindFrame>& wind_stack);  // With dynamic-wind support

    /**
     * @brief Invoke the Continuation with a return value.
     *
     * For the toy VM, this restores the saved stack and accumulator directly.
     * For the bytecode VM, this throws an internal exception used to unwind
     * to the VM loop, which then restores state.
     *
     * @param evaluator Evaluator expected to match the captured VM type.
     * @param args Argument list; first argument is the return value.
     * @param env Unused (continuations ignore dynamic environment here).
     * @return The return value for toy VM; bytecode VM uses non-local exit.
     */
    Object* apply(class IEvaluator* evaluator, Object* args, Environment* env) override;

    /**
     * @brief Clone this Continuation.
     */
    Object* clone() const override;  // Implemented in .cpp to avoid incomplete type issues

    /**
     * @brief Print Continuation representation.
     */
    void print(std::ostream& out) const override {
        out << "<Continuation>";
    }

    /**
     * @brief Return type code for continuations.
     */
    int get_type() const override { return 999; }  // Arbitrary type for Continuation

    /**
     * @brief Continuations are atoms.
     */
    int is_atom() const override { return 1; }  // Continuations are atomic

    /**
     * @brief Continuations are not pairs.
     */
    int pair() const override { return 0; }  // Continuations are not pairs

    /**
     * @brief Trace referenced objects for GC.
     */
    void trace_references(Tracer& tracer) override;

    /**
     * @brief Stream insertion operator for continuations.
     */
    friend std::ostream& operator<<(std::ostream& out, Continuation* cont) {
        out << "<Continuation>";
        return out;
    }
};

inline std::ostream& operator<<(std::ostream& out, Continuation& cont) {
    return out << &cont;
}

} } // namespace zombie::runtime
