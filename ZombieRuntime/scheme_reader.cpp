#include "zombie_runtime_types.h"
#include "scheme_reader.h"
#include "atom.h"
#include "cons_cell.h"
#include "Base.h"
#include "constants.h"  // For intern_symbol()
#include "numeric_runtime.h"
#include "a_rational.h"
#include "a_complex.h"
#include "a_vector.h"
#include "a_bytevector.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include <cctype>
#include <sstream>

namespace zombie { namespace runtime {

scheme_reader::scheme_reader()
    : m_pos(nullptr), m_end(nullptr), m_line(1), m_column(1) {
}

Object* scheme_reader::read_from_string(const std::string& source) {
    size_t consumed = 0;
    Object* result = read_from_string(source, consumed);
    return result ? result : make_error("read: unexpected end of input");
}

Object* scheme_reader::read_from_string(const std::string& source, size_t& consumed) {
    m_pos = source.c_str();
    m_end = m_pos + source.length();
    m_line = 1;
    m_column = 1;

    skip_whitespace();

    if (at_end()) {
        consumed = source.length();
        return nullptr;  // end of input -> EOF
    }

    Object* result = read_expr();
    consumed = static_cast<size_t>(m_pos - source.c_str());
    return result;
}

Object* scheme_reader::read_program_from_string(const std::string& source) {
    m_pos = source.c_str();
    m_end = m_pos + source.length();
    m_line = 1;
    m_column = 1;

    Object* result = nil;
    Object* tail = nil;
    gc_root<Object> result_guard;  // GC SAFETY: keep the in-progress program list alive
                                   // across allocations in later read_expr() calls.

    while (!at_end()) {
        skip_whitespace();
        if (at_end()) break;

        Object* expr = read_expr();
        if (!expr) {
            return make_error("read: failed to parse expression");
        }

        // Check if it's an error (starts with "ERROR:")
        a_string* err = dynamic_cast<a_string*>(expr);
        if (err && err->value().find("ERROR:") == 0) {
            return expr;  // Propagate error
        }

        // Append to result list
        gc_root<Object> expr_guard(expr);  // protect across cons()
        Object* new_cell = cons(expr, nil);
        if (result == nil) {
            result = new_cell;
            tail = new_cell;
            result_guard = gc_root<Object>(result);  // protect the head henceforth
        } else {
            set_rest(tail, new_cell);
            tail = new_cell;
        }
    }

    return result;
}

void scheme_reader::skip_whitespace() {
    while (!at_end()) {
        char c = peek();
        if (std::isspace(c)) {
            if (c == '\n') {
                m_line++;
                m_column = 1;
            } else {
                m_column++;
            }
            consume();
        } else if (c == ';') {
            skip_comment();
        } else {
            break;
        }
    }
}

void scheme_reader::skip_comment() {
    // Skip ; to end of line
    while (!at_end() && peek() != '\n') {
        consume();
    }
    if (!at_end() && peek() == '\n') {
        consume();
        m_line++;
        m_column = 1;
    }
}

char scheme_reader::peek() {
    if (at_end()) return '\0';
    return *m_pos;
}

char scheme_reader::consume() {
    if (at_end()) return '\0';
    char c = *m_pos++;
    m_column++;
    return c;
}

bool scheme_reader::at_end() {
    return m_pos >= m_end;
}

Object* scheme_reader::read_expr() {
    skip_whitespace();

    if (at_end()) {
        return make_error("read: unexpected end of input");
    }

    char c = peek();

    if (c == '(') {
        return read_list();
    } else if (c == '\'') {
        return read_quoted();
    } else if (c == '`') {
        return read_quasiquote();
    } else if (c == ',') {
        return read_unquote();
    } else if (c == '"') {
        return read_string();
    } else if (c == '#') {
        return read_char();
    } else if (c == ')') {
        return make_error("read: unexpected ')'");
    } else {
        return read_atom();
    }
}

Object* scheme_reader::read_list() {
    consume();  // consume '('

    Object* result = nil;
    Object* tail = nil;
    // GC SAFETY: reading subsequent elements allocates (interned symbols, sub-lists)
    // and can trigger a collection. Without rooting, the in-progress list is swept and
    // the next set_cdr writes into freed memory (heap-use-after-free). `result` is the
    // list head (assigned once); rooting it keeps the whole spine reachable. Each
    // element is rooted across the cons() that appends it.
    gc_root<Object> result_guard;

    while (true) {
        skip_whitespace();

        if (at_end()) {
            return make_error("read: unexpected end of input (unclosed list)");
        }

        if (peek() == ')') {
            consume();  // consume ')'
            return result;
        }

        // Check for dotted pair notation
        if (peek() == '.') {
            consume();  // consume '.'
            skip_whitespace();

            if (tail == nil) {
                return make_error("read: improper use of '.' in list");
            }

            Object* cdr_expr = read_expr();
            if (!cdr_expr) {
                return make_error("read: expected expression after '.'");
            }
            gc_root<Object> cdr_guard(cdr_expr);  // protect across any trailing allocation

            // Check for error (starts with "ERROR:")
            a_string* err = dynamic_cast<a_string*>(cdr_expr);
            if (err && err->value().find("ERROR:") == 0) {
                return cdr_expr;
            }

            set_rest(tail, cdr_expr);

            skip_whitespace();
            if (peek() != ')') {
                return make_error("read: expected ')' after dotted pair");
            }
            consume();
            return result;
        }

        Object* expr = read_expr();
        if (!expr) {
            return make_error("read: failed to read list element");
        }

        // Check for error (starts with "ERROR:")
        a_string* err = dynamic_cast<a_string*>(expr);
        if (err && err->value().find("ERROR:") == 0) {
            return expr;
        }

        gc_root<Object> expr_guard(expr);  // protect expr across the cons() allocation
        Object* new_cell = cons(expr, nil);
        if (result == nil) {
            result = new_cell;
            tail = new_cell;
            result_guard = gc_root<Object>(result);  // protect the list head henceforth
        } else {
            // Use set_cdr_noclone to avoid cloning issues
            ((ConsCell*)tail)->set_cdr_noclone(new_cell);
            tail = new_cell;
        }
    }
}

Object* scheme_reader::read_quoted() {
    consume();  // consume '

    Object* expr = read_expr();
    if (!expr) {
        return make_error("read: expected expression after quote");
    }

    // Check for error (starts with "ERROR:")
    a_string* err = dynamic_cast<a_string*>(expr);
    if (err && err->value().find("ERROR:") == 0) {
        return expr;
    }

    // Protect expr during multiple allocations
    gc_root<Object> guard(expr);

    // Return (quote <expr>)
    return cons(intern_symbol("quote"), cons(expr, nil));
}

Object* scheme_reader::read_quasiquote() {
    consume();  // consume `

    Object* expr = read_expr();
    if (!expr) {
        return make_error("read: expected expression after quasiquote");
    }

    // Check for error
    a_string* err = dynamic_cast<a_string*>(expr);
    if (err && err->value().find("ERROR:") == 0) {
        return expr;
    }

    // Protect expr during multiple allocations
    gc_root<Object> guard(expr);

    // Return (quasiquote <expr>)
    return cons(intern_symbol("quasiquote"), cons(expr, nil));
}

Object* scheme_reader::read_unquote() {
    consume();  // consume ,

    // Check for ,@ (unquote-splicing)
    if (!at_end() && peek() == '@') {
        consume();  // consume @

        Object* expr = read_expr();
        if (!expr) {
            return make_error("read: expected expression after unquote-splicing");
        }

        // Check for error
        a_string* err = dynamic_cast<a_string*>(expr);
        if (err && err->value().find("ERROR:") == 0) {
            return expr;
        }

        // Protect expr during multiple allocations
        gc_root<Object> guard(expr);

        // Return (unquote-splicing <expr>)
        return cons(intern_symbol("unquote-splicing"), cons(expr, nil));
    }

    // Just , (unquote)
    Object* expr = read_expr();
    if (!expr) {
        return make_error("read: expected expression after unquote");
    }

    // Check for error
    a_string* err = dynamic_cast<a_string*>(expr);
    if (err && err->value().find("ERROR:") == 0) {
        return expr;
    }

    // Protect expr during multiple allocations
    gc_root<Object> guard(expr);

    // Return (unquote <expr>)
    return cons(intern_symbol("unquote"), cons(expr, nil));
}

Object* scheme_reader::read_atom() {
    skip_whitespace();

    char c = peek();

    // Check for number
    if (is_digit(c) || (c == '-' && !at_end() && is_digit(*(m_pos + 1)))) {
        return read_number();
    }

    // Otherwise it's a symbol
    return read_symbol();
}

Object* scheme_reader::parse_number_token(const std::string& tok, int radix, int exactness) {
    if (tok.empty()) {
        return make_error("read: empty numeric literal after '#' prefix");
    }
    Object* val = nullptr;
    size_t slash = tok.find('/');
    try {
        if (slash != std::string::npos) {
            int64_t n = std::stoll(tok.substr(0, slash), nullptr, radix);
            int64_t d = std::stoll(tok.substr(slash + 1), nullptr, radix);
            if (d == 0) return make_error("read: division by zero in rational");
            val = numeric::make_rational(n, d);
        } else if (radix == 10 && (tok.find('.') != std::string::npos ||
                                   tok.find('e') != std::string::npos ||
                                   tok.find('E') != std::string::npos)) {
            // Decimal/exponent notation is only meaningful in radix 10 (in hex
            // 'e' is a digit); produce an inexact real.
            val = numeric::make_inexact(std::stod(tok));
        } else {
            int64_t n = std::stoll(tok, nullptr, radix);
            val = numeric::make_integer(n);
        }
    } catch (const std::exception&) {
        return make_error("read: invalid number literal '" + tok + "'");
    }
    if (exactness == 1)       val = numeric::to_exact(val);
    else if (exactness == -1) val = numeric::make_inexact(numeric::to_double(val));
    return val;
}

Object* scheme_reader::read_number() {
    std::string num_str;

    // Handle negative sign
    if (peek() == '-') {
        num_str += consume();
    }

    // Read digits and optional decimal point
    bool has_decimal = false;
    while (!at_end() && (is_digit(peek()) || peek() == '.')) {
        char c = peek();
        if (c == '.') {
            if (has_decimal) {
                return make_error("read: invalid number (multiple decimal points)");
            }
            has_decimal = true;
        }
        num_str += consume();
    }

    // Check for rational number (e.g., 22/7)
    if (!at_end() && peek() == '/') {
        if (has_decimal) {
            return make_error("read: invalid rational (decimal in numerator)");
        }
        consume();  // consume '/'

        // Read denominator
        std::string denom_str;
        while (!at_end() && is_digit(peek())) {
            denom_str += consume();
        }

        if (denom_str.empty()) {
            return make_error("read: invalid rational (missing denominator)");
        }

        // Check for delimiter
        if (!at_end() && !is_delimiter(peek())) {
            return make_error("read: invalid number (no delimiter)");
        }

        try {
            int64_t numerator = std::stoll(num_str);
            int64_t denominator = std::stoll(denom_str);
            if (denominator == 0) {
                return make_error("read: division by zero in rational");
            }
            return numeric::make_rational(numerator, denominator);
        } catch (const std::exception&) {
            // stoll throws invalid_argument/out_of_range on bad input
            return make_error("read: invalid rational format");
        }
    }

    // Check for complex number (e.g., 3+4i, 3-4i)
    if (!at_end() && (peek() == '+' || peek() == '-')) {
        char sign = peek();
        const char* saved_pos = m_pos;
        consume();  // consume '+' or '-'

        // Read imaginary part
        std::string imag_str;
        if (sign == '-') {
            imag_str = "-";
        }

        bool imag_has_decimal = false;
        while (!at_end() && (is_digit(peek()) || peek() == '.')) {
            char c = peek();
            if (c == '.') {
                if (imag_has_decimal) {
                    // Not a valid complex number, backtrack
                    m_pos = saved_pos;
                    goto parse_real;
                }
                imag_has_decimal = true;
            }
            imag_str += consume();
        }

        // Check for 'i' suffix
        if (!at_end() && peek() == 'i') {
            consume();  // consume 'i'

            // Check for delimiter
            if (!at_end() && !is_delimiter(peek())) {
                return make_error("read: invalid number (no delimiter)");
            }

            try {
                // Parse real part
                Object* real_part;
                if (has_decimal) {
                    real_part = gc_new<atom<double>>(std::stod(num_str));
                } else {
                    real_part = numeric::make_integer(std::stoll(num_str));
                }

                // Protect from GC during imaginary part parsing
                gc_root<Object> real_guard(real_part);

                // Parse imaginary part
                Object* imag_part;
                if (imag_str.empty() || imag_str == "-") {
                    // Just 'i' or '-i'
                    imag_part = numeric::make_integer(imag_str == "-" ? -1 : 1);
                } else if (imag_has_decimal) {
                    imag_part = gc_new<atom<double>>(std::stod(imag_str));
                } else {
                    imag_part = numeric::make_integer(std::stoll(imag_str));
                }

                return gc_new<a_complex>(real_part, imag_part);
            } catch (const std::exception&) {
                // stod/stoll throw invalid_argument/out_of_range on bad input
                return make_error("read: invalid complex number format");
            }
        } else {
            // Not a complex number, backtrack
            m_pos = saved_pos;
        }
    }

parse_real:
    // Check for delimiter
    if (!at_end() && !is_delimiter(peek())) {
        return make_error("read: invalid number (no delimiter)");
    }

    // Parse as regular number
    try {
        if (has_decimal) {
            // Parse as floating point (inexact)
            double value = std::stod(num_str);
            return gc_new<atom<double>>(value);
        } else {
            // Parse as integer (exact)
            int64_t value = std::stoll(num_str);
            return numeric::make_integer(value);
        }
    } catch (const std::exception&) {
        // stod/stoll throw invalid_argument/out_of_range on bad input
        return make_error("read: invalid number format");
    }
}

Object* scheme_reader::read_string() {
    consume();  // consume opening "

    std::string str;
    while (!at_end()) {
        char c = peek();

        if (c == '"') {
            consume();  // consume closing "
            return gc_new<a_string>(str);
        }

        if (c == '\\') {
            consume();
            if (at_end()) {
                return make_error("read: unexpected end in string");
            }
            char escaped = consume();
            switch (escaped) {
                case 'n': str += '\n'; break;
                case 't': str += '\t'; break;
                case 'r': str += '\r'; break;
                case '\\': str += '\\'; break;
                case '"': str += '"'; break;
                default: str += escaped; break;
            }
        } else {
            str += consume();
        }
    }

    return make_error("read: unclosed string");
}

Object* scheme_reader::read_symbol() {
    std::string sym;

    if (!is_symbol_start(peek())) {
        return make_error("read: invalid symbol start");
    }

    while (!at_end() && is_symbol_char(peek())) {
        sym += consume();
    }

    // Check for delimiter
    if (!at_end() && !is_delimiter(peek())) {
        return make_error("read: invalid symbol (no delimiter)");
    }

    // Use symbol interning to ensure eq? works correctly with symbols
    return intern_symbol(sym);
}

Object* scheme_reader::read_char() {
    consume();  // consume '#'

    if (at_end()) {
        return make_error("read: unexpected end after '#'");
    }

    char next = peek();

    // Handle vector literals: #(elem ...)
    if (next == '(') {
        Object* lst = read_list();  // consumes '(' ... ')', returns a cons list
        // Propagate a nested parse error.
        if (a_string* e = dynamic_cast<a_string*>(lst)) {
            if (e->value().rfind("ERROR:", 0) == 0) return lst;
        }
        // Root the list while we copy its elements into the vector (gc_new below
        // allocates and may collect; elements stay reachable via lst).
        gc_root<Object> guard(lst);
        std::vector<Object*> elems;
        for (Object* p = lst; p && !p->is_nil() && p->pair(); p = cdr(p)) {
            elems.push_back(car(p));
        }
        return gc_new<a_vector>(elems);
    }

    // Handle bytevector literals: #u8(byte ...)
    if (next == 'u') {
        consume();  // 'u'
        if (at_end() || peek() != '8') {
            return make_error("read: expected '8' after '#u' (bytevector literal)");
        }
        consume();  // '8'
        if (at_end() || peek() != '(') {
            return make_error("read: expected '(' after '#u8' (bytevector literal)");
        }
        Object* lst = read_list();  // consumes '(' ... ')', returns a cons list
        if (a_string* e = dynamic_cast<a_string*>(lst)) {
            if (e->value().rfind("ERROR:", 0) == 0) return lst;
        }
        gc_root<Object> guard(lst);
        std::vector<uint8_t> bytes;
        for (Object* p = lst; p && !p->is_nil() && p->pair(); p = cdr(p)) {
            Object* elem = car(p);
            if (!numeric::is_integer_value(elem)) {
                return make_error("read: bytevector elements must be exact integers in [0,255]");
            }
            int64_t v = numeric::to_int64(elem);
            if (v < 0 || v > 255) {
                return make_error("read: bytevector byte out of range [0,255]");
            }
            bytes.push_back(static_cast<uint8_t>(v));
        }
        return gc_new<a_bytevector>(bytes);
    }

    // Handle number prefixes: #b #o #d #x (radix) and #e #i (exactness),
    // combinable in either order (e.g. #xff, #e1.5, #e#xa). R7RS 7.1.1.
    if (next == 'x' || next == 'X' || next == 'b' || next == 'B' ||
        next == 'o' || next == 'O' || next == 'd' || next == 'D' ||
        next == 'e' || next == 'E' || next == 'i' || next == 'I') {
        int radix = 10;
        int exactness = 0;  // 0 = as-written, 1 = exact, -1 = inexact
        for (int k = 0; k < 2; ++k) {
            char c = peek();
            if      (c == 'x' || c == 'X') { radix = 16; consume(); }
            else if (c == 'b' || c == 'B') { radix = 2;  consume(); }
            else if (c == 'o' || c == 'O') { radix = 8;  consume(); }
            else if (c == 'd' || c == 'D') { radix = 10; consume(); }
            else if (c == 'e' || c == 'E') { exactness = 1;  consume(); }
            else if (c == 'i' || c == 'I') { exactness = -1; consume(); }
            else return make_error("read: unknown '#' syntax");
            // A second prefix must be introduced by another '#'.
            if (!at_end() && peek() == '#') { consume(); continue; }
            break;
        }
        std::string tok;
        while (!at_end() && !is_delimiter(peek())) {
            tok += consume();
        }
        return parse_number_token(tok, radix, exactness);
    }

    // Handle boolean literals #t and #f
    if (next == 't' || next == 'f') {
        consume();  // consume 't' or 'f'

        // Check that it's followed by a delimiter
        if (!at_end() && !is_delimiter(peek())) {
            return make_error("read: invalid boolean literal (must be #t or #f)");
        }

        return (next == 't') ? (Object*)t : (Object*)false_obj;
    }

    // Handle character literals #\...
    if (next != '\\') {
        return make_error("read: expected '\\' after '#' for character literal, or 't'/'f' for boolean");
    }

    consume();  // consume '\\'

    if (at_end()) {
        return make_error("read: unexpected end after '#\\'");
    }

    // Always take the FIRST character literally, even if it is itself a
    // delimiter: #\; #\( #\) #\" #\space-as-#\<space> are all valid single-char
    // literals. (The old code stopped at any delimiter, so #\; #\( #\) read an
    // empty name -> "empty character literal" / mis-parsed as comment/paren.)
    std::string name;
    name += consume();

    // Only a letter can begin a multi-character name (space, newline, tab,
    // return). For a punctuation/delimiter first char it is always single-char.
    if (std::isalpha(static_cast<unsigned char>(name[0]))) {
        while (!at_end() && !is_delimiter(peek())) {
            name += consume();
        }
    }

    // Handle named characters (R7RS 7.1.1)
    if (name == "space") {
        return gc_new<atom<char>>(' ');
    } else if (name == "newline") {
        return gc_new<atom<char>>('\n');
    } else if (name == "tab") {
        return gc_new<atom<char>>('\t');
    } else if (name == "return") {
        return gc_new<atom<char>>('\r');
    } else if (name == "null" || name == "nul") {
        return gc_new<atom<char>>('\0');
    } else if (name == "alarm") {
        return gc_new<atom<char>>('\a');
    } else if (name == "backspace") {
        return gc_new<atom<char>>('\b');
    } else if (name == "delete" || name == "rubout") {
        return gc_new<atom<char>>('\x7f');
    } else if (name == "escape" || name == "esc") {
        return gc_new<atom<char>>('\x1b');
    } else if ((name[0] == 'x' || name[0] == 'X') && name.length() > 1) {
        // #\xHH hex scalar value (R7RS)
        try {
            size_t pos;
            long code = std::stol(name.substr(1), &pos, 16);
            if (pos == name.length() - 1 && code >= 0 && code <= 0x10FFFF) {
                return gc_new<atom<char>>(static_cast<char>(code));
            }
        } catch (const std::exception&) { /* fall through to error */ }
        return make_error("read: invalid hex character literal '" + name + "'");
    } else if (name.length() == 1) {
        // Single character literal
        return gc_new<atom<char>>(name[0]);
    } else {
        return make_error("read: unknown character name '" + name + "'");
    }
}

Object* scheme_reader::make_error(const std::string& message) {
    std::ostringstream oss;
    oss << "ERROR: " << message << " at line " << m_line << ", column " << m_column;
    // Return error as a_string, not throwing (for embedding API)
    return gc_new<a_string>(oss.str());
}

bool scheme_reader::is_delimiter(char c) {
    return std::isspace(c) || c == '(' || c == ')' || c == '\0' || c == ';' || c == '"';
}

bool scheme_reader::is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool scheme_reader::is_symbol_start(char c) {
    return std::isalpha(c) || c == '+' || c == '-' || c == '*' || c == '/' ||
           c == '<' || c == '>' || c == '=' || c == '!' || c == '?' ||
           c == ':' || c == '$' || c == '%' || c == '_' || c == '&' ||
           c == '~' || c == '^' || c == '.';
}

bool scheme_reader::is_symbol_char(char c) {
    return is_symbol_start(c) || is_digit(c);
}

} } // namespace zombie::runtime
