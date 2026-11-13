#pragma once

#include "../primitive_base.h"

/**
 * @file primitive_strings.h
 * @brief String primitives (R5RS Section 6.7)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 *
 * Implements string operations including:
 * - Predicates: string?
 * - Construction: make-string, string
 * - Access: string-length, string-ref, string-set!, substring
 * - Operations: string-append, string-copy, string-upcase, string-downcase, string-fill!
 * - Conversions: string->list, list->string
 * - Comparisons: string=?, string<?, string>?, string<=?, string>=?
 * - Case-insensitive: string-ci=?, string-ci<?, string-ci>?, string-ci<=?, string-ci>=?
 */

/**
 * @brief String primitives (R5RS Section 6.7)
 *
 * Handles all string operations.
 *
 * **Example Usage:**
 * @code
 * (make-string 5 #\x)       ; => "xxxxx"
 * (string #\a #\b #\c)      ; => "abc"
 * (string-length "hello")   ; => 5
 * (string-ref "hello" 1)    ; => #\e
 * (substring "hello" 1 4)   ; => "ell"
 * (string-append "ab" "cd") ; => "abcd"
 * (string-upcase "hello")   ; => "HELLO"
 * @endcode
 */
class PrimitiveStrings : public PrimitiveBase {
public:
    /**
     * @brief Primitive IDs for string operations
     */
    enum ID {
        STRINGQ = 0,           ///< string? predicate
        MAKESTRING = 1,        ///< make-string
        STRING = 2,            ///< string (construct from chars)
        STRINGLENGTH = 3,      ///< string-length
        STRINGREF = 4,         ///< string-ref
        STRINGSET = 5,         ///< string-set!
        SUBSTRING = 6,         ///< substring
        STRINGAPPEND = 7,      ///< string-append
        STRINGCOPY = 8,        ///< string-copy
        STRINGUPCASE = 9,      ///< string-upcase
        STRINGDOWNCASE = 10,   ///< string-downcase
        STRINGFOLDCASE = 11,   ///< string-foldcase (R7RS)
        STRINGTOLIST = 12,     ///< string->list
        LISTTOSTRING = 13,     ///< list->string
        STRINGFILL = 14,       ///< string-fill!

        // Case-sensitive comparisons
        STRINGEQ = 15,         ///< string=?
        STRINGLT = 16,         ///< string<?
        STRINGGT = 17,         ///< string>?
        STRINGLE = 18,         ///< string<=?
        STRINGGE = 19,         ///< string>=?

        // Case-insensitive comparisons
        STRINGCIEQ = 20,       ///< string-ci=?
        STRINGCILT = 21,       ///< string-ci<?
        STRINGCIGT = 22,       ///< string-ci>?
        STRINGCILE = 23,       ///< string-ci<=?
        STRINGCIGE = 24,       ///< string-ci>=?

        // R7RS additions
        STRINGMAP = 25,        ///< string-map (R7RS)
        STRINGFOREACH = 26     ///< string-for-each (R7RS)
    };

    /**
     * @brief Construct string Primitive
     * @param id Which string Primitive
     * @param minArgs Minimum arguments
     * @param maxArgs Maximum arguments
     */
    PrimitiveStrings(ID id, int minArgs, int maxArgs);

    /**
     * @brief Apply string Primitive to arguments
     * @param eval Evaluator
     * @param args Argument list
     * @param env Environment
     * @return Result of string operation
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all string primitives into environment
     * @param env Environment to install primitives into
     */
    static void install(Environment* env);

private:
    ID m_id;  ///< Which string Primitive this instance represents
};
