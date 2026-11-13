#pragma once

#include "Base.h"
#include "../GarbageCollector/gc_tracer.h"

namespace zombie { namespace runtime {

/**
 * @file ValuesObject.h
 * @brief Multiple return values container for R5RS (values ...).
 */

/**
 * @brief Represents multiple return values from the R5RS (values ...) form.
 *
 * In R5RS, (values x y z) returns multiple values that can be captured by
 * call-with-values.
 *
 * @code
 * (call-with-values
 *   (lambda () (values 1 2 3))
 *   (lambda (a b c) (+ a b c)))
 * => 6
 * @endcode
 */
class ValuesObject : public Base {
public:
    /**
     * @brief Construct a values Object from a list of values.
     * @param vals List of values (can be nil for zero values).
     */
    explicit ValuesObject(Object* vals) : m_values(vals) {}

    /**
     * @brief Destroy the values Object.
     */
    virtual ~ValuesObject() {}

    /**
     * @brief Get the list of values.
     */
    Object* get_values() const { return m_values; }

    /**
     * @brief Get number of values.
     * @return Count of values in the list.
     */
    int count() const {
        return length(m_values);
    }

    /**
     * @brief Return runtime instance type code.
     */
    int instance_of() override { return static_cast<int>(Base::atype::VALUES); }

    /**
     * @brief Clone this values Object.
     */
    Object* clone() const override { return new ValuesObject(m_values); }

    /**
     * @brief Trace referenced objects for GC.
     */
    void trace_references(Tracer& tracer) override {
        tracer.visit_field(m_values);
    }

private:
    Object* m_values;
};

} } // namespace zombie::runtime
