#include "zombie_runtime_types.h"
#include "zombie_runtime_types.h"
#include "primitive_characters.h"
#include "../Base.h"
#include "../atom.h"
#include "../numeric_runtime.h"
#include <cctype>

/**
 * @file primitive_characters.cpp
 * @brief Implementation of character primitives (R5RS Section 6.6)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 */

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Extract character value from Object
 * @param obj Object to extract from
 * @return Character value or '\0' if not a character
 */
static char chr(Object* obj) {
    if (obj == NULL || obj->is_nil()) {
        return '\0';
    }
    a_char* ch = dynamic_cast<a_char*>(obj);
    if (ch) {
        return ch->value();
    }
    return '\0';
}

/**
 * @brief Create character Object from char value
 * @param c Character value
 * @return Character Object
 */
static Object* chr(char c) {
    return gc_new<a_char>(c);
}

/**
 * @brief Compare two character objects
 * @param x First character
 * @param y Second character
 * @param caseInsensitive True for case-insensitive comparison
 * @return -1 if x < y, 0 if equal, 1 if x > y
 */
static int charCompare(Object* x, Object* y, bool caseInsensitive) {
    char c1 = chr(x);
    char c2 = chr(y);

    if (caseInsensitive) {
        c1 = std::tolower(c1);
        c2 = std::tolower(c2);
    }

    if (c1 < c2) return -1;
    if (c1 > c2) return 1;
    return 0;
}

// Variadic comparison chain for char=?/<?/>?/<=?/>=? (and ci variants): the
// predicate holds iff it holds for every adjacent pair (R7RS 6.6). op encodes
// the relation: 0 '=', 1 '<', 2 '>', 3 '<=', 4 '>='.
static Object* charCompareChain(Object* args, bool ci, int op) {
    Object* prev = first(args);
    for (Object* q = rest(args); q && !q->is_nil(); q = rest(q)) {
        Object* cur = first(q);
        int c = charCompare(prev, cur, ci);
        bool ok = (op == 0) ? (c == 0)
                : (op == 1) ? (c < 0)
                : (op == 2) ? (c > 0)
                : (op == 3) ? (c <= 0)
                            : (c >= 0);
        if (!ok) return static_cast<Object*>(false_obj);
        prev = cur;
    }
    return t;
}

// ============================================================================
// Constructor
// ============================================================================

PrimitiveCharacters::PrimitiveCharacters(ID id)
    : PrimitiveBase(
        (id >= CHAREQ && id <= CHARCIGE) ? 2 : 1,        // comparisons: 2+ args
        (id >= CHAREQ && id <= CHARCIGE) ? 999999 : 1    // (variadic chain)
      ),
      m_id(id) {
    // Set Primitive name based on ID
    switch (id) {
        case CHARQ:           m_name = "char?"; break;
        case CHARALPHABETICQ: m_name = "char-alphabetic?"; break;
        case CHARNUMERICQ:    m_name = "char-numeric?"; break;
        case DIGITVALUE:      m_name = "digit-value"; break;
        case CHARWHITESPACEQ: m_name = "char-whitespace?"; break;
        case CHARUPPERCASEQ:  m_name = "char-upper-case?"; break;
        case CHARLOWERCASEQ:  m_name = "char-lower-case?"; break;
        case CHARTOINTEGER:   m_name = "char->integer"; break;
        case INTEGERTOCHAR:   m_name = "integer->char"; break;
        case CHARUPCASE:      m_name = "char-upcase"; break;
        case CHARDOWNCASE:    m_name = "char-downcase"; break;
        case CHARFOLDCASE:    m_name = "char-foldcase"; break;

        // Case-sensitive comparisons
        case CHAREQ:  m_name = "char=?"; break;
        case CHARLT:  m_name = "char<?"; break;
        case CHARGT:  m_name = "char>?"; break;
        case CHARLE:  m_name = "char<=?"; break;
        case CHARGE:  m_name = "char>=?"; break;

        // Case-insensitive comparisons
        case CHARCIEQ: m_name = "char-ci=?"; break;
        case CHARCILT: m_name = "char-ci<?"; break;
        case CHARCIGT: m_name = "char-ci>?"; break;
        case CHARCILE: m_name = "char-ci<=?"; break;
        case CHARCIGE: m_name = "char-ci>=?"; break;
    }
}

// ============================================================================
// Primitive Application
// ============================================================================

Object* PrimitiveCharacters::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Arity checking
    if (!check_arity(args)) {
        return arity_error("");
    }

    // Extract arguments
    Object* x = first(args);
    Object* y = (m_id >= CHAREQ && m_id <= CHARCIGE) ? second(args) : nil;

    // Dispatch based on Primitive ID
    switch (m_id) {
        // ====================================================================
        // Predicates
        // ====================================================================

        case CHARQ:
            return truth(is_char(x));

        case CHARALPHABETICQ: {
            if (!is_char(x)) {
                return error("char-alphabetic?: expected a character");
            }
            return truth(std::isalpha(chr(x)));
        }

        case CHARNUMERICQ: {
            if (!is_char(x)) {
                return error("char-numeric?: expected a character");
            }
            return truth(std::isdigit(chr(x)));
        }

        case CHARWHITESPACEQ: {
            if (!is_char(x)) {
                return error("char-whitespace?: expected a character");
            }
            return truth(std::isspace(chr(x)));
        }

        case DIGITVALUE: {
            // (digit-value c) -> 0..9 for a decimal digit, else #f (R7RS).
            if (!is_char(x)) {
                return error("digit-value: expected a character");
            }
            char c = chr(x);
            if (c >= '0' && c <= '9') return numeric::make_integer(c - '0');
            return false_obj;
        }

        case CHARUPPERCASEQ: {
            if (!is_char(x)) {
                return error("char-upper-case?: expected a character");
            }
            return truth(std::isupper(chr(x)));
        }

        case CHARLOWERCASEQ: {
            if (!is_char(x)) {
                return error("char-lower-case?: expected a character");
            }
            return truth(std::islower(chr(x)));
        }

        // ====================================================================
        // Conversions
        // ====================================================================

        case CHARTOINTEGER: {
            if (!is_char(x)) {
                return error("char->integer: expected a character");
            }
            return numeric::make_integer((int)chr(x));
        }

        case INTEGERTOCHAR: {
            if (!numeric::is_number(x)) {
                return error("integer->char: expected a number");
            }
            return chr((char)(int)num(x));
        }

        case CHARUPCASE: {
            if (!is_char(x)) {
                return error("char-upcase: expected a character");
            }
            return chr(std::toupper(chr(x)));
        }

        case CHARDOWNCASE: {
            if (!is_char(x)) {
                return error("char-downcase: expected a character");
            }
            return chr(std::tolower(chr(x)));
        }

        case CHARFOLDCASE: {
            // (char-foldcase char) - convert character using case folding
            // R7RS: For case-insensitive comparison, uses Unicode case folding
            // For ASCII simplicity, we use tolower (same as char-downcase for ASCII)
            if (!is_char(x)) {
                return error("char-foldcase: expected a character");
            }
            return chr(std::tolower(chr(x)));
        }

        // ====================================================================
        // Case-Sensitive Comparisons
        // ====================================================================

        case CHAREQ:  return charCompareChain(args, false, 0);
        case CHARLT:  return charCompareChain(args, false, 1);
        case CHARGT:  return charCompareChain(args, false, 2);
        case CHARLE:  return charCompareChain(args, false, 3);
        case CHARGE:  return charCompareChain(args, false, 4);

        // ====================================================================
        // Case-Insensitive Comparisons
        // ====================================================================

        case CHARCIEQ: return charCompareChain(args, true, 0);
        case CHARCILT: return charCompareChain(args, true, 1);
        case CHARCIGT: return charCompareChain(args, true, 2);
        case CHARCILE: return charCompareChain(args, true, 3);
        case CHARCIGE: return charCompareChain(args, true, 4);

        default:
            return error("Unknown character Primitive");
    }
}

// ============================================================================
// Registration
// ============================================================================

void PrimitiveCharacters::install(Environment* env) {
    // GC Safety: Use macro to protect name string before allocating primitive
    #define DEF(name, id) do { \
        gc_root<a_string> _n(gc_new<a_string>(name)); \
        env->define(_n.get(), gc_new<PrimitiveCharacters>(id)); \
    } while(0)

    // Predicates
    DEF("char?",            CHARQ);
    DEF("char-alphabetic?", CHARALPHABETICQ);
    DEF("char-numeric?",    CHARNUMERICQ);
    DEF("digit-value",      DIGITVALUE);
    DEF("char-whitespace?", CHARWHITESPACEQ);
    DEF("char-upper-case?", CHARUPPERCASEQ);
    DEF("char-lower-case?", CHARLOWERCASEQ);

    // Conversions
    DEF("char->integer", CHARTOINTEGER);
    DEF("integer->char", INTEGERTOCHAR);
    DEF("char-upcase",   CHARUPCASE);
    DEF("char-downcase", CHARDOWNCASE);
    DEF("char-foldcase", CHARFOLDCASE);

    // Case-sensitive comparisons
    DEF("char=?",  CHAREQ);
    DEF("char<?",  CHARLT);
    DEF("char>?",  CHARGT);
    DEF("char<=?", CHARLE);
    DEF("char>=?", CHARGE);

    // Case-insensitive comparisons
    DEF("char-ci=?",  CHARCIEQ);
    DEF("char-ci<?",  CHARCILT);
    DEF("char-ci>?",  CHARCIGT);
    DEF("char-ci<=?", CHARCILE);
    DEF("char-ci>=?", CHARCIGE);

    #undef DEF
}
