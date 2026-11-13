#include "pch.h"
// OutputPort.cpp: implementation of the OutputPort class.
//
//////////////////////////////////////////////////////////////////////

#include "OutputPort.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include "gc_context.h"

namespace zombie { namespace runtime {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

// Global output ports - initialized lazily via init_standard_ports()
// to ensure GC is available
OutputPort* stdout_port = nullptr;
OutputPort* stderr_port = nullptr;

void init_stdout_port() {
    if (stdout_port) return;  // Already initialized

    stdout_port = gc_new<OutputPort>(std::cout);
    stderr_port = gc_new<OutputPort>(std::cerr);

    // Register as persistent roots (never collect)
    if (auto* gc = gc_context::current()) {
        gc->register_persistent_root(reinterpret_cast<gc_object**>(&stdout_port));
        gc->register_persistent_root(reinterpret_cast<gc_object**>(&stderr_port));
    }
}

OutputPort::OutputPort()
: m_port(std::cout)
{

}

OutputPort::OutputPort(std::ostream &port)
: m_port(port)
{

}


OutputPort::~OutputPort() {

}

void OutputPort::set_stream(std::ostream& port) {
    //m_port = port;
}

} } // namespace zombie::runtime
