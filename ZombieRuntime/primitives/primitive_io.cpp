#include "zombie_runtime_types.h"
#include "primitive_io.h"
#include "../native_primitive.h"
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
#include "../a_exception.h"
#include "../../GarbageCollector/gc_ptr.h"
#include "../a_vector.h"
#include "../procedure.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <iostream>
#include <vector>

/**
 * @file primitive_io.cpp
 * @brief I/O primitives (R7RS 6.13), table-driven.
 */

namespace {

using namespace zombie::runtime;

// --- `write` datum serialization (quoted strings, #\ chars, recursive) ---
void write_datum(std::ostream& os, Object* o);

void write_string_literal(std::ostream& os, const std::string& s) {
    os << '"';
    for (char c : s) {
        switch (c) {
            case '"':  os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\n': os << "\\n";  break;
            case '\r': os << "\\r";  break;
            case '\t': os << "\\t";  break;
            default:   os << c;      break;
        }
    }
    os << '"';
}

void write_char_literal(std::ostream& os, char c) {
    os << "#\\";
    switch (c) {
        case ' ':  os << "space";   break;
        case '\n': os << "newline"; break;
        case '\t': os << "tab";     break;
        case '\r': os << "return";  break;
        case '\0': os << "null";    break;
        default:   os << c;         break;
    }
}

void write_datum(std::ostream& os, Object* o) {
    if (o == nullptr) { os << "#<eof>"; return; }
    if (is_string(o)) { write_string_literal(os, ((a_string*)o)->value()); return; }
    if (is_char(o))   { write_char_literal(os, ((a_char*)o)->value()); return; }
    if (is_vector(o)) {
        a_vector* v = (a_vector*)o;
        os << "#(";
        for (int i = 0; i < v->length(); ++i) { if (i) os << ' '; write_datum(os, v->get(i)); }
        os << ')';
        return;
    }
    if (is_pair(o)) {
        os << '(';
        Object* cur = o;
        bool firstp = true;
        while (is_pair(cur)) {
            if (!firstp) os << ' ';
            firstp = false;
            write_datum(os, first(cur));
            cur = rest(cur);
        }
        if (!cur->is_nil()) { os << " . "; write_datum(os, cur); }
        os << ')';
        return;
    }
    o->print(os);
}

// Argument helpers (match the original's nil-guarded extraction).
inline Object* io_x(Object* a) { return (a != nil) ? first(a) : nil; }
inline Object* io_y(Object* a) { return (a != nil && rest(a) != nil) ? second(a) : nil; }

// Resolve an optional input/output port argument, defaulting to the current port.
InputPort* in_port(Object* arg, const char* who, bool& ok) {
    ok = true;
    if (arg != nullptr && arg != nil) {
        InputPort* p = dynamic_cast<InputPort*>(arg);
        if (!p) { ok = false; return nullptr; }
        return p;
    }
    return stdin_port;
}
OutputPort* out_port(Object* arg, const char* who, bool& ok) {
    ok = true;
    if (arg != nullptr && arg != nil) {
        OutputPort* p = dynamic_cast<OutputPort*>(arg);
        if (!p) { ok = false; return nullptr; }
        return p;
    }
    return stdout_port;
}

// ---- Port predicates ----
Object* prim_eof_objectq(IEvaluator*, Object* a, Environment*) { return truth(io_x(a) == nullptr); }
Object* prim_eof_object(IEvaluator*, Object*, Environment*) { return nullptr; }  // EOF sentinel
Object* prim_input_portq(IEvaluator*, Object* a, Environment*) {
    Object* x = io_x(a);
    if (x == nullptr || x == nil) return truth(false);
    Base* b = dynamic_cast<Base*>(x);
    return truth(b && b->instance_of() == static_cast<int>(Base::atype::INPUTPORT));
}
Object* prim_output_portq(IEvaluator*, Object* a, Environment*) {
    Object* x = io_x(a);
    if (x == nullptr || x == nil) return truth(false);
    Base* b = dynamic_cast<Base*>(x);
    return truth(b && b->instance_of() == static_cast<int>(Base::atype::OUTPUTPORT));
}

// ---- Current ports ----
Object* prim_current_input_port(IEvaluator*, Object*, Environment*) { return stdin_port; }
Object* prim_current_output_port(IEvaluator*, Object*, Environment*) { return stdout_port; }

// ---- File ports ----
Object* prim_open_input_file(IEvaluator*, Object* a, Environment*) {
    Object* x = io_x(a);
    if (!is_string(x)) return error("open-input-file: expected a string filename");
    std::string filename = ((a_string*)x)->value();
    FileInputPort* port = gc_new<FileInputPort>(filename);
    if (!port->open()) return error("open-input-file: could not open file: " + filename);
    return port;
}
Object* prim_close_input_port(IEvaluator*, Object* a, Environment*) {
    InputPort* port = dynamic_cast<InputPort*>(io_x(a));
    if (!port) return error("close-input-port: expected an input port");
    port->close();
    return t;
}
Object* prim_open_output_file(IEvaluator*, Object* a, Environment*) {
    Object* x = io_x(a);
    if (!is_string(x)) return error("open-output-file: expected a string filename");
    std::string filename = ((a_string*)x)->value();
    FileOutputPort* port = gc_new<FileOutputPort>(filename);
    if (!port->open()) return error("open-output-file: could not open file: " + filename);
    return port;
}
Object* prim_close_output_port(IEvaluator*, Object* a, Environment*) {
    Object* x = io_x(a);
    OutputPort* port = dynamic_cast<OutputPort*>(x);
    if (!port) return error("close-output-port: expected an output port");
    FileOutputPort* file_port = dynamic_cast<FileOutputPort*>(x);
    if (file_port) file_port->close();
    return t;
}

// ---- Character input ----
Object* prim_read_char(IEvaluator*, Object* a, Environment*) {
    bool ok; InputPort* port = in_port(io_x(a), "read-char", ok);
    if (!ok) return error("read-char: expected an input port");
    char c = port->peek();
    if (port->is_eof()) return nullptr;
    port->get();  // consume exactly one character
    return gc_new<a_char>(c);
}
Object* prim_peek_char(IEvaluator*, Object* a, Environment*) {
    bool ok; InputPort* port = in_port(io_x(a), "peek-char", ok);
    if (!ok) return error("peek-char: expected an input port");
    char c = port->peek();
    if (port->is_eof()) return nullptr;
    return gc_new<a_char>(c);
}
Object* prim_char_readyq(IEvaluator*, Object* a, Environment*) {
    bool ok; in_port(io_x(a), "char-ready?", ok);
    if (!ok) return error("char-ready?: expected an input port");
    return t;  // no non-blocking check; always ready (or EOF)
}

// ---- S-expression input ----
Object* prim_read(IEvaluator*, Object* a, Environment*) {
    bool ok; InputPort* port = in_port(io_x(a), "read", ok);
    if (!ok) return error("read: expected an input port");
    Object* result = port->read();
    return result ? result : nullptr;
}

// ---- File loading ----
Object* prim_load(IEvaluator* evaluator, Object* a, Environment* env) {
    Object* x = io_x(a);
    if (!is_string(x)) return error("load: expected a string filename");
    std::string filename = ((a_string*)x)->value();
    std::ifstream file(filename);
    if (!file.is_open()) return error("load: could not open file: " + filename);
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    std::string contents = buffer.str();
    if (contents.empty()) return nil;
    // Real reader: understands comments, char literals, block comments.
    scheme_reader reader;
    Object* program = reader.read_program_from_string(contents);
    if (is_read_exception(program)) return error("load: " + static_cast<a_exception*>(program)->message());
    gc_root<Object> guard_program(program);  // keep AST alive across eval
    Object* result = nil;
    for (Object* f = program; f && !f->is_nil(); f = cdr(f)) {
        result = evaluator->eval(car(f), env);
    }
    return result;
}

// ---- Output ----
Object* prim_write(IEvaluator*, Object* a, Environment*) {
    Object* x = io_x(a);
    bool ok; OutputPort* port = out_port(io_y(a), "write", ok);
    if (!ok) return error("write: expected an output port");
    std::ostringstream oss;
    write_datum(oss, x);
    port->write(oss.str());
    return x;
}
Object* prim_display(IEvaluator*, Object* a, Environment*) {
    Object* x = io_x(a);
    bool ok; OutputPort* port = out_port(io_y(a), "display", ok);
    if (!ok) return error("display: expected an output port");
    port->write(x);
    return x;
}
// (with-output-to-string thunk) — rebind the default output port to a string port.
Object* prim_with_output_to_string(IEvaluator* evaluator, Object* a, Environment* env) {
    Procedure* thunk = dynamic_cast<Procedure*>(io_x(a));
    if (!thunk) return error("with-output-to-string: argument must be a procedure");
    StringOutputPort* sp = gc_new<StringOutputPort>();
    gc_root<Object> spguard(sp);
    OutputPort* saved = stdout_port;
    stdout_port = sp;
    try { thunk->apply(evaluator, nil, env); }
    catch (...) { stdout_port = saved; throw; }
    stdout_port = saved;
    return gc_new<a_string>(sp->get_string());
}
// (call-with-output-string proc) — call proc on a fresh string port.
Object* prim_call_with_output_string(IEvaluator* evaluator, Object* a, Environment* env) {
    Procedure* proc = dynamic_cast<Procedure*>(io_x(a));
    if (!proc) return error("call-with-output-string: argument must be a procedure");
    StringOutputPort* sp = gc_new<StringOutputPort>();
    gc_root<Object> spguard(sp);
    proc->apply(evaluator, cons(sp, nil), env);
    return gc_new<a_string>(sp->get_string());
}
Object* prim_newline(IEvaluator*, Object* a, Environment*) {
    bool ok; OutputPort* port = out_port(io_x(a), "newline", ok);
    if (!ok) return error("newline: expected an output port");
    port->new_line();
    return t;
}
Object* prim_write_char(IEvaluator*, Object* a, Environment*) {
    Object* char_obj = io_x(a);
    Object* port_obj = (a != nil && rest(a) != nil) ? second(a) : stdout_port;
    if (!is_char(char_obj)) return error("write-char: first argument must be a character");
    char ch = ((a_char*)char_obj)->value();
    OutputPort* port = dynamic_cast<OutputPort*>(port_obj);
    if (port) port->write_char(ch);
    else std::cout << ch;
    return char_obj;
}

// ---- Environments ----
Object* prim_the_environment(IEvaluator*, Object*, Environment* env) { return (Object*)env; }
Object* prim_interaction_environment(IEvaluator*, Object*, Environment* env) {
    Environment* e = env;
    while (e && e->parent() != nullptr) e = e->parent();
    return (Object*)(e ? e : env);
}

// ---- R7RS textual I/O ----
Object* prim_read_line(IEvaluator*, Object* a, Environment*) {
    bool ok; InputPort* port = in_port(io_x(a), "read-line", ok);
    if (!ok) return error("read-line: expected an input port");
    std::string line = port->get_line();
    if (line.empty() && port->is_eof()) return nullptr;
    return gc_new<a_string>(line);
}
Object* prim_read_string(IEvaluator*, Object* a, Environment*) {
    Object* x = io_x(a);
    if (!is_int(x)) return error("read-string: first argument must be an integer");
    int k = (int)num(x);
    if (k < 0) return error("read-string: count must be non-negative");
    bool ok; InputPort* port = in_port(io_y(a), "read-string", ok);
    if (!ok) return error("read-string: expected an input port");
    std::string result;
    result.reserve(k);
    for (int i = 0; i < k; i++) {
        if (port->is_eof()) { if (result.empty()) return nullptr; break; }
        char c = port->get();
        if (port->is_eof()) { if (result.empty()) return nullptr; break; }
        result += c;
    }
    return gc_new<a_string>(result);
}
Object* prim_write_string(IEvaluator*, Object* a, Environment*) {
    Object* x = io_x(a);
    if (!is_string(x)) return error("write-string: first argument must be a string");
    a_string* str = dynamic_cast<a_string*>(x);
    if (!str) return error("write-string: first argument must be a string");
    bool ok; OutputPort* port = out_port(io_y(a), "write-string", ok);
    if (!ok) return error("write-string: expected an output port");
    port->write(str->value());
    return nil;
}
Object* prim_flush_output(IEvaluator*, Object* a, Environment*) {
    bool ok; out_port(io_x(a), "flush-output-port", ok);
    if (!ok) return error("flush-output-port: expected an output port");
    return nil;  // most output flushes on close
}

// ---- R7RS string ports ----
Object* prim_open_input_string(IEvaluator*, Object* a, Environment*) {
    Object* x = io_x(a);
    if (!is_string(x)) return error("open-input-string: expected a string");
    a_string* str = dynamic_cast<a_string*>(x);
    if (!str) return error("open-input-string: expected a string");
    return gc_new<StringInputPort>(str->value());
}
Object* prim_open_output_string(IEvaluator*, Object*, Environment*) { return gc_new<StringOutputPort>(); }
Object* prim_get_output_string(IEvaluator*, Object* a, Environment*) {
    StringOutputPort* port = dynamic_cast<StringOutputPort*>(io_x(a));
    if (!port) return error("get-output-string: expected a string output port");
    return gc_new<a_string>(port->get_string());
}

// ---- R7RS bytevector I/O ----
Object* prim_read_u8(IEvaluator*, Object* a, Environment*) {
    bool ok; InputPort* port = in_port(io_x(a), "read-u8", ok);
    if (!ok) return error("read-u8: expected an input port");
    if (port->is_eof()) return nullptr;
    char byte = port->get();
    if (port->is_eof()) return nullptr;
    return gc_new<a_long>((unsigned char)byte);
}
Object* prim_peek_u8(IEvaluator*, Object* a, Environment*) {
    bool ok; InputPort* port = in_port(io_x(a), "peek-u8", ok);
    if (!ok) return error("peek-u8: expected an input port");
    if (port->is_eof()) return nullptr;
    char byte = port->peek();
    if (port->is_eof() || byte == EOF) return nullptr;
    return gc_new<a_long>((unsigned char)byte);
}
Object* prim_u8_readyq(IEvaluator*, Object* a, Environment*) {
    bool ok; in_port(io_x(a), "u8-ready?", ok);
    if (!ok) return error("u8-ready?: expected an input port");
    return t;
}
Object* prim_write_u8(IEvaluator*, Object* a, Environment*) {
    Object* x = io_x(a);
    if (x == nullptr) return error("write-u8: received nullptr (EOF Object)");
    if (x == nil) return error("write-u8: received nil");
    int type = x->get_type();
    if (type != static_cast<int>(Type::INT) && type != static_cast<int>(Type::LONG)) {
        return error("write-u8: expected an integer byte value, got type " + std::to_string(type));
    }
    int byte_val = (int)num(x);
    if (byte_val < 0 || byte_val > 255) return error("write-u8: byte value must be 0-255");
    bool ok; OutputPort* port = out_port(io_y(a), "write-u8", ok);
    if (!ok) return error("write-u8: expected an output port");
    port->write_char((char)(unsigned char)byte_val);
    return nil;
}
Object* prim_read_bytevector(IEvaluator*, Object* a, Environment*) {
    Object* x = io_x(a);
    if (!is_int(x)) return error("read-bytevector: first argument must be an integer");
    int k = (int)num(x);
    if (k < 0) return error("read-bytevector: k must be non-negative");
    bool ok; InputPort* port = in_port(io_y(a), "read-bytevector", ok);
    if (!ok) return error("read-bytevector: expected an input port");
    std::vector<uint8_t> bytes;
    for (int i = 0; i < k; i++) {
        if (port->is_eof()) break;
        char byte = port->get();
        if (port->is_eof()) break;
        bytes.push_back((unsigned char)byte);
    }
    if (bytes.empty() && k > 0) return nullptr;
    return gc_new<a_bytevector>(bytes);
}
Object* prim_read_bytevector_bang(IEvaluator*, Object* a, Environment*) {
    Object* x = io_x(a), *y = io_y(a);
    if (!is_bytevector(x)) return error("read-bytevector!: first argument must be a bytevector");
    a_bytevector* bv = dynamic_cast<a_bytevector*>(x);
    if (!bv) return error("read-bytevector!: invalid bytevector");
    InputPort* port = stdin_port;
    Object* third = nullptr, *fourth = nullptr;
    if (y != nullptr && y != nil) {
        port = dynamic_cast<InputPort*>(y);
        if (!port) return error("read-bytevector!: second argument must be an input port");
        Object* tail = rest(a);
        if (tail != nil) { tail = rest(tail);
            if (tail != nil) { third = first(tail); tail = rest(tail);
                if (tail != nil) fourth = first(tail); } }
    }
    int start = 0, end = bv->length();
    if (third != nullptr && third != nil) { if (!is_int(third)) return error("read-bytevector!: start must be an integer"); start = (int)num(third); }
    if (fourth != nullptr && fourth != nil) { if (!is_int(fourth)) return error("read-bytevector!: end must be an integer"); end = (int)num(fourth); }
    if (start < 0 || start > bv->length()) return error("read-bytevector!: start index out of range");
    if (end < start || end > bv->length()) return error("read-bytevector!: end index out of range");
    int bytes_read = 0;
    for (int i = start; i < end; i++) {
        if (port->is_eof()) break;
        char byte = port->get();
        if (port->is_eof()) break;
        bv->set(i, (unsigned char)byte);
        bytes_read++;
    }
    if (bytes_read == 0 && end > start) return nullptr;
    return gc_new<a_long>(bytes_read);
}
Object* prim_write_bytevector(IEvaluator*, Object* a, Environment*) {
    Object* x = io_x(a), *y = io_y(a);
    if (!is_bytevector(x)) return error("write-bytevector: first argument must be a bytevector");
    a_bytevector* bv = dynamic_cast<a_bytevector*>(x);
    if (!bv) return error("write-bytevector: invalid bytevector");
    OutputPort* port = stdout_port;
    Object* third = nullptr, *fourth = nullptr;
    if (y != nullptr && y != nil) {
        port = dynamic_cast<OutputPort*>(y);
        if (!port) return error("write-bytevector: second argument must be an output port");
        Object* tail = rest(a);
        if (tail != nil) { tail = rest(tail);
            if (tail != nil) { third = first(tail); tail = rest(tail);
                if (tail != nil) fourth = first(tail); } }
    }
    int start = 0, end = bv->length();
    if (third != nullptr && third != nil) { if (!is_int(third)) return error("write-bytevector: start must be an integer"); start = (int)num(third); }
    if (fourth != nullptr && fourth != nil) { if (!is_int(fourth)) return error("write-bytevector: end must be an integer"); end = (int)num(fourth); }
    if (start < 0 || start > bv->length()) return error("write-bytevector: start index out of range");
    if (end < start || end > bv->length()) return error("write-bytevector: end index out of range");
    for (int i = start; i < end; i++) port->write_char((char)bv->get(i));
    return nil;
}

}  // namespace

void PrimitiveIO::install(zombie::runtime::Environment* env) {
    using namespace zombie::runtime;
    static const NativeEntry entries[] = {
        { "eof-object?",  1, 1, prim_eof_objectq },
        { "eof-object",   0, 0, prim_eof_object },
        { "input-port?",  1, 1, prim_input_portq },
        { "output-port?", 1, 1, prim_output_portq },
        { "current-input-port",  0, 0, prim_current_input_port },
        { "current-output-port", 0, 0, prim_current_output_port },
        { "open-input-file",  1, 1, prim_open_input_file },
        { "close-input-port", 1, 1, prim_close_input_port },
        { "open-output-file",  1, 1, prim_open_output_file },
        { "close-output-port", 1, 1, prim_close_output_port },
        { "read-char",   0, 1, prim_read_char },
        { "peek-char",   0, 1, prim_peek_char },
        { "char-ready?", 0, 1, prim_char_readyq },
        { "read", 0, 1, prim_read },
        { "load", 1, 1, prim_load },
        { "write",      1, 2, prim_write },
        { "display",    1, 2, prim_display },
        { "newline",    0, 1, prim_newline },
        { "write-char", 1, 2, prim_write_char },
        { "the-environment", 0, 0, prim_the_environment },
        { "read-line",         0, 1, prim_read_line },
        { "read-string",       1, 2, prim_read_string },
        { "write-string",      1, 2, prim_write_string },
        { "flush-output-port", 0, 1, prim_flush_output },
        { "open-input-string",  1, 1, prim_open_input_string },
        { "open-output-string", 0, 0, prim_open_output_string },
        { "get-output-string",  1, 1, prim_get_output_string },
        { "with-output-to-string", 1, 1, prim_with_output_to_string },
        { "call-with-output-string", 1, 1, prim_call_with_output_string },
        { "interaction-environment", 0, 0, prim_interaction_environment },
        { "read-u8",           0, 1, prim_read_u8 },
        { "peek-u8",           0, 1, prim_peek_u8 },
        { "u8-ready?",         0, 1, prim_u8_readyq },
        { "write-u8",          1, 2, prim_write_u8 },
        { "read-bytevector",   1, 2, prim_read_bytevector },
        { "read-bytevector!",  1, 4, prim_read_bytevector_bang },
        { "write-bytevector",  1, 4, prim_write_bytevector },
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}
