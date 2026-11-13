#pragma once

#include "../primitive_base.h"

/**
 * @file primitive_io.h
 * @brief I/O primitives (R5RS Section 6.10)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 *
 * Implements input/output operations including:
 * - Port predicates: eof-Object?, input-port?, output-port?
 * - Port access: current-input-port, current-output-port
 * - File operations: open-input-file, close-input-port, open-output-file, close-output-port
 * - Character I/O: read-char, peek-char, char-ready?, write-char
 * - S-expression I/O: read, write, display
 * - Output: newline
 * - File loading: load
 * - Environment: the-environment
 */

/**
 * @brief I/O primitives (R5RS Section 6.10)
 *
 * Handles all input/output operations.
 *
 * **Example Usage:**
 * @code
 * (open-input-file "test.txt")      ; => #<input-port>
 * (read-char)                       ; => #\a
 * (peek-char)                       ; => #\a (doesn't consume)
 * (read)                            ; => (+ 1 2)
 * (write '(a b c))                  ; => (a b c)
 * (display "hello")                 ; => hello
 * (newline)                         ; => newline
 * (load "file.scm")                 ; => loads and evaluates file
 * @endcode
 */
class PrimitiveIO : public PrimitiveBase {
public:
    /**
     * @brief Primitive IDs for I/O operations
     */
    enum ID {
        EOFOBJECTQ = 0,       ///< eof-Object? predicate
        INPUTPORTQ = 1,       ///< input-port? predicate
        CURRENTINPUTPORT = 2, ///< current-input-port
        OPENINPUTFILE = 3,    ///< open-input-file
        CLOSEINPUTPORT = 4,   ///< close-input-port
        OUTPUTPORTQ = 5,      ///< output-port? predicate
        CURRENTOUTPUTPORT = 6,///< current-output-port
        OPENOUTPUTFILE = 7,   ///< open-output-file
        CLOSEOUTPUTPORT = 8,  ///< close-output-port
        READCHAR = 9,         ///< read-char
        PEEKCHAR = 10,        ///< peek-char
        CHARREADYQ = 11,      ///< char-ready?
        READ = 12,            ///< read
        LOAD = 13,            ///< load
        WRITE = 14,           ///< write
        DISPLAY = 15,         ///< display
        NEWLINE = 16,         ///< newline
        WRITECHAR = 17,       ///< write-char
        THEENVIRONMENT = 18,  ///< the-environment

        // R7RS additions
        READLINE = 19,        ///< read-line (R7RS)
        READSTRING = 20,      ///< read-string (R7RS)
        WRITESTRING = 21,     ///< write-string (R7RS)
        FLUSHOUTPUT = 22,     ///< flush-output-port (R7RS)

        // R7RS string ports
        OPENINPUTSTRING = 23,  ///< open-input-string (R7RS)
        OPENOUTPUTSTRING = 24, ///< open-output-string (R7RS)
        GETOUTPUTSTRING = 25,  ///< get-output-string (R7RS)

        // R7RS bytevector I/O
        READU8 = 26,           ///< read-u8 (R7RS)
        PEEKU8 = 27,           ///< peek-u8 (R7RS)
        U8READYQ = 28,         ///< u8-ready? (R7RS)
        WRITEU8 = 29,          ///< write-u8 (R7RS)
        READBYTEVECTOR = 30,   ///< read-bytevector (R7RS)
        READBYTEVECTORBANG = 31, ///< read-bytevector! (R7RS)
        WRITEBYTEVECTOR = 32   ///< write-bytevector (R7RS)
    };

    /**
     * @brief Construct I/O Primitive
     * @param id Which I/O Primitive
     * @param minArgs Minimum arguments
     * @param maxArgs Maximum arguments
     */
    PrimitiveIO(ID id, int minArgs, int maxArgs);

    /**
     * @brief Apply I/O Primitive to arguments
     * @param eval Evaluator
     * @param args Argument list
     * @param env Environment
     * @return Result of I/O operation
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all I/O primitives into environment
     * @param env Environment to install primitives into
     */
    static void install(Environment* env);

private:
    ID m_id;  ///< Which I/O Primitive this instance represents
};
