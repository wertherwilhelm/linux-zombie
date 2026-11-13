#pragma once

#include "../primitive_base.h"

/**
 * @file primitive_characters.h
 * @brief Character primitives (R5RS Section 6.6)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 *
 * Implements character operations including:
 * - Predicates: char?, char-alphabetic?, char-numeric?, char-whitespace?, char-upper-case?, char-lower-case?
 * - Conversions: char->integer, integer->char, char-upcase, char-downcase
 * - Comparisons: char=?, char<?, char>?, char<=?, char>=?
 * - Case-insensitive comparisons: char-ci=?, char-ci<?, char-ci>?, char-ci<=?, char-ci>=?
 */

/**
 * @brief Character primitives (R5RS Section 6.6)
 *
 * Handles all character operations.
 *
 * **Example Usage:**
 * @code
 * (char? #\a)                ; => #t
 * (char-alphabetic? #\a)     ; => #t
 * (char-numeric? #\5)        ; => #t
 * (char-upcase #\a)          ; => #\A
 * (char->integer #\A)        ; => 65
 * (integer->char 65)         ; => #\A
 * (char=? #\a #\a)           ; => #t
 * (char-ci=? #\a #\A)        ; => #t (case-insensitive)
 * @endcode
 */
class PrimitiveCharacters : public PrimitiveBase {
public:
    /**
     * @brief Primitive IDs for character operations
     */
    enum ID {
        CHARQ = 0,              ///< char? predicate
        CHARALPHABETICQ = 1,    ///< char-alphabetic? predicate
        CHARNUMERICQ = 2,       ///< char-numeric? predicate
        CHARWHITESPACEQ = 3,    ///< char-whitespace? predicate
        CHARUPPERCASEQ = 4,     ///< char-upper-case? predicate
        CHARLOWERCASEQ = 5,     ///< char-lower-case? predicate
        CHARTOINTEGER = 6,      ///< char->integer conversion
        INTEGERTOCHAR = 7,      ///< integer->char conversion
        CHARUPCASE = 8,         ///< char-upcase
        CHARDOWNCASE = 9,       ///< char-downcase
        CHARFOLDCASE = 10,      ///< char-foldcase (R7RS)

        // Case-sensitive comparisons
        CHAREQ = 11,            ///< char=?
        CHARLT = 12,            ///< char<?
        CHARGT = 13,            ///< char>?
        CHARLE = 14,            ///< char<=?
        CHARGE = 15,            ///< char>=?

        // Case-insensitive comparisons
        CHARCIEQ = 16,          ///< char-ci=?
        CHARCILT = 17,          ///< char-ci<?
        CHARCIGT = 18,          ///< char-ci>?
        CHARCILE = 19,          ///< char-ci<=?
        CHARCIGE = 20           ///< char-ci>=?
    };

    /**
     * @brief Construct character Primitive
     * @param id Which character Primitive
     */
    PrimitiveCharacters(ID id);

    /**
     * @brief Apply character Primitive to arguments
     * @param eval Evaluator
     * @param args Argument list
     * @param env Environment
     * @return Result of character operation
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all character primitives into environment
     * @param env Environment to install primitives into
     */
    static void install(Environment* env);

private:
    ID m_id;  ///< Which character Primitive this instance represents
};
