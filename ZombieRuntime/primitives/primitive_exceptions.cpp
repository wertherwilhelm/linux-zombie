/**
 * @file primitive_exceptions.cpp
 * @brief Implementation of R7RS exception handling primitives
 */

#include "pch.h"
#include "../pch.h"
#include "primitive_exceptions.h"
#include "../a_exception.h"
#include "../atom.h"
#include "../Base.h"
#include "../cons_cell.h"
#include "../constants.h"
#include "../exception_handlers.h"
#include <stdexcept>
#include <sstream>

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
// Constructor
// ============================================================================

PrimitiveExceptions::PrimitiveExceptions(ID id, int minArgs, int maxArgs)
    : PrimitiveBase(minArgs, maxArgs), m_id(id) {
    // Set Primitive name based on ID
    switch (id) {
        case RAISE:                 m_name = "raise"; break;
        case RAISECONTINUABLE:      m_name = "raise-continuable"; break;
        case ERROR:                 m_name = "error"; break;
        case ERROROBJECTQ:          m_name = "error-Object?"; break;
        case ERROROBJECTMESSAGE:    m_name = "error-Object-message"; break;
        case ERROROBJECTIRRITANTS:  m_name = "error-Object-irritants"; break;
        case FILEERRORQ:            m_name = "file-error?"; break;
        case READERRORQ:            m_name = "read-error?"; break;
        case WITHEXCEPTIONHANDLER:  m_name = "with-exception-handler"; break;
        case GUARDIMPL:             m_name = "%guard"; break;
    }
}

// ============================================================================
// Primitive Application
// ============================================================================

Object* PrimitiveExceptions::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Arity checking
    if (!check_arity(args)) {
        return arity_error("");
    }

    // Extract common arguments
    Object* x = first(args);
    Object* y = (rest(args) && !rest(args)->is_nil()) ? second(args) : nil;

    // Dispatch based on Primitive ID
    switch (m_id) {
        // ====================================================================
        // Raising Exceptions
        // ====================================================================
        case RAISE:
            // (raise obj) - invoke the current handler at the raise point;
            // unwind as scheme_error if no handler is installed
            return zombie::runtime::exc::raise_object(eval, env, x, false);

        case RAISECONTINUABLE:
            // (raise-continuable obj) - handler's result becomes the value
            // of this expression
            return zombie::runtime::exc::raise_object(eval, env, x, true);

        case WITHEXCEPTIONHANDLER: {
            // (with-exception-handler handler thunk)
            Procedure* handler = dynamic_cast<Procedure*>(x);
            Procedure* thunk = dynamic_cast<Procedure*>(y);
            if (!handler || !thunk) {
                return error("with-exception-handler: arguments must be procedures");
            }

            // Keep the handler alive while it sits on the handler stack
            gc_root<Object> guard_handler(x);
            gc_root<Object> guard_thunk(y);
            zombie::runtime::exc::handler_scope scope(x);
            return thunk->apply(eval, nil, env);
        }

        case GUARDIMPL: {
            // (%guard handler-proc thunk) — runtime support for compiled
            // guard forms. The bytecode compiler rewrites
            //   (guard (var clause ...) body ...)
            // to
            //   (%guard (lambda (var) (cond clause ... [else (raise var)]))
            //           (lambda () body ...))
            // This frame owns the C++ RAII work the compiled code cannot
            // express: installing a GuardHandler for the thunk's dynamic
            // extent and recovering the payload when it escapes. Mirrors
            // the recursive evaluator's native guard (lisp.cpp).
            Procedure* handler = dynamic_cast<Procedure*>(x);
            Procedure* thunk = dynamic_cast<Procedure*>(y);
            if (!handler || !thunk) {
                return error("%guard: arguments must be procedures");
            }
            gc_root<Object> guard_handler(x);
            gc_root<Object> guard_thunk(y);

            zombie::runtime::exc::GuardHandler* gh =
                gc_new<zombie::runtime::exc::GuardHandler>();
            gc_root<Object> guard_gh(gh);

            Object* payload = nullptr;
            {
                zombie::runtime::exc::handler_scope scope(gh);
                try {
                    return thunk->apply(eval, nil, env);
                } catch (const zombie::runtime::exc::GuardEscape& ge) {
                    if (ge.token != static_cast<const void*>(gh)) {
                        throw;  // belongs to an outer guard
                    }
                    payload = ge.payload;
                } catch (const scheme_error& se) {
                    payload = se.exception()
                        ? static_cast<Object*>(se.exception())
                        : static_cast<Object*>(gc_new<a_exception>(se.message(), nil, a_exception::GENERAL));
                } catch (const std::runtime_error& re) {
                    payload = gc_new<a_exception>(re.what(), nil, a_exception::GENERAL);
                }
            }

            // The compiled clause dispatcher gets the payload; its appended
            // else clause re-raises (now outside this guard's handler scope)
            gc_root<Object> guard_payload(payload);
            Object* arg_list = cons(payload, nil);
            gc_root<Object> guard_args(arg_list);
            return handler->apply(eval, arg_list, env);
        }

        case ERROR: {
            // (error message obj ...)
            // Creates and raises an error Object
            if (x->get_type() != static_cast<int>(Type::STRING)) {
                return error("error: message must be a string");
            }

            a_string* msg_str = static_cast<a_string*>(x);
            std::string message = msg_str->value();

            // Collect irritants (remaining arguments)
            Object* irritants = rest(args);

            // Create error Object
            a_exception* exc = gc_new<a_exception>(message, irritants, a_exception::GENERAL);
            gc_root<Object> guard_exc(exc);

            // error = raise of an error object (R7RS), so handlers see it
            return zombie::runtime::exc::raise_object(eval, env, exc, false);
        }

        // ====================================================================
        // Exception Predicates
        // ====================================================================
        case ERROROBJECTQ:
            // (error-Object? obj)
            return is_exception(x) ? t : static_cast<Object*>(false_obj);

        case FILEERRORQ:
            // (file-error? obj)
            return is_file_exception(x) ? t : static_cast<Object*>(false_obj);

        case READERRORQ:
            // (read-error? obj)
            return is_read_exception(x) ? t : static_cast<Object*>(false_obj);

        // ====================================================================
        // Exception Accessors
        // ====================================================================
        case ERROROBJECTMESSAGE: {
            // (error-Object-message error-Object)
            if (!is_exception(x)) {
                return error("error-Object-message: argument must be an error Object");
            }

            a_exception* exc = static_cast<a_exception*>(x);
            return gc_new<a_string>(exc->message());
        }

        case ERROROBJECTIRRITANTS: {
            // (error-Object-irritants error-Object)
            if (!is_exception(x)) {
                return error("error-Object-irritants: argument must be an error Object");
            }

            a_exception* exc = static_cast<a_exception*>(x);
            Object* irritants = exc->irritants();
            return irritants ? irritants : nil;
        }

        default:
            return error("Unknown exception Primitive");
    }
}

// ============================================================================
// Installation
// ============================================================================

void PrimitiveExceptions::install(Environment* env) {
    // GC Safety: Protect each name string before allocating the primitive
    // to prevent collection if the second gc_new triggers GC.

    // Raising exceptions
    gc_root<a_string> name_raise(gc_new<a_string>("raise"));
    env->define(name_raise.get(), gc_new<PrimitiveExceptions>(RAISE, 1, 1));

    gc_root<a_string> name_raise_cont(gc_new<a_string>("raise-continuable"));
    env->define(name_raise_cont.get(), gc_new<PrimitiveExceptions>(RAISECONTINUABLE, 1, 1));

    gc_root<a_string> name_error(gc_new<a_string>("error"));
    env->define(name_error.get(), gc_new<PrimitiveExceptions>(ERROR, 1, 999999));

    // Exception predicates
    gc_root<a_string> name_error_obj(gc_new<a_string>("error-Object?"));
    env->define(name_error_obj.get(), gc_new<PrimitiveExceptions>(ERROROBJECTQ, 1, 1));

    gc_root<a_string> name_file_err(gc_new<a_string>("file-error?"));
    env->define(name_file_err.get(), gc_new<PrimitiveExceptions>(FILEERRORQ, 1, 1));

    gc_root<a_string> name_read_err(gc_new<a_string>("read-error?"));
    env->define(name_read_err.get(), gc_new<PrimitiveExceptions>(READERRORQ, 1, 1));

    // Exception accessors
    gc_root<a_string> name_err_msg(gc_new<a_string>("error-Object-message"));
    env->define(name_err_msg.get(), gc_new<PrimitiveExceptions>(ERROROBJECTMESSAGE, 1, 1));

    gc_root<a_string> name_err_irr(gc_new<a_string>("error-Object-irritants"));
    env->define(name_err_irr.get(), gc_new<PrimitiveExceptions>(ERROROBJECTIRRITANTS, 1, 1));

    // Handler installation
    gc_root<a_string> name_weh(gc_new<a_string>("with-exception-handler"));
    env->define(name_weh.get(), gc_new<PrimitiveExceptions>(WITHEXCEPTIONHANDLER, 2, 2));

    // Runtime support for compiled guard forms (bytecode VM)
    gc_root<a_string> name_guard_impl(gc_new<a_string>("%guard"));
    env->define(name_guard_impl.get(), gc_new<PrimitiveExceptions>(GUARDIMPL, 2, 2));
}
