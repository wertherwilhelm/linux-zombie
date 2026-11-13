#include "pch.h"
// InputPort.cpp: implementation of the InputPort class.
//
//////////////////////////////////////////////////////////////////////

#include "InputPort.h"
#include "cons_cell.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include "gc_context.h"
#include <algorithm>

namespace zombie { namespace runtime {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

// Global stdin port - initialized lazily via init_standard_ports()
// to ensure GC is available
InputPort* stdin_port = nullptr;

void init_stdin_port() {
    if (stdin_port) return;  // Already initialized

    stdin_port = gc_new<InputPort>(std::cin);

    // Register as persistent root (never collect)
    if (auto* gc = gc_context::current()) {
        gc->register_persistent_root(reinterpret_cast<gc_object**>(&stdin_port));
    }
}

InputPort::InputPort(std::string str)
: m_port(std::cin), m_is_eof(false)
{

}

InputPort::InputPort()
: m_port(std::cin), m_is_eof(false)
{

}

InputPort::InputPort(std::istream& port)
: m_port(port), m_is_eof(false)
{

}

InputPort::~InputPort()
{

}

bool InputPort::open()
{
    return true;
}

bool InputPort::close()
{
    return true;
}

bool InputPort::is_eof()
{
    return m_port.eof();
}

char InputPort::peek()
{
    return static_cast<char>(m_port.peek());
}

char InputPort::get()
{
    return static_cast<char>(m_port.get());
}

std::string InputPort::get_line()
{
    std::string line;
    std::getline(m_port, line);
    return line;
}

Object* InputPort::read()
{
    std::string line = get_line();
    if (line.empty() && is_eof()) {
        return nullptr;
    }

    std::list<std::string> tokens = tokenize(line);
    if (tokens.empty()) {
        return nullptr;
    }

    return parse(tokens);
}

// Tokenize a string into a list of tokens
std::list<std::string> InputPort::tokenize(const std::string& str)
{
    std::list<std::string> tokens;
    std::string::const_iterator it = str.begin();

    while (it != str.end()) {
        // Skip whitespace
        while (it != str.end() && *it == ' ')
            ++it;

        if (it == str.end())
            break;

        // Handle parentheses
        if (*it == '(' || *it == ')') {
            tokens.emplace_back((*it == '(') ? "(" : ")");
            ++it;
        }
        // Handle comments
        else if (*it == ';') {
            if (it + 1 != str.end() && *(it + 1) == ';') {
                // Skip to end of line
                it = std::find_if(it, str.end(), [](char c) { return c == '\n' || c == '\r'; });
                if (it != str.end())
                    ++it;
            }
        }
        // Handle quoted strings
        else if (*it == '"') {
            ++it; // Skip opening quote
            auto start = it;
            while (it != str.end() && *it != '"') {
                if (*it == '\\' && it + 1 != str.end())
                    ++it; // Skip escaped character
                ++it;
            }
            tokens.emplace_back(start, it);
            if (it != str.end())
                ++it; // Skip closing quote
        }
        // Handle regular tokens
        else {
            auto t = std::find_if(it, str.end(), [](char c) { return c == ' ' || c == '(' || c == ')'; });
            tokens.emplace_back(it, t);
            it = t;
        }
    }

    return tokens;
}

// Parse tokens into an Object tree
Object* InputPort::parse(std::list<std::string>& tokens)
{
    if (tokens.empty()) {
        return nullptr;
    }

    const std::string token(tokens.front());
    tokens.pop_front();

    if (token == "(") {
        ConsCell* c = gc_new<ConsCell>();

        while (!tokens.empty() && tokens.front() != ")") {
            Object* l = parse(tokens);

            if (car(c)->is_nil()) {
                c->car(l);
            }
            else {
                c = (ConsCell*)append(c, l);
            }
        }

        if (!tokens.empty())
            tokens.pop_front(); // Remove closing ')'

        return c;
    }
    else if (token == ")") {
        throw std::runtime_error("Unexpected closing parenthesis");
    }
    else {
        // Try to parse as number
        std::istringstream token_stream(token);
        int int_value;
        double double_value;

        if (token_stream >> int_value && token_stream.eof()) {
            return gc_new<a_double>(int_value);
        }
        else if (token_stream.clear(), token_stream.str(token), token_stream >> double_value && token_stream.eof()) {
            return gc_new<a_double>(double_value);
        }
        else {
            // Return as string/symbol
            return gc_new<a_string>(token);
        }
    }
}

} } // namespace zombie::runtime
