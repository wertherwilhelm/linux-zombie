#include "pch.h"
#include "FileOutputPort.h"

namespace zombie { namespace runtime {

FileOutputPort::FileOutputPort(const std::string& filename)
    : OutputPort(m_file_stream)  // Pass file stream to Base class
    , m_file_stream()            // Initialize stream first (matches declaration order)
    , m_filename(filename)
{
    // Don't open in constructor - let open() do it
}

FileOutputPort::~FileOutputPort() {
    close();
}

bool FileOutputPort::open() {
    if (m_file_stream.is_open()) {
        return true;  // Already open
    }
    m_file_stream.open(m_filename);
    return m_file_stream.is_open();
}

bool FileOutputPort::close() {
    if (m_file_stream.is_open()) {
        m_file_stream.flush();
        m_file_stream.close();
    }
    return true;
}

} } // namespace zombie::runtime
