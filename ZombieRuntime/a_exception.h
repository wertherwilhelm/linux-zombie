#pragma once

/**
 * @file a_exception.h
 * @brief R7RS Exception objects (Section 6.11)
 */

#include "Object.h"
#include "atom.h"
#include "gc_ptr.h"
#include <stdexcept>

namespace zombie { namespace runtime {

/**
 * @brief R7RS error-Object type
 *
 * Error objects carry:
 * - A message (string)
 * - A list of irritants (arbitrary Scheme values)
 * - Error kind (general, file, read)
 */
class a_exception : public ValueType {
public:
    enum ErrorKind {
        GENERAL,   // General error
        FILE,      // File I/O error
        READ       // Read/parse error
    };

    /**
     * @brief Construct error Object with message and irritants
     * @param message Error message
     * @param irritants List of irritant values (or nil)
     * @param kind Error kind (default: GENERAL)
     */
    a_exception(const std::string& message, Object* irritants = nullptr, ErrorKind kind = GENERAL);

    /**
     * @brief Get error message
     */
    const std::string& message() const { return m_message; }

    /**
     * @brief Get irritants list
     */
    Object* irritants() const { return m_irritants; }

    /**
     * @brief Get error kind
     */
    ErrorKind kind() const { return m_kind; }

    /**
     * @brief Check if this is a file error
     */
    bool is_file_error() const { return m_kind == FILE; }

    /**
     * @brief Check if this is a read error
     */
    bool is_read_error() const { return m_kind == READ; }

    // Object interface
    int get_type() const override { return static_cast<int>(Type::EXCEPTION); }
    int is_atom() const override { return 1; }
    int pair() const override { return 0; }
    Object* clone() const override;
    void print(std::ostream& os) const override;
    bool is_equal(Object* val) override;
    void trace_references(Tracer& tracer) override;

private:
    std::string m_message;
    Object* m_irritants;    ///< List of irritants (GC-managed)
    ErrorKind m_kind;
};

/**
 * @brief Check if Object is an exception
 */
bool is_exception(const Object* obj);

/**
 * @brief Check if Object is a file error
 */
bool is_file_exception(const Object* obj);

/**
 * @brief Check if Object is a read error
 */
bool is_read_exception(const Object* obj);

// ============================================================================
// C++ Exception Wrapper
// ============================================================================

/**
 * @brief C++ exception that wraps a Scheme exception Object
 *
 * This allows C++ code to throw exceptions that carry structured Scheme
 * error information (message, irritants, error kind) instead of just strings.
 *
 * Usage:
 * @code
 * // Throw structured error
 * throw scheme_error("division by zero", cons(divisor, nil));
 *
 * // Catch and extract Scheme exception
 * try {
 *     eval(expr);
 * } catch (const scheme_error& e) {
 *     a_exception* exc = e.exception();  // Get Scheme error Object
 *     std::string msg = exc->message();
 *     Object* irritants = exc->irritants();
 * }
 * @endcode
 */
class scheme_error : public std::runtime_error {
public:
    /**
     * @brief Construct from message (creates GENERAL error)
     */
    explicit scheme_error(const std::string& message);

    /**
     * @brief Construct from message and irritants
     */
    scheme_error(const std::string& message, Object* irritants);

    /**
     * @brief Construct from message, irritants, and error kind
     */
    scheme_error(const std::string& message, Object* irritants, a_exception::ErrorKind kind);

    /**
     * @brief Construct from existing a_exception Object
     */
    explicit scheme_error(a_exception* exc);

    /**
     * @brief Get the wrapped Scheme exception Object
     * @return Pointer to a_exception (may be nullptr if not created via GC)
     */
    a_exception* exception() const { return m_exception; }

    /**
     * @brief Get error message
     */
    const std::string& message() const { return m_message; }

    /**
     * @brief Get error kind
     */
    a_exception::ErrorKind kind() const { return m_kind; }

private:
    std::string m_message;
    a_exception::ErrorKind m_kind;
    a_exception* m_exception;  // May be nullptr if GC not available
};

} } // namespace zombie::runtime
