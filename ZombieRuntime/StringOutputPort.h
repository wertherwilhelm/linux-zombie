#pragma once

#include "OutputPort.h"
#include <sstream>
#include <string>

namespace zombie { namespace runtime {

/**
 * @file StringOutputPort.h
 * @brief String-backed output port.
 */

/**
 * @brief OutputPort that writes to a string.
 *
 * Owns the ostringstream and manages its lifetime.
 * Provides get_string() to retrieve captured content.
 */
class StringOutputPort : public OutputPort {
public:
    /**
     * @brief Construct a string output port.
     */
    StringOutputPort();

    /**
     * @brief Destroy the string output port.
     */
    virtual ~StringOutputPort();

    /**
     * @brief Open the stream (always true for string ports).
     * @return True.
     */
    bool open() { return true; }  // Always "open" since we own the stream

    /**
     * @brief Close the stream (always true for string ports).
     * @return True.
     */
    bool close() { return true; }

    /**
     * @brief Get the captured string content.
     * @return Captured output.
     */
    std::string get_string() const { return m_string_stream.str(); }

private:
    std::ostringstream m_string_stream;  // MUST be first - passed to Base constructor
};

} } // namespace zombie::runtime
