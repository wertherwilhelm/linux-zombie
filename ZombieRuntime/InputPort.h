#pragma once

#include <iostream>
#include "Base.h"
#include <string>
#include <list>
#include <sstream>

namespace zombie { namespace runtime {

/**
 * @file InputPort.h
 * @brief Input port for reading Scheme source and character streams.
 */

/**
 * @brief Input port for Scheme reader operations.
 *
 * Wraps a std::istream and provides character and S-expression reading helpers.
 * Parsing/tokenizing is currently built-in but intended to be refactored into
 * dedicated tokenizer/parser classes.
 */
class InputPort : public Base
{
public:
    /**
     * @brief Construct input port bound to std::cin.
     */
    InputPort();

    /**
     * @brief Construct input port from a string buffer.
     * @param str Source string for the internal stream.
     */
    InputPort(std::string str);

    /**
     * @brief Construct input port bound to an existing stream.
     * @param port Stream to wrap (defaults to std::cin).
     */
    InputPort(std::istream& port = std::cin);

    /**
     * @brief Destructor.
     */
    virtual ~InputPort();

    /**
     * @brief Runtime type identifier for input ports.
     */
    int instance_of() override { return static_cast<int>(Base::atype::INPUTPORT); }

    /**
     * @brief Ports are not cloneable (returns self).
     */
    Object* clone() const override { return const_cast<InputPort*>(this); }  // Ports should not be cloned

    /**
     * @brief Bind to a new input stream.
     * @param port Stream to use (defaults to std::cin).
     */
    void set_stream(std::istream& port = std::cin);

    /**
     * @brief Open the port (if applicable).
     * @return true on success.
     */
    bool open();

    /**
     * @brief Close the port (if applicable).
     * @return true on success.
     */
    bool close();

    /**
     * @brief Check end-of-file state.
     */
    bool is_eof();

    /**
     * @brief Peek next character without consuming it.
     */
    char peek();

    /**
     * @brief Get next character (consuming it).
     */
    char get();  // Get next character (consuming it)

    /**
     * @brief Read a line from the stream.
     */
    std::string get_line();

    /**
     * @brief Read a full S-expression from the input port.
     * @return Parsed Object or error Object on failure.
     */
    Object* read();

    /**
     * @brief Tokenize a string into a list of tokens.
     * @param str Source string to tokenize.
     * @return List of token strings.
     */
    static std::list<std::string> tokenize(const std::string& str);

    /**
     * @brief Parse tokens into an Object tree.
     * @param tokens Token list to parse (consumed as parsing proceeds).
     * @return Parsed Object or error Object on failure.
     */
    static Object* parse(std::list<std::string>& tokens);

private:
    std::istream& m_port;
    bool m_is_eof;
};

extern InputPort *stdin_port;

/**
 * @brief Initialize stdin_port through GC (called by init_lisp_constants).
 */
void init_stdin_port();

} } // namespace zombie::runtime
