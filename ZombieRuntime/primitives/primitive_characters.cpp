#include "zombie_runtime_types.h"
#include "primitive_characters.h"
#include "../native_primitive.h"
#include "../Base.h"
#include "../atom.h"
#include "../numeric_runtime.h"
#include <cctype>

/**
 * @file primitive_characters.cpp
 * @brief Character primitives (R7RS 6.6), table-driven.
 */

namespace {

using namespace zombie::runtime;

char chr(Object* obj) {
    if (obj == NULL || obj->is_nil()) return '\0';
    a_char* ch = dynamic_cast<a_char*>(obj);
    return ch ? ch->value() : '\0';
}
Object* chr(char c) { return gc_new<a_char>(c); }

// -1 / 0 / 1 for x </=/> y (optionally case-insensitive).
int charCompare(Object* x, Object* y, bool ci) {
    char c1 = chr(x), c2 = chr(y);
    if (ci) { c1 = std::tolower(c1); c2 = std::tolower(c2); }
    return (c1 < c2) ? -1 : (c1 > c2) ? 1 : 0;
}

// Variadic comparison chain (R7RS 6.6): holds iff it holds for every adjacent
// pair. op: 0 '=', 1 '<', 2 '>', 3 '<=', 4 '>='.
Object* charCompareChain(Object* args, bool ci, int op) {
    Object* prev = first(args);
    for (Object* q = rest(args); q && !q->is_nil(); q = rest(q)) {
        Object* cur = first(q);
        int c = charCompare(prev, cur, ci);
        bool ok = (op == 0) ? (c == 0) : (op == 1) ? (c < 0) : (op == 2) ? (c > 0)
                : (op == 3) ? (c <= 0) : (c >= 0);
        if (!ok) return static_cast<Object*>(false_obj);
        prev = cur;
    }
    return t;
}

// Shared guard for the 1-char-argument operations.
#define NEED_CHAR(x, who) do { if (!is_char(x)) return error(who ": expected a character"); } while (0)

Object* prim_charq(IEvaluator*, Object* a, Environment*) { return truth(is_char(first(a))); }
Object* prim_char_alpha(IEvaluator*, Object* a, Environment*) { Object* x=first(a); NEED_CHAR(x,"char-alphabetic?"); return truth(std::isalpha(chr(x))); }
Object* prim_char_numeric(IEvaluator*, Object* a, Environment*) { Object* x=first(a); NEED_CHAR(x,"char-numeric?"); return truth(std::isdigit(chr(x))); }
Object* prim_char_white(IEvaluator*, Object* a, Environment*) { Object* x=first(a); NEED_CHAR(x,"char-whitespace?"); return truth(std::isspace(chr(x))); }
Object* prim_char_upperq(IEvaluator*, Object* a, Environment*) { Object* x=first(a); NEED_CHAR(x,"char-upper-case?"); return truth(std::isupper(chr(x))); }
Object* prim_char_lowerq(IEvaluator*, Object* a, Environment*) { Object* x=first(a); NEED_CHAR(x,"char-lower-case?"); return truth(std::islower(chr(x))); }

// (digit-value c) -> 0..9 for a decimal digit, else #f (R7RS).
Object* prim_digit_value(IEvaluator*, Object* a, Environment*) {
    Object* x=first(a); NEED_CHAR(x,"digit-value");
    char c = chr(x);
    if (c >= '0' && c <= '9') return numeric::make_integer(c - '0');
    return static_cast<Object*>(false_obj);
}

Object* prim_char_to_int(IEvaluator*, Object* a, Environment*) { Object* x=first(a); NEED_CHAR(x,"char->integer"); return numeric::make_integer((int)chr(x)); }
Object* prim_int_to_char(IEvaluator*, Object* a, Environment*) {
    Object* x=first(a);
    if (!numeric::is_number(x)) return error("integer->char: expected a number");
    return chr((char)(int)num(x));
}
Object* prim_char_upcase(IEvaluator*, Object* a, Environment*)   { Object* x=first(a); NEED_CHAR(x,"char-upcase"); return chr(std::toupper(chr(x))); }
Object* prim_char_downcase(IEvaluator*, Object* a, Environment*) { Object* x=first(a); NEED_CHAR(x,"char-downcase"); return chr(std::tolower(chr(x))); }
// ASCII case folding == downcase.
Object* prim_char_foldcase(IEvaluator*, Object* a, Environment*) { Object* x=first(a); NEED_CHAR(x,"char-foldcase"); return chr(std::tolower(chr(x))); }

// Comparison chains (case-sensitive then case-insensitive).
Object* prim_char_eq(IEvaluator*, Object* a, Environment*) { return charCompareChain(a, false, 0); }
Object* prim_char_lt(IEvaluator*, Object* a, Environment*) { return charCompareChain(a, false, 1); }
Object* prim_char_gt(IEvaluator*, Object* a, Environment*) { return charCompareChain(a, false, 2); }
Object* prim_char_le(IEvaluator*, Object* a, Environment*) { return charCompareChain(a, false, 3); }
Object* prim_char_ge(IEvaluator*, Object* a, Environment*) { return charCompareChain(a, false, 4); }
Object* prim_char_ci_eq(IEvaluator*, Object* a, Environment*) { return charCompareChain(a, true, 0); }
Object* prim_char_ci_lt(IEvaluator*, Object* a, Environment*) { return charCompareChain(a, true, 1); }
Object* prim_char_ci_gt(IEvaluator*, Object* a, Environment*) { return charCompareChain(a, true, 2); }
Object* prim_char_ci_le(IEvaluator*, Object* a, Environment*) { return charCompareChain(a, true, 3); }
Object* prim_char_ci_ge(IEvaluator*, Object* a, Environment*) { return charCompareChain(a, true, 4); }

#undef NEED_CHAR

}  // namespace

void PrimitiveCharacters::install(zombie::runtime::Environment* env) {
    using namespace zombie::runtime;
    static const NativeEntry entries[] = {
        { "char?",            1, 1,      prim_charq        },
        { "char-alphabetic?", 1, 1,      prim_char_alpha   },
        { "char-numeric?",    1, 1,      prim_char_numeric },
        { "digit-value",      1, 1,      prim_digit_value  },
        { "char-whitespace?", 1, 1,      prim_char_white   },
        { "char-upper-case?", 1, 1,      prim_char_upperq  },
        { "char-lower-case?", 1, 1,      prim_char_lowerq  },
        { "char->integer",    1, 1,      prim_char_to_int  },
        { "integer->char",    1, 1,      prim_int_to_char  },
        { "char-upcase",      1, 1,      prim_char_upcase  },
        { "char-downcase",    1, 1,      prim_char_downcase},
        { "char-foldcase",    1, 1,      prim_char_foldcase},
        { "char=?",           2, 999999, prim_char_eq      },
        { "char<?",           2, 999999, prim_char_lt      },
        { "char>?",           2, 999999, prim_char_gt      },
        { "char<=?",          2, 999999, prim_char_le      },
        { "char>=?",          2, 999999, prim_char_ge      },
        { "char-ci=?",        2, 999999, prim_char_ci_eq   },
        { "char-ci<?",        2, 999999, prim_char_ci_lt   },
        { "char-ci>?",        2, 999999, prim_char_ci_gt   },
        { "char-ci<=?",       2, 999999, prim_char_ci_le   },
        { "char-ci>=?",       2, 999999, prim_char_ci_ge   },
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}
