#include "pch.h"
#include "StringInputPort.h"

namespace zombie { namespace runtime {

StringInputPort::StringInputPort(const std::string& content)
    : InputPort(m_string_stream)  // Pass string stream to Base class
    , m_string_stream(content)     // Initialize stream with content (matches declaration order)
    , m_content(content)
{
    // Stream is already initialized with content
}

StringInputPort::~StringInputPort() {
    // Nothing to clean up - stream is owned and will be destroyed automatically
}

} } // namespace zombie::runtime
