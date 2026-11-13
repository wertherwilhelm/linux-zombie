#include "pch.h"
#include "StringOutputPort.h"

namespace zombie { namespace runtime {

StringOutputPort::StringOutputPort()
    : OutputPort(m_string_stream)  // Pass string stream to Base class
    , m_string_stream()              // Initialize empty stream (matches declaration order)
{
    // Stream is initialized and ready to accept output
}

StringOutputPort::~StringOutputPort() {
    // Nothing to clean up - stream is owned and will be destroyed automatically
}

} } // namespace zombie::runtime
