#include "pch.h"
#include "../pch.h"
#include "primitive_io.h"
#include "../Base.h"
#include "../atom.h"
#include "../InputPort.h"
#include "../OutputPort.h"
#include "../FileInputPort.h"
#include "../FileOutputPort.h"
#include "../StringInputPort.h"
#include "../StringOutputPort.h"
#include "../scheme_reader.h"
#include "../constants.h"
#include "../i_evaluator.h"
#include "../numeric_runtime.h"
#include "../a_bytevector.h"
#include "../../GarbageCollector/gc_ptr.h"
#include <fstream>
#include <sstream>
#include <cctype>

/**
 * @file primitive_io.cpp
 * @brief Implementation of I/O primitives (R5RS Section 6.10)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 */

// ============================================================================
// Constructor
// ============================================================================

PrimitiveIO::PrimitiveIO(ID id, int minArgs, int maxArgs)
    : PrimitiveBase(minArgs, maxArgs), m_id(id) {
    // Set Primitive name based on ID
    switch (id) {
        case EOFOBJECTQ:       m_name = "eof-Object?"; break;
        case INPUTPORTQ:       m_name = "input-port?"; break;
        case CURRENTINPUTPORT: m_name = "current-input-port"; break;
        case OPENINPUTFILE:    m_name = "open-input-file"; break;
        case CLOSEINPUTPORT:   m_name = "close-input-port"; break;
        case OUTPUTPORTQ:      m_name = "output-port?"; break;
        case CURRENTOUTPUTPORT:m_name = "current-output-port"; break;
        case OPENOUTPUTFILE:   m_name = "open-output-file"; break;
        case CLOSEOUTPUTPORT:  m_name = "close-output-port"; break;
        case READCHAR:         m_name = "read-char"; break;
        case PEEKCHAR:         m_name = "peek-char"; break;
        case CHARREADYQ:       m_name = "char-ready?"; break;
        case READ:             m_name = "read"; break;
        case LOAD:             m_name = "load"; break;
        case WRITE:            m_name = "write"; break;
        case DISPLAY:          m_name = "display"; break;
        case NEWLINE:          m_name = "newline"; break;
        case WRITECHAR:        m_name = "write-char"; break;
        case THEENVIRONMENT:   m_name = "the-environment"; break;

        // R7RS additions
        case READLINE:         m_name = "read-line"; break;
        case READSTRING:       m_name = "read-string"; break;
        case WRITESTRING:      m_name = "write-string"; break;
        case FLUSHOUTPUT:      m_name = "flush-output-port"; break;

        // R7RS string ports
        case OPENINPUTSTRING:  m_name = "open-input-string"; break;
        case OPENOUTPUTSTRING: m_name = "open-output-string"; break;
        case GETOUTPUTSTRING:  m_name = "get-output-string"; break;

        // R7RS bytevector I/O
        case READU8:      m_name = "read-u8"; break;
        case PEEKU8:      m_name = "peek-u8"; break;
        case U8READYQ:    m_name = "u8-ready?"; break;
        case WRITEU8:     m_name = "write-u8"; break;
        case READBYTEVECTOR:     m_name = "read-bytevector"; break;
        case READBYTEVECTORBANG: m_name = "read-bytevector!"; break;
        case WRITEBYTEVECTOR:    m_name = "write-bytevector"; break;
    }
}

// ============================================================================
// Primitive Application
// ============================================================================

Object* PrimitiveIO::apply(IEvaluator* evaluator, Object* args, Environment* env) {
    // Arity checking
    if (!check_arity(args)) {
        return arity_error("");
    }

    // Extract arguments
    Object* x = (args != nil) ? first(args) : nil;
    Object* y = (args != nil && rest(args) != nil) ? second(args) : nil;

    // Dispatch based on Primitive ID
    switch (m_id) {
        // ====================================================================
        // Port Predicates
        // ====================================================================

        case EOFOBJECTQ: {
            // Check if Object is the EOF Object
            // In C++, we check if it's nullptr (EOF indicator)
            return truth(x == nullptr);
        }

        case INPUTPORTQ: {
            // Check if Object is an input port using instance_of()
            if (x == nullptr || x == nil) {
                return truth(false);
            }
            Base* b = dynamic_cast<Base*>(x);
            return truth(b && b->instance_of() == static_cast<int>(Base::atype::INPUTPORT));
        }

        case OUTPUTPORTQ: {
            // Check if Object is an output port using instance_of()
            if (x == nullptr || x == nil) {
                return truth(false);
            }
            Base* b = dynamic_cast<Base*>(x);
            return truth(b && b->instance_of() == static_cast<int>(Base::atype::OUTPUTPORT));
        }

        // ====================================================================
        // Current Ports
        // ====================================================================

        case CURRENTINPUTPORT: {
            // Return the current input port (usually stdin_port)
            return stdin_port;
        }

        case CURRENTOUTPUTPORT: {
            // Return the current output port (usually stdout_port)
            return stdout_port;
        }

        // ====================================================================
        // Input File Operations
        // ====================================================================

        case OPENINPUTFILE: {
            // Open an input file
            // x should be a string containing the filename
            if (!is_string(x)) {
                return error("open-input-file: expected a string filename");
            }
            std::string filename = ((a_string*)x)->value();
            FileInputPort* port = gc_new<FileInputPort>(filename);
            if (!port->open()) {
                return error("open-input-file: could not open file: " + filename);
            }
            return port;
        }

        case CLOSEINPUTPORT: {
            // Close an input port
            InputPort* port = dynamic_cast<InputPort*>(x);
            if (!port) {
                return error("close-input-port: expected an input port");
            }
            port->close();
            return t;
        }

        // ====================================================================
        // Output File Operations
        // ====================================================================

        case OPENOUTPUTFILE: {
            // Open an output file
            // x should be a string containing the filename
            if (!is_string(x)) {
                return error("open-output-file: expected a string filename");
            }
            std::string filename = ((a_string*)x)->value();
            FileOutputPort* port = gc_new<FileOutputPort>(filename);
            if (!port->open()) {
                return error("open-output-file: could not open file: " + filename);
            }
            return port;
        }

        case CLOSEOUTPUTPORT: {
            // Close an output port
            OutputPort* port = dynamic_cast<OutputPort*>(x);
            if (!port) {
                return error("close-output-port: expected an output port");
            }
            // If it's a FileOutputPort, close the file to flush the buffer
            FileOutputPort* file_port = dynamic_cast<FileOutputPort*>(x);
            if (file_port) {
                file_port->close();
            }
            return t;
        }

        // ====================================================================
        // Character Input
        // ====================================================================

        case READCHAR: {
            // Read a character from input port
            // x is optional port argument (defaults to stdin)
            InputPort* port = stdin_port;
            if (x != nullptr && x != nil) {
                port = dynamic_cast<InputPort*>(x);
                if (!port) {
                    return error("read-char: expected an input port");
                }
            }

            char c = port->peek();
            if (port->is_eof()) {
                return nullptr;  // EOF
            }
            // Consume the character by reading a line (simplified)
            port->get_line();
            return gc_new<a_char>(c);
        }

        case PEEKCHAR: {
            // Peek at next character without consuming it
            InputPort* port = stdin_port;
            if (x != nullptr && x != nil) {
                port = dynamic_cast<InputPort*>(x);
                if (!port) {
                    return error("peek-char: expected an input port");
                }
            }

            char c = port->peek();
            if (port->is_eof()) {
                return nullptr;  // EOF
            }
            return gc_new<a_char>(c);
        }

        case CHARREADYQ: {
            // Check if input is available without blocking
            // Optional port argument (defaults to stdin)
            InputPort* port = stdin_port;
            if (x != nullptr && x != nil) {
                port = dynamic_cast<InputPort*>(x);
                if (!port) {
                    return error("char-ready?: expected an input port");
                }
            }

            // R5RS allows this to always return #t if we can't check for available input
            // For file ports, there's always input ready (or EOF)
            // For stdin, we don't have true non-blocking I/O, so return #t
            return t;
        }

        // ====================================================================
        // S-expression Input
        // ====================================================================

        case READ: {
            // Read an S-expression from input port
            InputPort* port = stdin_port;
            if (x != nullptr && x != nil) {
                port = dynamic_cast<InputPort*>(x);
                if (!port) {
                    return error("read: expected an input port");
                }
            }

            Object* result = port->read();
            return result ? result : nullptr;
        }

        // ====================================================================
        // File Loading
        // ====================================================================

        case LOAD: {
            // Load and evaluate a Scheme file
            // (load "filename.scm")
            if (!is_string(x)) {
                return error("load: expected a string filename");
            }

            std::string filename = ((a_string*)x)->value();

            // Open and read the entire file
            std::ifstream file(filename);
            if (!file.is_open()) {
                return error("load: could not open file: " + filename);
            }

            // Read entire file contents
            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();
            std::string contents = buffer.str();

            if (contents.empty()) {
                return nil;
            }

            // Use scheme_reader to parse the file contents
            scheme_reader reader;
            Object* result = nil;

            // Parse and evaluate each top-level expression
            size_t pos = 0;
            while (pos < contents.length()) {
                // Skip whitespace
                while (pos < contents.length() && std::isspace(contents[pos])) {
                    pos++;
                }

                if (pos >= contents.length()) {
                    break;
                }

                // Extract one S-expression
                // We need to read one complete S-expression at a time
                size_t start = pos;
                int paren_depth = 0;
                bool in_string = false;
                bool escape = false;

                while (pos < contents.length()) {
                    char c = contents[pos];

                    if (escape) {
                        escape = false;
                        pos++;
                        continue;
                    }

                    if (c == '\\' && in_string) {
                        escape = true;
                        pos++;
                        continue;
                    }

                    if (c == '"') {
                        in_string = !in_string;
                    }

                    if (!in_string) {
                        if (c == '(') paren_depth++;
                        if (c == ')') paren_depth--;

                        // If this is an atom (not starting with '('), read until whitespace
                        if (start == pos && c != '(' && c != ')') {
                            while (pos < contents.length() &&
                                   !std::isspace(contents[pos]) &&
                                   contents[pos] != '(' &&
                                   contents[pos] != ')') {
                                pos++;
                            }
                            break;
                        }
                    }

                    pos++;

                    // Complete S-expression found
                    if (!in_string && paren_depth == 0 && pos > start) {
                        break;
                    }
                }

                std::string expr_str = contents.substr(start, pos - start);
                if (expr_str.empty()) {
                    continue;
                }

                // Parse and evaluate this expression
                Object* expr = reader.read_from_string(expr_str);
                if (expr) {
                    result = evaluator->eval(expr, env);
                }
            }

            return result;
        }

        // ====================================================================
        // Output
        // ====================================================================

        case WRITE: {
            // Write an Object to output port (with quotes for strings)
            // x is the Object to write
            // y is optional output port (defaults to stdout)
            OutputPort* port = stdout_port;
            if (y != nullptr && y != nil) {
                port = dynamic_cast<OutputPort*>(y);
                if (!port) {
                    return error("write: expected an output port");
                }
            }

            port->write(x);
            return x;
        }

        case DISPLAY: {
            // Display an Object to output port (without quotes for strings)
            // x is the Object to display
            // y is optional output port (defaults to stdout)
            OutputPort* port = stdout_port;
            if (y != nullptr && y != nil) {
                port = dynamic_cast<OutputPort*>(y);
                if (!port) {
                    return error("display: expected an output port");
                }
            }

            port->write(x);
            return x;
        }

        case NEWLINE: {
            // Write a newline to output port
            // x is optional output port (defaults to stdout)
            OutputPort* port = stdout_port;
            if (x != nullptr && x != nil) {
                port = dynamic_cast<OutputPort*>(x);
                if (!port) {
                    return error("newline: expected an output port");
                }
            }

            port->new_line();
            return t;
        }

        case WRITECHAR: {
            // (write-char char [port]) - writes a single character to output port
            Object* char_obj = x;
            Object* port_obj = (rest(args) != nil) ? second(args) : stdout_port;

            if (!is_char(char_obj)) {
                return error("write-char: first argument must be a character");
            }

            char ch = ((a_char*)char_obj)->value();

            // If port is specified, use it; otherwise use stdout
            OutputPort* port = dynamic_cast<OutputPort*>(port_obj);
            if (port) {
                port->write_char(ch);
            } else {
                std::cout << ch;
            }

            // Returns unspecified value (we return the character)
            return char_obj;
        }

        // ====================================================================
        // Environment
        // ====================================================================

        case THEENVIRONMENT: {
            // (the-environment) - returns the current environment as a first-class Object
            // This is called during evaluation, so env parameter contains the current environment
            return (Object*)env;
        }

        // ====================================================================
        // R7RS I/O Operations
        // ====================================================================

        case READLINE: {
            // (read-line) or (read-line port)
            // Reads a line from the input port, excluding the newline character
            InputPort* port = stdin_port;
            if (x != nullptr && x != nil) {
                port = dynamic_cast<InputPort*>(x);
                if (!port) {
                    return error("read-line: expected an input port");
                }
            }

            std::string line = port->get_line();
            // Only return EOF if the line is empty AND we're at EOF
            // (if we read a line successfully, return it even if we're now at EOF)
            if (line.empty() && port->is_eof()) {
                return nullptr;  // EOF Object
            }
            return gc_new<a_string>(line);
        }

        case READSTRING: {
            // (read-string k) or (read-string k port)
            // Reads up to k characters from the input port
            if (!is_int(x)) {
                return error("read-string: first argument must be an integer");
            }
            int k = (int)num(x);
            if (k < 0) {
                return error("read-string: count must be non-negative");
            }

            InputPort* port = stdin_port;
            if (y != nullptr && y != nil) {
                port = dynamic_cast<InputPort*>(y);
                if (!port) {
                    return error("read-string: expected an input port");
                }
            }

            std::string result;
            result.reserve(k);
            for (int i = 0; i < k; i++) {
                if (port->is_eof()) {
                    // EOF reached before reading k characters
                    if (result.empty()) {
                        return nullptr;  // EOF Object if no characters read
                    }
                    break;
                }
                char c = port->get();
                if (port->is_eof()) {
                    // Check again after get() in case we just hit EOF
                    if (result.empty()) {
                        return nullptr;
                    }
                    break;
                }
                result += c;
            }

            return gc_new<a_string>(result);
        }

        case WRITESTRING: {
            // (write-string string) or (write-string string port)
            // Writes the string to the output port
            if (!is_string(x)) {
                return error("write-string: first argument must be a string");
            }

            a_string* str = dynamic_cast<a_string*>(x);
            if (!str) {
                return error("write-string: first argument must be a string");
            }

            OutputPort* port = stdout_port;
            if (y != nullptr && y != nil) {
                port = dynamic_cast<OutputPort*>(y);
                if (!port) {
                    return error("write-string: expected an output port");
                }
            }

            port->write(str->value());
            return nil;  // write-string returns unspecified
        }

        case FLUSHOUTPUT: {
            // (flush-output-port) or (flush-output-port port)
            // Flushes the output port
            // Note: R7RS says if no port is given, flush current output port
            // We use stdout_port as the default
            OutputPort* port = stdout_port;
            if (x != nullptr && x != nil) {
                port = dynamic_cast<OutputPort*>(x);
                if (!port) {
                    return error("flush-output-port: expected an output port");
                }
            }

            // OutputPort::write uses operator<< which doesn't auto-flush
            // We need to manually flush - since we don't have direct stream access,
            // we can write a newline or use some other mechanism
            // For now, just return nil - most output will be flushed on close anyway
            return nil;  // flush-output-port returns unspecified
        }


    // ====================================================================
    // R7RS String Ports
    // ====================================================================

    case OPENINPUTSTRING: {
        // (open-input-string string)
        // Creates an input port that reads from the given string
        if (!is_string(x)) {
            return error("open-input-string: expected a string");
        }
        a_string* str = dynamic_cast<a_string*>(x);
        if (!str) {
            return error("open-input-string: expected a string");
        }
        
        StringInputPort* port = gc_new<StringInputPort>(str->value());
        return port;
    }

    case OPENOUTPUTSTRING: {
        // (open-output-string)
        // Creates an output port that accumulates output into a string
        // No arguments
        StringOutputPort* port = gc_new<StringOutputPort>();
        return port;
    }

    case GETOUTPUTSTRING: {
        // (get-output-string port)
        // Retrieves the accumulated string from an output string port
        StringOutputPort* port = dynamic_cast<StringOutputPort*>(x);
        if (!port) {
            return error("get-output-string: expected a string output port");
        }
        
        std::string content = port->get_string();
        return gc_new<a_string>(content);
    }


    // ====================================================================
    // R7RS Bytevector I/O
    // ====================================================================

    case READU8: {
        // (read-u8) or (read-u8 port)
        // Reads a byte from the input port and returns it as an integer 0-255
        // Returns EOF Object at end of file
        InputPort* port = stdin_port;
        if (x != nullptr && x != nil) {
            port = dynamic_cast<InputPort*>(x);
            if (!port) {
                return error("read-u8: expected an input port");
            }
        }

        if (port->is_eof()) {
            return nullptr;  // EOF Object
        }

        char byte = port->get();
        if (port->is_eof()) {
            return nullptr;  // EOF Object
        }

        // Return byte as unsigned integer 0-255
        return gc_new<atom<long long>>((unsigned char)byte);
    }

    case PEEKU8: {
        // (peek-u8) or (peek-u8 port)
        // Peeks at the next byte without consuming it
        // Returns EOF Object at end of file
        InputPort* port = stdin_port;
        if (x != nullptr && x != nil) {
            port = dynamic_cast<InputPort*>(x);
            if (!port) {
                return error("peek-u8: expected an input port");
            }
        }

        if (port->is_eof()) {
            return nullptr;  // EOF Object
        }

        char byte = port->peek();
        if (port->is_eof() || byte == EOF) {
            return nullptr;  // EOF Object
        }

        // Return byte as unsigned integer 0-255
        return gc_new<atom<long long>>((unsigned char)byte);
    }

    case U8READYQ: {
        // (u8-ready?) or (u8-ready? port)
        // Returns #t if a byte is ready to be read
        // For now, always return #t (data is assumed ready)
        InputPort* port = stdin_port;
        if (x != nullptr && x != nil) {
            port = dynamic_cast<InputPort*>(x);
            if (!port) {
                return error("u8-ready?: expected an input port");
            }
        }

        // Check if we're at EOF
        if (port->is_eof()) {
            return t;  // EOF is "ready" (will return EOF on read)
        }

        // For file/string ports, data is always ready
        return t;
    }

    case WRITEU8: {
        // (write-u8 byte) or (write-u8 byte port)
        // Writes a byte (0-255) to the output port
        if (x == nullptr) {
            return error("write-u8: received nullptr (EOF Object)");
        }
        if (x == nil) {
            return error("write-u8: received nil");
        }
        // Accept both INT and LONG types (read-u8 returns LONG)
        int type = x->get_type();
        if (type != static_cast<int>(Type::INT) && type != static_cast<int>(Type::LONG)) {
            std::string err_msg = "write-u8: expected an integer byte value, got type " + std::to_string(type);
            return error(err_msg);
        }

        int byte_val = (int)num(x);
        if (byte_val < 0 || byte_val > 255) {
            return error("write-u8: byte value must be 0-255");
        }

        OutputPort* port = stdout_port;
        if (y != nullptr && y != nil) {
            port = dynamic_cast<OutputPort*>(y);
            if (!port) {
                return error("write-u8: expected an output port");
            }
        }

        port->write_char((char)(unsigned char)byte_val);
        return nil;  // write-u8 returns unspecified
    }

    case READBYTEVECTOR: {
        // (read-bytevector k) or (read-bytevector k port)
        // Reads up to k bytes from port into a new bytevector
        if (!is_int(x)) {
            return error("read-bytevector: first argument must be an integer");
        }

        int k = (int)num(x);
        if (k < 0) {
            return error("read-bytevector: k must be non-negative");
        }

        InputPort* port = stdin_port;
        if (y != nullptr && y != nil) {
            port = dynamic_cast<InputPort*>(y);
            if (!port) {
                return error("read-bytevector: expected an input port");
            }
        }

        // Read up to k bytes
        std::vector<uint8_t> bytes;
        for (int i = 0; i < k; i++) {
            if (port->is_eof()) {
                break;
            }
            char byte = port->get();
            if (port->is_eof()) {
                break;
            }
            bytes.push_back((unsigned char)byte);
        }

        // Return EOF if no bytes were read
        if (bytes.empty() && k > 0) {
            return nullptr;  // EOF Object
        }

        return gc_new<a_bytevector>(bytes);
    }

    case READBYTEVECTORBANG: {
        // (read-bytevector! bytevector) or (read-bytevector! bytevector port [start [end]])
        // Reads bytes into existing bytevector, returns number of bytes read
        if (!is_bytevector(x)) {
            return error("read-bytevector!: first argument must be a bytevector");
        }

        a_bytevector* bv = dynamic_cast<a_bytevector*>(x);
        if (!bv) {
            return error("read-bytevector!: invalid bytevector");
        }

        // Get port (second argument)
        InputPort* port = stdin_port;
        Object* third = nullptr;
        Object* fourth = nullptr;

        if (y != nullptr && y != nil) {
            port = dynamic_cast<InputPort*>(y);
            if (!port) {
                return error("read-bytevector!: second argument must be an input port");
            }
            // Get start and end if provided
            Object* arg_tail = rest(args);  // Points to second arg
            if (arg_tail != nil) {
                arg_tail = rest(arg_tail);  // Points to third arg (if exists)
                if (arg_tail != nil) {
                    third = first(arg_tail);
                    arg_tail = rest(arg_tail);  // Points to fourth arg (if exists)
                    if (arg_tail != nil) {
                        fourth = first(arg_tail);
                    }
                }
            }
        }

        int start = 0;
        int end = bv->length();

        if (third != nullptr && third != nil) {
            if (!is_int(third)) {
                return error("read-bytevector!: start must be an integer");
            }
            start = (int)num(third);
        }

        if (fourth != nullptr && fourth != nil) {
            if (!is_int(fourth)) {
                return error("read-bytevector!: end must be an integer");
            }
            end = (int)num(fourth);
        }

        if (start < 0 || start > bv->length()) {
            return error("read-bytevector!: start index out of range");
        }
        if (end < start || end > bv->length()) {
            return error("read-bytevector!: end index out of range");
        }

        // Read bytes into bytevector
        int bytes_read = 0;
        for (int i = start; i < end; i++) {
            if (port->is_eof()) {
                break;
            }
            char byte = port->get();
            if (port->is_eof()) {
                break;
            }
            bv->set(i, (unsigned char)byte);
            bytes_read++;
        }

        // Return EOF if no bytes were read
        if (bytes_read == 0 && end > start) {
            return nullptr;  // EOF Object
        }

        return gc_new<atom<long long>>(bytes_read);
    }

    case WRITEBYTEVECTOR: {
        // (write-bytevector bytevector) or (write-bytevector bytevector port [start [end]])
        // Writes bytevector (or slice) to port
        if (!is_bytevector(x)) {
            return error("write-bytevector: first argument must be a bytevector");
        }

        a_bytevector* bv = dynamic_cast<a_bytevector*>(x);
        if (!bv) {
            return error("write-bytevector: invalid bytevector");
        }

        // Get port (second argument)
        OutputPort* port = stdout_port;
        Object* third = nullptr;
        Object* fourth = nullptr;

        if (y != nullptr && y != nil) {
            port = dynamic_cast<OutputPort*>(y);
            if (!port) {
                return error("write-bytevector: second argument must be an output port");
            }
            // Get start and end if provided
            Object* arg_tail = rest(args);  // Points to second arg
            if (arg_tail != nil) {
                arg_tail = rest(arg_tail);  // Points to third arg (if exists)
                if (arg_tail != nil) {
                    third = first(arg_tail);
                    arg_tail = rest(arg_tail);  // Points to fourth arg (if exists)
                    if (arg_tail != nil) {
                        fourth = first(arg_tail);
                    }
                }
            }
        }

        int start = 0;
        int end = bv->length();

        if (third != nullptr && third != nil) {
            if (!is_int(third)) {
                return error("write-bytevector: start must be an integer");
            }
            start = (int)num(third);
        }

        if (fourth != nullptr && fourth != nil) {
            if (!is_int(fourth)) {
                return error("write-bytevector: end must be an integer");
            }
            end = (int)num(fourth);
        }

        if (start < 0 || start > bv->length()) {
            return error("write-bytevector: start index out of range");
        }
        if (end < start || end > bv->length()) {
            return error("write-bytevector: end index out of range");
        }

        // Write bytes to port
        for (int i = start; i < end; i++) {
            port->write_char((char)bv->get(i));
        }

        return nil;  // write-bytevector returns unspecified
    }


        default:
            return error("Unknown I/O Primitive");
    }
}

// ============================================================================
// Registration
// ============================================================================

void PrimitiveIO::install(Environment* env) {
    // GC Safety: Use macro to protect name string before allocating primitive
    #define DEF(name, ...) do { \
        gc_root<a_string> _n(gc_new<a_string>(name)); \
        env->define(_n.get(), gc_new<PrimitiveIO>(__VA_ARGS__)); \
    } while(0)

    // Port predicates
    DEF("eof-Object?",  EOFOBJECTQ, 1, 1);
    DEF("input-port?",  INPUTPORTQ, 1, 1);
    DEF("output-port?", OUTPUTPORTQ, 1, 1);

    // Current ports
    DEF("current-input-port",  CURRENTINPUTPORT, 0, 0);
    DEF("current-output-port", CURRENTOUTPUTPORT, 0, 0);

    // Input file operations
    DEF("open-input-file",  OPENINPUTFILE, 1, 1);
    DEF("close-input-port", CLOSEINPUTPORT, 1, 1);

    // Output file operations
    DEF("open-output-file",  OPENOUTPUTFILE, 1, 1);
    DEF("close-output-port", CLOSEOUTPUTPORT, 1, 1);

    // Character input
    DEF("read-char",   READCHAR, 0, 1);
    DEF("peek-char",   PEEKCHAR, 0, 1);
    DEF("char-ready?", CHARREADYQ, 0, 1);

    // S-expression input
    DEF("read", READ, 0, 1);

    // File loading
    DEF("load", LOAD, 1, 1);

    // Output
    DEF("write",      WRITE, 1, 2);
    DEF("display",    DISPLAY, 1, 2);
    DEF("newline",    NEWLINE, 0, 1);
    DEF("write-char", WRITECHAR, 1, 2);

    // Environment
    DEF("the-environment", THEENVIRONMENT, 0, 0);

    // R7RS additions
    DEF("read-line",         READLINE, 0, 1);
    DEF("read-string",       READSTRING, 1, 2);
    DEF("write-string",      WRITESTRING, 1, 2);
    DEF("flush-output-port", FLUSHOUTPUT, 0, 1);

    // R7RS string ports
    DEF("open-input-string",  OPENINPUTSTRING, 1, 1);
    DEF("open-output-string", OPENOUTPUTSTRING, 0, 0);
    DEF("get-output-string",  GETOUTPUTSTRING, 1, 1);

    // R7RS bytevector I/O
    DEF("read-u8",           READU8, 0, 1);
    DEF("peek-u8",           PEEKU8, 0, 1);
    DEF("u8-ready?",         U8READYQ, 0, 1);
    DEF("write-u8",          WRITEU8, 1, 2);
    DEF("read-bytevector",   READBYTEVECTOR, 1, 2);
    DEF("read-bytevector!",  READBYTEVECTORBANG, 1, 4);
    DEF("write-bytevector",  WRITEBYTEVECTOR, 1, 4);

    #undef DEF
}
