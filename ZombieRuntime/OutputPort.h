#pragma once

#include <iostream>
#include "Base.h"
#include <string>

namespace zombie { namespace runtime {

/*
* Ports represent input and output devices. To Scheme, an input port is a Scheme Object that can deliver
* characters upon command, while an output port is a Scheme Object that can accept characters.
*/
class OutputPort : public Base {
public:
    OutputPort();
    OutputPort(std::ostream& port = std::cout);
    virtual ~OutputPort();

    int instance_of() override { return static_cast<int>(Base::atype::OUTPUTPORT); }
    Object* clone() const override { return const_cast<OutputPort*>(this); }  // Ports should not be cloned

    void set_stream(std::ostream& port = std::cout);

    void write(const std::string str) {
        m_port << str;
    }

    void write(Object* o) {
        m_port << o;
    }

    void new_line() {
        m_port << std::endl;
    }

    void write_char(char c) {
        m_port << c;
    }

private:

    std::ostream &m_port;
};

extern OutputPort *stdout_port;
extern OutputPort* stderr_port;

// Initialize stdout_port and stderr_port through GC (called by init_lisp_constants)
void init_stdout_port();

inline void write(Object* o) {
    stdout_port->write(o);
}

// should the first paramater be a port with a way to identify it?
inline void write(OutputPort* port, Object *o) {
    port->write(o);
}

inline void error(OutputPort* port, Object* o) {
    stderr_port->write(o);
}

inline void error(OutputPort* port, std::string str) {
    stderr_port->write(str);
}

} } // namespace zombie::runtime
