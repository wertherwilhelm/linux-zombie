#include "pch.h"
#include "FileInputPort.h"

namespace zombie { namespace runtime {

FileInputPort::FileInputPort(const std::string& filename)
    : InputPort(m_file_stream)  // Pass file stream to Base class
    , m_file_stream()           // Initialize stream first (matches declaration order)
    , m_filename(filename)
{
    // Don't open in constructor - let open() do it
}

FileInputPort::~FileInputPort() {
    close();
}

bool FileInputPort::open() {
    if (m_file_stream.is_open()) {
        return true;  // Already open
    }
    m_file_stream.open(m_filename);
    return m_file_stream.is_open();
}

bool FileInputPort::close() {
    if (m_file_stream.is_open()) {
        m_file_stream.close();
    }
    return true;
}

} } // namespace zombie::runtime
