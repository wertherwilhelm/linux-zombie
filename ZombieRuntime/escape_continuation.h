/**
 * @file escape_continuation.h
 * @brief Escape-only continuations for the recursive LISP evaluator.
 *
 * The recursive evaluator keeps its control state on the C++ stack, so a
 * full re-entrant continuation cannot be captured there (that is the toy
 * VM's and bytecode VM's domain — both intercept call/cc themselves). What
 * CAN be supported is the upward, one-shot case: invoking the continuation
 * unwinds the C++ stack back to the call/cc frame that created it, which is
 * what the overwhelming majority of call/cc uses (early exit, exceptions,
 * generators that only escape) need.
 *
 * Invocation throws EscapeInvoked, deliberately not derived from
 * std::exception so generic catch(const std::exception&) sites cannot
 * swallow it (same pattern as ContinuationInvoked and GuardEscape).
 * dynamic-wind's catch(...) + rethrow still runs after-thunks on the way
 * out, as required.
 *
 * Once the call/cc frame returns, the continuation is invalidated; invoking
 * it afterwards raises a Scheme-visible error instead of unwinding into a
 * stack frame that no longer exists.
 */
#pragma once

#include "Object.h"
#include "procedure.h"
#include "Base.h"
#include <stdexcept>

namespace zombie { namespace runtime {

/**
 * @brief Non-std exception thrown when an escape continuation is invoked.
 * Caught by the call/cc frame whose token matches; outer frames rethrow.
 */
struct EscapeInvoked {
    const void* token;   ///< Identity of the call/cc activation
    Object* value;       ///< Value the continuation was invoked with
};

/**
 * @brief One-shot upward continuation created by the call/cc primitive.
 */
class EscapeContinuation : public Procedure {
public:
    Object* apply(IEvaluator* /*eval*/, Object* args, Environment* /*env*/) override {
        if (!valid_) {
            throw std::runtime_error(
                "call/cc: continuation invoked outside its dynamic extent "
                "(the recursive evaluator only supports escape continuations; "
                "use the toy VM or bytecode VM for re-entrant call/cc)");
        }
        throw EscapeInvoked{ this, (args && !args->is_nil()) ? car(args) : nil };
    }

    /// Called by the creating call/cc frame when its dynamic extent ends.
    void invalidate() { valid_ = false; }

    int get_type() const override { return static_cast<int>(Type::CPP_OBJECT); }
    int is_atom() const override { return 1; }
    int pair() const override { return 0; }
    Object* clone() const override { return const_cast<EscapeContinuation*>(this); }
    void print(std::ostream& os) const override { os << "#<continuation>"; }
    bool is_equal(Object* val) override { return this == val; }
    void trace_references(Tracer& /*tracer*/) override {}

private:
    bool valid_ = true;
};

} } // namespace zombie::runtime
