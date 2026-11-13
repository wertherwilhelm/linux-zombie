#pragma once

#include "InputPort.h"
#include <sstream>
#include <string>

namespace zombie { namespace runtime {

/**
 * @file StringInputPort.h
 * @brief String-backed input port.
 */

/**
 * @brief InputPort that reads from a string.
 *
 * Owns the istringstream and manages its lifetime.
 */
class StringInputPort : public InputPort {
public:
    /**
     * @brief Construct a string input port.
     * @param content String content to read from.
     */
    StringInputPort(const std::string& content);

    /**
     * @brief Destroy the string input port.
     */
    virtual ~StringInputPort();

    /**
     * @brief Open the stream (always true for string ports).
     * @return True.
     */
    bool open() { return true; }  // Always "open" since we own the string

    /**
     * @brief Close the stream (always true for string ports).
     * @return True.
     */
    bool close() { return true; }

private:
    std::istringstream m_string_stream;  // MUST be first - passed to Base constructor
    std::string m_content;
};

} } // namespace zombie::runtime
