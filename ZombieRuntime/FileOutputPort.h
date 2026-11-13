#pragma once

#include "OutputPort.h"
#include <fstream>
#include <string>

namespace zombie { namespace runtime {

/**
 * @file FileOutputPort.h
 * @brief File-backed output port.
 */

/**
 * @brief OutputPort that writes to a file.
 *
 * Owns the ofstream and manages its lifetime.
 */
class FileOutputPort : public OutputPort {
public:
    /**
     * @brief Construct a file output port.
     * @param filename Path to the file to open.
     */
    FileOutputPort(const std::string& filename);

    /**
     * @brief Destroy the file output port.
     */
    virtual ~FileOutputPort();

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
    std::ofstream m_file_stream;  // MUST be first - passed to Base constructor
    std::string m_filename;
};

} } // namespace zombie::runtime
