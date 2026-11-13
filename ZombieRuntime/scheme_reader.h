#pragma once

#include "Object.h"
#include <string>

namespace zombie { namespace runtime {

/**
 * scheme_reader - Standalone string → cons list parser
 *
 * Converts Scheme source text into cons-list representation.
 * Agnostic to expression types - just produces cons structures.
 *
 * This enables:
 * - Multiple front-ends (s-expressions, other syntaxes) → same IR
 * - Separation of parsing from evaluation
 * - Easier Macro expansion (operate on cons lists)
 *
 * Grammar (simplified):
 *   expr   ::= atom | list | quoted
 *   list   ::= '(' expr* ')'
 *   quoted ::= "'" expr
 *   atom   ::= number | string | symbol
 *
 * Error handling: Returns ERROR Object on parse failure (no exceptions)
 */
class scheme_reader {
public:
    scheme_reader();

    /**
     * Parse a single expression from source string.
     * Returns: Object* (cons list, atom, or ERROR)
     */
    Object* read_from_string(const std::string& source);

    /**
     * Parse multiple expressions (program).
     * Returns: list of expressions, or ERROR
     */
    Object* read_program_from_string(const std::string& source);

private:
    const char* m_pos;
    const char* m_end;
    int m_line;
    int m_column;

    // Lexical analysis
    void skip_whitespace();
    void skip_comment();  // ; to end of line
    char peek();
    char consume();
    bool at_end();

    // Parsing
    Object* read_expr();
    Object* read_list();       // ( ... )
    Object* read_quoted();     // 'expr
    Object* read_quasiquote(); // `expr
    Object* read_unquote();    // ,expr or ,@expr
    Object* read_atom();
    Object* read_number();
    Object* read_string();     // "..."
    Object* read_symbol();
    Object* read_char();       // #\a character literals

    // Error reporting
    Object* make_error(const std::string& message);

    // Helpers
    bool is_delimiter(char c);
    bool is_digit(char c);
    bool is_symbol_start(char c);
    bool is_symbol_char(char c);
};

} } // namespace zombie::runtime
