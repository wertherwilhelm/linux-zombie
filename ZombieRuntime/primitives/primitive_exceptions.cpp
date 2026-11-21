#include "zombie_runtime_types.h"
#include "primitive_exceptions.h"
#include "../native_primitive.h"
#include "../a_exception.h"
#include "../atom.h"
#include "../Base.h"
#include "../cons_cell.h"
#include "../constants.h"
#include "../exception_handlers.h"
#include <stdexcept>
#include <sstream>

/**
 * @file primitive_exceptions.cpp
 * @brief R7RS exception primitives (6.11), table-driven.
 */

// ============================================================================
// Handler-stack raise (R7RS 6.11)
// ============================================================================

namespace zombie { namespace runtime { namespace exc {

Object* raise_object(IEvaluator* eval, Environment* env, Object* payload, bool continuable) {
    if (!handler_stack.empty()) {
        Object* handler_obj = handler_stack.back();
        Procedure* handler = dynamic_cast<Procedure*>(handler_obj);
        if (handler) {
            gc_root<Object> guard_payload(payload);
            gc_root<Object> guard_handler(handler_obj);

            // The handler runs with the outer handler installed
            handler_pop_scope outer;

            Object* arg_list = cons(payload, nil);
            gc_root<Object> guard_args(arg_list);
            Object* result = handler->apply(eval, arg_list, env);

            if (continuable) {
                // raise-continuable: handler's result is the value of the
                // raise-continuable expression
                return result;
            }

            // raise: the handler must not return. Raise a secondary
            // exception in the handler's dynamic environment (i.e. with the
            // outer handler still current).
            a_exception* secondary = gc_new<a_exception>(
                "raise: exception handler returned from non-continuable exception",
                cons(payload, nil), a_exception::GENERAL);
            gc_root<Object> guard_secondary(secondary);
            return raise_object(eval, env, secondary, false);
        }
    }

    // No handler installed: unwind as a C++ exception carrying the payload
    if (is_exception(payload)) {
        throw scheme_error(static_cast<a_exception*>(payload));
    }

    std::ostringstream oss;
    payload->print(oss);
    gc_root<Object> guard_payload(payload);
    a_exception* wrapped = gc_new<a_exception>(
        "Scheme exception: " + oss.str(), cons(payload, nil), a_exception::GENERAL);
    throw scheme_error(wrapped);
}

} } } // namespace zombie::runtime::exc

// ============================================================================
// Primitives (table-driven)
// ============================================================================

namespace {

using namespace zombie::runtime;
namespace exc = zombie::runtime::exc;

// (raise obj) — invoke the current handler at the raise point; unwind as a
// scheme_error if no handler is installed.
Object* prim_raise(IEvaluator* eval, Object* args, Environment* env) {
    return exc::raise_object(eval, env, first(args), false);
}

// (raise-continuable obj) — handler's result becomes this expression's value.
Object* prim_raise_continuable(IEvaluator* eval, Object* args, Environment* env) {
    return exc::raise_object(eval, env, first(args), true);
}

// (with-exception-handler handler thunk) — install handler for the thunk's extent.
Object* prim_with_exception_handler(IEvaluator* eval, Object* args, Environment* env) {
    Object* x = first(args), *y = second(args);
    Procedure* handler = dynamic_cast<Procedure*>(x);
    Procedure* thunk = dynamic_cast<Procedure*>(y);
    if (!handler || !thunk) return error("with-exception-handler: arguments must be procedures");
    gc_root<Object> guard_handler(x);
    gc_root<Object> guard_thunk(y);
    exc::handler_scope scope(x);
    return thunk->apply(eval, nil, env);
}

// (%guard handler-proc thunk) — runtime support for the Expander's guard
// desugaring. Installs a GuardHandler for the thunk's dynamic extent and
// recovers the payload when it escapes (a Scheme raise, a C++ primitive error,
// or a nested re-raise), then runs the clause dispatcher OUTSIDE this guard's
// handler scope.
Object* prim_guard(IEvaluator* eval, Object* args, Environment* env) {
    Object* x = first(args), *y = second(args);
    Procedure* handler = dynamic_cast<Procedure*>(x);
    Procedure* thunk = dynamic_cast<Procedure*>(y);
    if (!handler || !thunk) return error("%guard: arguments must be procedures");
    gc_root<Object> guard_handler(x);
    gc_root<Object> guard_thunk(y);

    exc::GuardHandler* gh = gc_new<exc::GuardHandler>();
    gc_root<Object> guard_gh(gh);

    Object* payload = nullptr;
    {
        exc::handler_scope scope(gh);
        try {
            return thunk->apply(eval, nil, env);
        } catch (const exc::GuardEscape& ge) {
            if (ge.token != static_cast<const void*>(gh)) throw;  // belongs to an outer guard
            payload = ge.payload;
        } catch (const scheme_error& se) {
            payload = se.exception()
                ? static_cast<Object*>(se.exception())
                : static_cast<Object*>(gc_new<a_exception>(se.message(), nil, a_exception::GENERAL));
        } catch (const std::runtime_error& re) {
            payload = gc_new<a_exception>(re.what(), nil, a_exception::GENERAL);
        }
    }

    gc_root<Object> guard_payload(payload);
    Object* arg_list = cons(payload, nil);
    gc_root<Object> guard_args(arg_list);
    return handler->apply(eval, arg_list, env);  // runs outside the handler scope
}

// (error message obj ...) — raise an error object (R7RS), so handlers see it.
Object* prim_error(IEvaluator* eval, Object* args, Environment* env) {
    Object* x = first(args);
    if (x->get_type() != static_cast<int>(Type::STRING)) {
        return error("error: message must be a string");
    }
    std::string message = static_cast<a_string*>(x)->value();
    Object* irritants = rest(args);
    a_exception* exc_obj = gc_new<a_exception>(message, irritants, a_exception::GENERAL);
    gc_root<Object> guard_exc(exc_obj);
    return exc::raise_object(eval, env, exc_obj, false);
}

Object* prim_error_objectq(IEvaluator*, Object* a, Environment*) {
    return is_exception(first(a)) ? t : static_cast<Object*>(false_obj);
}
Object* prim_file_errorq(IEvaluator*, Object* a, Environment*) {
    return is_file_exception(first(a)) ? t : static_cast<Object*>(false_obj);
}
Object* prim_read_errorq(IEvaluator*, Object* a, Environment*) {
    return is_read_exception(first(a)) ? t : static_cast<Object*>(false_obj);
}

Object* prim_error_message(IEvaluator*, Object* a, Environment*) {
    Object* x = first(a);
    if (!is_exception(x)) return error("error-object-message: argument must be an error object");
    return gc_new<a_string>(static_cast<a_exception*>(x)->message());
}
Object* prim_error_irritants(IEvaluator*, Object* a, Environment*) {
    Object* x = first(a);
    if (!is_exception(x)) return error("error-object-irritants: argument must be an error object");
    Object* irritants = static_cast<a_exception*>(x)->irritants();
    return irritants ? irritants : nil;
}

}  // namespace

void PrimitiveExceptions::install(zombie::runtime::Environment* env) {
    using namespace zombie::runtime;
    static const NativeEntry entries[] = {
        { "raise",                  1, 1,      prim_raise                  },
        { "raise-continuable",      1, 1,      prim_raise_continuable      },
        { "error",                  1, 999999, prim_error                  },
        { "error-object?",          1, 1,      prim_error_objectq          },
        { "file-error?",            1, 1,      prim_file_errorq            },
        { "read-error?",            1, 1,      prim_read_errorq            },
        { "error-object-message",   1, 1,      prim_error_message          },
        { "error-object-irritants", 1, 1,      prim_error_irritants        },
        { "with-exception-handler", 2, 2,      prim_with_exception_handler },
        { "%guard",                 2, 2,      prim_guard                  },
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}
