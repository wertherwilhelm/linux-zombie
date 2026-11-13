/**
 * @file a_parameter.cpp
 * @brief R7RS Parameter Object implementation
 */

#include "pch.h"
#include "a_parameter.h"
#include "Base.h"
#include "constants.h"
#include "i_evaluator.h"
#include <sstream>

namespace zombie { namespace runtime {

// ============================================================================
// Constructor
// ============================================================================

a_parameter::a_parameter(Object* init, Object* converter)
    : m_converter(converter ? converter : nil) {
    // Note: the converter is applied to the initial value at the
    // make-parameter call site (PrimitiveParameters), which has the
    // evaluator needed to invoke it. The constructor stores values as-is.
    m_value = init ? init : nil;
}

// ============================================================================
// Value access
// ============================================================================

void a_parameter::set_value(Object* value) {
    m_value = value ? value : nil;
}

void a_parameter::set_value(IEvaluator* eval, Environment* env, Object* value) {
    if (has_converter() && eval) {
        Procedure* conv = dynamic_cast<Procedure*>(m_converter);
        if (conv) {
            gc_root<Object> guard_value(value);
            Object* arg_list = cons(value, nil);
            gc_root<Object> guard_args(arg_list);
            m_value = conv->apply(eval, arg_list, env);
            return;
        }
    }
    m_value = value ? value : nil;
}

// ============================================================================
// Procedure interface - parameters are callable
// ============================================================================

Object* a_parameter::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Parameters are called as procedures:
    // (p)     -> Get current value
    // (p val) -> Set value to val (returns unspecified)

    if (args->is_nil()) {
        // No arguments: return current value
        return m_value;
    }

    // One argument: set value (converter applies per R7RS)
    Object* new_value = car(args);
    set_value(eval, env, new_value);

    // R7RS: return value is unspecified, we return the new value
    return m_value;
}

// ============================================================================
// Object interface implementation
// ============================================================================

Object* a_parameter::clone() const {
    return gc_new<a_parameter>(m_value, m_converter);
}

void a_parameter::print(std::ostream& os) const {
    os << "#<parameter ";
    if (m_value) {
        m_value->print(os);
    } else {
        os << "nil";
    }
    os << ">";
}

bool a_parameter::is_equal(Object* val) {
    if (!val || !is_parameter(val)) {
        return false;
    }

    a_parameter* other = static_cast<a_parameter*>(val);
    // Parameters are equal if they're the same Object
    return this == other;
}

void a_parameter::trace_references(Tracer& tracer) {
    // Trace current value and converter
    if (m_value && m_value != nil) {
        tracer.visit_field(m_value);
    }
    if (m_converter && m_converter != nil) {
        tracer.visit_field(m_converter);
    }
}

// ============================================================================
// Helper functions
// ============================================================================

bool is_parameter(const Object* obj) {
    return obj && dynamic_cast<const a_parameter*>(obj) != nullptr;
}

} } // namespace zombie::runtime
