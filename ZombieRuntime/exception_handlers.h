/**
 * @file exception_handlers.h
 * @brief R7RS exception handler stack (with-exception-handler / raise / guard)
 *
 * Handlers are installed for a dynamic extent (with-exception-handler, guard)
 * and invoked by raise/raise-continuable AT THE RAISE POINT, before any stack
 * unwinding. That is what makes raise-continuable resumable: the handler's
 * return value becomes the value of the raise-continuable expression.
 *
 * Per R7RS 6.11 a handler runs with the OUTER handler installed, so a raise
 * from inside a handler reaches the next handler out (handler_pop_scope).
 *
 * GC note: the stack holds raw Object*. Every frame that pushes a handler
 * (the with-exception-handler primitive, the guard special form) must keep
 * the handler alive with a gc_root for as long as it is on the stack.
 */
#pragma once

#include "Object.h"
#include "atom.h"
#include "procedure.h"
#include "Base.h"
#include <vector>

namespace zombie { namespace runtime { namespace exc {

/// Current handler stack, innermost handler last. Thread-local because
/// dynamic extent follows the C++ call stack.
inline thread_local std::vector<Object*> handler_stack;

/// RAII: install a handler for the current scope.
struct handler_scope {
    explicit handler_scope(Object* h) { handler_stack.push_back(h); }
    ~handler_scope() { handler_stack.pop_back(); }
    handler_scope(const handler_scope&) = delete;
    handler_scope& operator=(const handler_scope&) = delete;
};

/// RAII: remove the current handler while it runs, so a raise inside the
/// handler goes to the next handler out. Restores on scope exit (including
/// unwinding, which keeps the stack balanced with the pushing frame's
/// handler_scope destructor).
struct handler_pop_scope {
    Object* saved;
    handler_pop_scope() : saved(handler_stack.back()) { handler_stack.pop_back(); }
    ~handler_pop_scope() { handler_stack.push_back(saved); }
    handler_pop_scope(const handler_pop_scope&) = delete;
    handler_pop_scope& operator=(const handler_pop_scope&) = delete;
};

/**
 * @brief Non-std exception thrown by a guard's handler to escape to its
 * guard expression. Deliberately not derived from std::exception so generic
 * catch(const std::exception&) sites cannot swallow it (same pattern as
 * ContinuationInvoked in continuation.h).
 */
struct GuardEscape {
    const void* token;   ///< Identity of the guard activation (its handler)
    Object* payload;     ///< The raised object
};

/**
 * @brief Handler installed by the guard special form. Invoked at the raise
 * point; immediately escapes to the matching guard frame via GuardEscape.
 */
class GuardHandler : public Procedure {
public:
    Object* apply(IEvaluator* /*eval*/, Object* args, Environment* /*env*/) override {
        throw GuardEscape{ this, (args && !args->is_nil()) ? car(args) : nil };
    }

    int get_type() const override { return static_cast<int>(Type::CPP_OBJECT); }
    int is_atom() const override { return 1; }
    int pair() const override { return 0; }
    Object* clone() const override { return const_cast<GuardHandler*>(this); }
    void print(std::ostream& os) const override { os << "#<guard-handler>"; }
    bool is_equal(Object* val) override { return this == val; }
    void trace_references(Tracer& /*tracer*/) override {}
};

/**
 * @brief Raise payload through the handler stack.
 *
 * With a handler installed: invokes it (with the outer handler current).
 * For raise-continuable the handler's result is returned. For raise, a
 * returning handler triggers a secondary exception to the outer handler.
 * With no handler: unwinds as a C++ scheme_error carrying the payload.
 *
 * Defined in primitives/primitive_exceptions.cpp.
 */
Object* raise_object(IEvaluator* eval, Environment* env, Object* payload, bool continuable);

} } } // namespace zombie::runtime::exc
