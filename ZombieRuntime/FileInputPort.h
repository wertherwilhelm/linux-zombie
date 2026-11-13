#pragma once

#include "InputPort.h"
#include <fstream>
#include <string>

namespace zombie { namespace runtime {

/**
 * @file FileInputPort.h
 * @brief File-backed input port.
 */

/**
 * @brief InputPort that reads from a file.
 *
 * Owns the ifstream and manages its lifetime.
 */
class FileInputPort : public InputPort {
public:
    /**
     * @brief Construct a file input port.
     * @param filename Path to the file to open.
     */
    FileInputPort(const std::string& filename);

    /**
     * @brief Destroy the file input port.
     */
    virtual ~FileInputPort();

    /**
     * @brief Open the underlying file stream.
     * @return True on success.
     */
    bool open();

    /**
     * @brief Close the underlying file stream.
     * @return True on success.
     */
    bool close();

    /**
     * @brief Check if the stream is open.
     * @return True if open.
     */
    bool is_open() const { return m_file_stream.is_open(); }

private:
    std::ifstream m_file_stream;  // MUST be first - passed to Base constructor
    std::string m_filename;
};

} } // namespace zombie::runtime
