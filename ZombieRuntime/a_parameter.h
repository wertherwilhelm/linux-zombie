#pragma once

/**
 * @file a_parameter.h
 * @brief R7RS Parameter objects (Section 4.2.6)
 */

#include "Object.h"
#include "procedure.h"
#include "gc_ptr.h"

namespace zombie { namespace runtime {

/**
 * @brief R7RS parameter Object
 *
 * Parameters are procedures that access and update dynamic state.
 * They're created with make-parameter and modified temporarily with parameterize.
 *
 * Usage:
 *   (define p (make-parameter 10))
 *   (p)           ; Returns 10
 *   (p 20)        ; Sets value to 20, returns unspecified
 *   (parameterize ((p 30)) (p))  ; Temporarily sets to 30, returns 30
 */
class a_parameter : public Procedure {
public:
    /**
     * @brief Construct parameter with initial value
     * @param init Initial value
     * @param converter Optional converter Procedure (or nil)
     */
    a_parameter(Object* init, Object* converter = nullptr);

    /**
     * @brief Get current value
     */
    Object* get_value() const { return m_value; }

    /**
     * @brief Set current value directly (no converter applied).
     */
    void set_value(Object* value);

    /**
     * @brief Set current value, applying the converter if present.
     * @param eval Evaluator used to invoke the converter Procedure.
     * @param env Environment for the converter call.
     * @param value Raw value; stored converted if a converter exists.
     */
    void set_value(IEvaluator* eval, Environment* env, Object* value);

    /**
     * @brief Get converter Procedure
     */
    Object* get_converter() const { return m_converter; }

    /**
     * @brief Check if this parameter has a converter
     */
    bool has_converter() const { return m_converter && m_converter != nil; }

    // Procedure interface
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    // Object interface
    int get_type() const override { return static_cast<int>(Type::CPP_OBJECT); }
    int is_atom() const override { return 1; }
    int pair() const override { return 0; }
    Object* clone() const override;
    void print(std::ostream& os) const override;
    bool is_equal(Object* val) override;
    void trace_references(Tracer& tracer) override;

private:
    Object* m_value;      // Current parameter value (GC-managed)
    Object* m_converter;  // Optional converter Procedure (GC-managed)
};

/**
 * @brief Check if Object is a parameter
 */
bool is_parameter(const Object* obj);

} } // namespace zombie::runtime
