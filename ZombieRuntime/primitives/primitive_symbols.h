#pragma once

#include "../primitive_base.h"

/**
 * @file primitive_symbols.h
 * @brief Symbol primitives (R5RS Section 6.4)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 *
 * Implements symbol-related primitives:
 * - symbol?: Type predicate for symbols
 * - symbol->string: Convert symbol to string
 * - string->symbol: Convert string to symbol
 */

/**
 * @brief Symbol primitives (R5RS Section 6.4)
 *
 * Handles symbol operations.
 *
 * **Example Usage:**
 * @code
 * (symbol? 'foo)             ; => #t
 * (symbol? "foo")            ; => #f
 * (symbol->string 'foo)      ; => "foo"
 * (string->symbol "foo")     ; => foo
 * @endcode
 */
class PrimitiveSymbols : public PrimitiveBase {
public:
    /**
     * @brief Primitive IDs for symbol operations
     */
    enum ID {
        SYMBOLQ = 0,         ///< symbol? predicate
        SYMBOLTOSTRING = 1,  ///< symbol->string conversion
        STRINGTOSYMBOL = 2   ///< string->symbol conversion
    };

    /**
     * @brief Construct symbol Primitive
     * @param id Which symbol Primitive
     */
    PrimitiveSymbols(ID id);

    /**
     * @brief Apply symbol Primitive to arguments
     * @param eval Evaluator
     * @param args Argument list (expected: 1 argument)
     * @param env Environment
     * @return Result of symbol operation
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all symbol primitives into environment
     * @param env Environment to install primitives into
     */
    static void install(Environment* env);

private:
    ID m_id;  ///< Which symbol Primitive this instance represents
};
