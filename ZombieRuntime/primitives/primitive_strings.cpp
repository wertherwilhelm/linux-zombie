#include "pch.h"
#include "../pch.h"
#include "primitive_strings.h"
#include "../Base.h"
#include "../atom.h"
#include "../numeric_runtime.h"
#include <algorithm>
#include <cctype>

/**
 * @file primitive_strings.cpp
 * @brief Implementation of string primitives (R5RS Section 6.7)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 */

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Concatenate multiple strings
 * @param args List of string arguments
 * @return Concatenated string
 */
static Object* string_append(Object* args) {
    // (string-append str1 str2 ...) - concatenate strings
    // Handle empty argument list
    if (args == NULL || args->is_nil()) {
        return gc_new<a_string>("");
    }

    std::string result = "";

    // Iterate through all arguments
    for (Object* arg = args; arg != NULL && !arg->is_nil(); arg = rest(arg)) {
        Object* str_obj = first(arg);

        // Check if argument is a string
        if (!is_string(str_obj)) {
            return error("string-append: all arguments must be strings");
        }

        // Get the string value and append it
        a_string* str = dynamic_cast<a_string*>(str_obj);
        if (str) {
            result += str->value();
        }
    }

    return gc_new<a_string>(result);
}

/**
 * @brief Construct string from list of characters
 * @param args List of character arguments
 * @return String constructed from characters
 */
static Object* ListToString(Object* args) {
    // (string char1 char2 ...) - construct a string from characters
    // Handle empty argument list
    if (args == NULL || args->is_nil()) {
        return gc_new<a_string>("");
    }

    std::string result = "";

    // Iterate through all arguments
    for (Object* arg = args; arg != NULL && !arg->is_nil(); arg = rest(arg)) {
        Object* char_obj = first(arg);

        // Check if argument is a character
        if (!is_char(char_obj)) {
            return error("string: all arguments must be characters");
        }

        // Get the character value and append it
        a_char* ch = dynamic_cast<a_char*>(char_obj);
        if (ch) {
            result += ch->value();
        }
    }

    return gc_new<a_string>(result);
}

/**
 * @brief Compare two strings
 * @param str1 First string
 * @param str2 Second string
 * @param case_insensitive True for case-insensitive comparison
 * @return -1 if str1 < str2, 0 if equal, 1 if str1 > str2
 */
static int stringCompare(Object* str1, Object* str2, bool case_insensitive) {
    // Compare two strings, return -1 if str1 < str2, 0 if equal, 1 if str1 > str2
    if (!is_string(str1) || !is_string(str2)) {
        error("stringCompare: both arguments must be strings");
        return 0;
    }

    a_string* s1 = dynamic_cast<a_string*>(str1);
    a_string* s2 = dynamic_cast<a_string*>(str2);

    if (!s1 || !s2) {
        return 0;
    }

    std::string str1_val = s1->value();
    std::string str2_val = s2->value();

    if (case_insensitive) {
        // Convert both strings to lowercase for comparison
        std::transform(str1_val.begin(), str1_val.end(), str1_val.begin(), ::tolower);
        std::transform(str2_val.begin(), str2_val.end(), str2_val.begin(), ::tolower);
    }

    // Use string comparison
    int result = str1_val.compare(str2_val);

    // Normalize to -1, 0, 1
    if (result < 0) return -1;
    if (result > 0) return 1;
    return 0;
}

// ============================================================================
// Constructor
// ============================================================================

PrimitiveStrings::PrimitiveStrings(ID id, int minArgs, int maxArgs)
    : PrimitiveBase(minArgs, maxArgs), m_id(id) {
    // Set Primitive name based on ID
    switch (id) {
        case STRINGQ:       m_name = "string?"; break;
        case MAKESTRING:    m_name = "make-string"; break;
        case STRING:        m_name = "string"; break;
        case STRINGLENGTH:  m_name = "string-length"; break;
        case STRINGREF:     m_name = "string-ref"; break;
        case STRINGSET:     m_name = "string-set!"; break;
        case SUBSTRING:     m_name = "substring"; break;
        case STRINGAPPEND:  m_name = "string-append"; break;
        case STRINGCOPY:    m_name = "string-copy"; break;
        case STRINGUPCASE:  m_name = "string-upcase"; break;
        case STRINGDOWNCASE:m_name = "string-downcase"; break;
        case STRINGFOLDCASE:m_name = "string-foldcase"; break;
        case STRINGTOLIST:  m_name = "string->list"; break;
        case LISTTOSTRING:  m_name = "list->string"; break;
        case STRINGFILL:    m_name = "string-fill!"; break;

        // Case-sensitive comparisons
        case STRINGEQ:      m_name = "string=?"; break;
        case STRINGLT:      m_name = "string<?"; break;
        case STRINGGT:      m_name = "string>?"; break;
        case STRINGLE:      m_name = "string<=?"; break;
        case STRINGGE:      m_name = "string>=?"; break;

        // Case-insensitive comparisons
        case STRINGCIEQ:    m_name = "string-ci=?"; break;
        case STRINGCILT:    m_name = "string-ci<?"; break;
        case STRINGCIGT:    m_name = "string-ci>?"; break;
        case STRINGCILE:    m_name = "string-ci<=?"; break;
        case STRINGCIGE:    m_name = "string-ci>=?"; break;

        // R7RS additions
        case STRINGMAP:     m_name = "string-map"; break;
        case STRINGFOREACH: m_name = "string-for-each"; break;
    }
}

// ============================================================================
// Primitive Application
// ============================================================================

Object* PrimitiveStrings::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Arity checking
    if (!check_arity(args)) {
        return arity_error("");
    }

    // Extract arguments
    Object* x = first(args);
    Object* y = (rest(args) && !rest(args)->is_nil()) ? second(args) : nil;
    Object* z = (rest(args) && !rest(args)->is_nil() && rest(rest(args)) && !rest(rest(args))->is_nil()) ? third(args) : nil;

    // Dispatch based on Primitive ID
    switch (m_id) {
        // ====================================================================
        // Predicate
        // ====================================================================

        case STRINGQ:
            return truth(is_string(x));

        // ====================================================================
        // Construction
        // ====================================================================

        case MAKESTRING: {
            // (make-string k [char]) - create string of length k, optionally filled with char
            if (x == NULL || x->is_nil()) {
                return error("make-string: requires size argument");
            }
            int size = (int)num(x);
            if (size < 0) {
                return error("make-string: size must be non-negative");
            }
            char fill_char = ' ';  // default fill character is space
            if (y != NULL && !y->is_nil()) {
                a_char* ch = dynamic_cast<a_char*>(y);
                if (ch) {
                    fill_char = ch->value();
                }
            }
            std::string result(size, fill_char);
            return gc_new<a_string>(result);
        }

        case STRING:
            // (string char1 char2 ...) - construct string from characters
            return ListToString(args);

        // ====================================================================
        // Access and Mutation
        // ====================================================================

        case STRINGLENGTH: {
            if (!is_string(x)) {
                return error("string-length: argument must be a string");
            }
            a_string* str = dynamic_cast<a_string*>(x);
            return numeric::make_integer(str->value().length());
        }

        case STRINGREF: {
            if (!is_string(x)) {
                return error("string-ref: first argument must be a string");
            }
            if (y == NULL || y->is_nil()) {
                return error("string-ref: requires index argument");
            }
            a_string* str = dynamic_cast<a_string*>(x);
            int index = (int)num(y);
            if (index < 0 || index >= (int)str->value().length()) {
                return error("string-ref: index out of bounds");
            }
            return gc_new<a_char>(str->value()[index]);
        }

        case STRINGSET: {
            // (string-set! string k char) - set character at index k
            if (!is_string(x)) {
                return error("string-set!: first argument must be a string");
            }
            if (y == NULL || y->is_nil()) {
                return error("string-set!: requires index argument");
            }
            a_string* str = dynamic_cast<a_string*>(x);
            int index = (int)num(y);
            if (index < 0 || index >= (int)str->value().length()) {
                return error("string-set!: index out of bounds");
            }
            Object* z = third(args);
            if (z == NULL || z->is_nil()) {
                return error("string-set!: requires character argument");
            }
            a_char* ch = dynamic_cast<a_char*>(z);
            if (!ch) {
                return error("string-set!: third argument must be a character");
            }
            // Mutate the string in-place using non-const value()
            str->value()[index] = ch->value();
            // R5RS: string-set! returns unspecified value; we return the string
            return str;
        }

        case SUBSTRING: {
            // (substring string start end) - extract substring from start to end
            if (!is_string(x)) {
                return error("substring: first argument must be a string");
            }
            if (y == NULL || y->is_nil()) {
                return error("substring: requires start index");
            }
            if (z == NULL || z->is_nil()) {
                return error("substring: requires end index");
            }
            a_string* str = dynamic_cast<a_string*>(x);
            int start = (int)num(y);
            int end = (int)num(z);

            if (start < 0 || start > (int)str->value().length()) {
                return error("substring: start index out of bounds");
            }
            if (end < start || end > (int)str->value().length()) {
                return error("substring: end index out of bounds");
            }

            std::string result = str->value().substr(start, end - start);
            return gc_new<a_string>(result);
        }

        // ====================================================================
        // Operations
        // ====================================================================

        case STRINGAPPEND:
            // (string-append str1 str2 ...) - concatenate strings
            return string_append(args);

        case STRINGCOPY: {
            // (string-copy string) - create a copy of the string
            if (!is_string(x)) {
                return error("string-copy: argument must be a string");
            }
            a_string* str = dynamic_cast<a_string*>(x);
            return gc_new<a_string>(str->value());
        }

        case STRINGUPCASE: {
            // (string-upcase string) - convert string to uppercase
            if (!is_string(x)) {
                return error("string-upcase: argument must be a string");
            }
            a_string* str = dynamic_cast<a_string*>(x);
            std::string result = str->value();
            std::transform(result.begin(), result.end(), result.begin(), ::toupper);
            return gc_new<a_string>(result);
        }

        case STRINGDOWNCASE: {
            // (string-downcase string) - convert string to lowercase
            if (!is_string(x)) {
                return error("string-downcase: argument must be a string");
            }
            a_string* str = dynamic_cast<a_string*>(x);
            std::string result = str->value();
            std::transform(result.begin(), result.end(), result.begin(), ::tolower);
            return gc_new<a_string>(result);
        }

        case STRINGFOLDCASE: {
            // (string-foldcase string) - convert string using case folding
            // R7RS: For case-insensitive comparison, uses Unicode case folding
            // For ASCII simplicity, we use tolower (same as string-downcase for ASCII)
            if (!is_string(x)) {
                return error("string-foldcase: argument must be a string");
            }
            a_string* str = dynamic_cast<a_string*>(x);
            std::string result = str->value();
            std::transform(result.begin(), result.end(), result.begin(), ::tolower);
            return gc_new<a_string>(result);
        }

        case STRINGFILL: {
            // (string-fill! string char) - fills string with char (mutates in place)
            if (!is_string(x)) {
                return error("string-fill!: first argument must be a string");
            }
            if (!is_char(y)) {
                return error("string-fill!: second argument must be a character");
            }

            a_string* str = (a_string*)x;
            char fill_char = ((a_char*)y)->value();

            // Mutate the string in place by filling with character
            std::string& str_val = str->value();
            std::fill(str_val.begin(), str_val.end(), fill_char);

            // Returns unspecified value (we return the string)
            return str;
        }

        // ====================================================================
        // Conversions
        // ====================================================================

        case STRINGTOLIST: {
            // (string->list string) - convert string to list of characters
            if (!is_string(x)) {
                return error("string->list: argument must be a string");
            }
            a_string* str = dynamic_cast<a_string*>(x);
            std::string s = str->value();

            // Handle empty string case
            if (s.empty()) {
                return nil;
            }

            Object* result = nil;
            // Protect accumulator from GC during list building
            gc_root<Object> result_guard(result);

            // Build list from end to beginning to maintain order
            for (int i = (int)s.length() - 1; i >= 0; i--) {
                result = cons(gc_new<a_char>(s[i]), result);
            }
            return result;
        }

        case LISTTOSTRING: {
            // (list->string list) - convert a list of characters to a string
            if (x == NULL || x->is_nil()) {
                return gc_new<a_string>("");
            }

            std::string result = "";
            // Iterate through the list
            for (Object* item = x; item != NULL && !item->is_nil(); item = rest(item)) {
                Object* char_obj = first(item);

                // Check if item is a character
                if (!is_char(char_obj)) {
                    return error("list->string: all list elements must be characters");
                }

                // Get the character value and append it
                a_char* ch = dynamic_cast<a_char*>(char_obj);
                if (ch) {
                    result += ch->value();
                }
            }

            return gc_new<a_string>(result);
        }

        // ====================================================================
        // Case-Sensitive Comparisons
        // ====================================================================

        case STRINGEQ:
            return truth(stringCompare(x, y, false) == 0);

        case STRINGLT:
            return truth(stringCompare(x, y, false) < 0);

        case STRINGGT:
            return truth(stringCompare(x, y, false) > 0);

        case STRINGLE:
            return truth(stringCompare(x, y, false) <= 0);

        case STRINGGE:
            return truth(stringCompare(x, y, false) >= 0);

        // ====================================================================
        // Case-Insensitive Comparisons
        // ====================================================================

        case STRINGCIEQ:
            return truth(stringCompare(x, y, true) == 0);

        case STRINGCILT:
            return truth(stringCompare(x, y, true) < 0);

        case STRINGCIGT:
            return truth(stringCompare(x, y, true) > 0);

        case STRINGCILE:
            return truth(stringCompare(x, y, true) <= 0);

        case STRINGCIGE:
            return truth(stringCompare(x, y, true) >= 0);

        // ====================================================================
        // R7RS additions
        // ====================================================================

        case STRINGMAP: {
            // (string-map proc string1 string2 ...)
            // R7RS Section 6.7
            // Apply proc to corresponding characters of strings
            if (!eval) {
                return error("string-map: requires evaluator");
            }

            Object* proc = x;  // First argument is the Procedure
            Object* str_args = rest(args);  // Remaining arguments are strings

            if (!str_args || str_args->is_nil()) {
                return error("string-map: requires at least one string");
            }

            // Get first string to determine length
            Object* first_str_obj = first(str_args);
            if (!is_string(first_str_obj)) {
                return error("string-map: arguments must be strings");
            }
            a_string* first_str = (a_string*)first_str_obj;
            int len = static_cast<int>(first_str->value().length());

            // Verify all strings have same length
            Object* check_args = rest(str_args);
            while (check_args && !check_args->is_nil()) {
                Object* str_obj = first(check_args);
                if (!is_string(str_obj)) {
                    return error("string-map: arguments must be strings");
                }
                a_string* str = (a_string*)str_obj;
                if (str->value().length() != len) {
                    return error("string-map: all strings must have same length");
                }
                check_args = rest(check_args);
            }

            // Apply proc to each set of corresponding characters
            std::string result_str;
            for (int i = 0; i < len; i++) {
                // Gather characters at index i from all strings
                Object* proc_args = nil;
                Object* tail = nil;

                Object* str_list = str_args;
                while (str_list && !str_list->is_nil()) {
                    a_string* str = (a_string*)first(str_list);
                    char ch = str->value()[i];
                    Object* char_obj = gc_new<a_char>(ch);
                    Object* new_cell = cons(char_obj, nil);

                    if (proc_args == nil) {
                        proc_args = new_cell;
                        tail = new_cell;
                    } else {
                        set_rest(tail, new_cell);
                        tail = new_cell;
                    }

                    str_list = rest(str_list);
                }

                // Apply Procedure
                Object* result;
                if (proc->is_atom()) {
                    return error("string-map: first argument must be a Procedure");
                }
                Procedure* p = dynamic_cast<Procedure*>(proc);
                if (!p) {
                    return error("string-map: first argument must be a Procedure");
                }
                result = p->apply(eval, proc_args, env);

                // Result must be a character
                if (!is_char(result)) {
                    return error("string-map: Procedure must return a character");
                }
                a_char* ch_result = (a_char*)result;
                result_str += ch_result->value();
            }

            return gc_new<a_string>(result_str);
        }

        case STRINGFOREACH: {
            // (string-for-each proc string1 string2 ...)
            // R7RS Section 6.7
            // Apply proc to corresponding characters for side effects
            if (!eval) {
                return error("string-for-each: requires evaluator");
            }

            Object* proc = x;  // First argument is the Procedure
            Object* str_args = rest(args);  // Remaining arguments are strings

            if (!str_args || str_args->is_nil()) {
                return error("string-for-each: requires at least one string");
            }

            // Get first string to determine length
            Object* first_str_obj = first(str_args);
            if (!is_string(first_str_obj)) {
                return error("string-for-each: arguments must be strings");
            }
            a_string* first_str = (a_string*)first_str_obj;
            int len = static_cast<int>(first_str->value().length());

            // Verify all strings have same length
            Object* check_args = rest(str_args);
            while (check_args && !check_args->is_nil()) {
                Object* str_obj = first(check_args);
                if (!is_string(str_obj)) {
                    return error("string-for-each: arguments must be strings");
                }
                a_string* str = (a_string*)str_obj;
                if (str->value().length() != len) {
                    return error("string-for-each: all strings must have same length");
                }
                check_args = rest(check_args);
            }

            // Apply proc to each set of corresponding characters
            for (int i = 0; i < len; i++) {
                // Gather characters at index i from all strings
                Object* proc_args = nil;
                Object* tail = nil;

                Object* str_list = str_args;
                while (str_list && !str_list->is_nil()) {
                    a_string* str = (a_string*)first(str_list);
                    char ch = str->value()[i];
                    Object* char_obj = gc_new<a_char>(ch);
                    Object* new_cell = cons(char_obj, nil);

                    if (proc_args == nil) {
                        proc_args = new_cell;
                        tail = new_cell;
                    } else {
                        set_rest(tail, new_cell);
                        tail = new_cell;
                    }

                    str_list = rest(str_list);
                }

                // Apply Procedure (discard result)
                if (proc->is_atom()) {
                    return error("string-for-each: first argument must be a Procedure");
                }
                Procedure* p = dynamic_cast<Procedure*>(proc);
                if (!p) {
                    return error("string-for-each: first argument must be a Procedure");
                }
                p->apply(eval, proc_args, env);
            }

            return nil;  // R7RS: string-for-each returns unspecified value
        }

        default:
            return error("Unknown string Primitive");
    }
}

// ============================================================================
// Registration
// ============================================================================

void PrimitiveStrings::install(Environment* env) {
    const int n = 999999;

    // GC Safety: Use macro to protect name string before allocating primitive
    #define DEF(name, ...) do { \
        gc_root<a_string> _n(gc_new<a_string>(name)); \
        env->define(_n.get(), gc_new<PrimitiveStrings>(__VA_ARGS__)); \
    } while(0)

    // Predicate
    DEF("string?", STRINGQ, 1, 1);

    // Construction
    DEF("make-string", MAKESTRING, 1, 2);
    DEF("string",      STRING, 0, n);

    // Access and mutation
    DEF("string-length", STRINGLENGTH, 1, 1);
    DEF("string-ref",    STRINGREF, 2, 2);
    DEF("string-set!",   STRINGSET, 3, 3);
    DEF("substring",     SUBSTRING, 3, 3);

    // Operations
    DEF("string-append",   STRINGAPPEND, 0, n);
    DEF("string-copy",     STRINGCOPY, 1, 1);
    DEF("string-upcase",   STRINGUPCASE, 1, 1);
    DEF("string-downcase", STRINGDOWNCASE, 1, 1);
    DEF("string-foldcase", STRINGFOLDCASE, 1, 1);
    DEF("string-fill!",    STRINGFILL, 2, 2);

    // Conversions
    DEF("string->list", STRINGTOLIST, 1, 1);
    DEF("list->string", LISTTOSTRING, 1, 1);

    // Case-sensitive comparisons
    DEF("string=?",  STRINGEQ, 2, 2);
    DEF("string<?",  STRINGLT, 2, 2);
    DEF("string>?",  STRINGGT, 2, 2);
    DEF("string<=?", STRINGLE, 2, 2);
    DEF("string>=?", STRINGGE, 2, 2);

    // Case-insensitive comparisons
    DEF("string-ci=?",  STRINGCIEQ, 2, 2);
    DEF("string-ci<?",  STRINGCILT, 2, 2);
    DEF("string-ci>?",  STRINGCIGT, 2, 2);
    DEF("string-ci<=?", STRINGCILE, 2, 2);
    DEF("string-ci>=?", STRINGCIGE, 2, 2);

    // R7RS additions
    DEF("string-map",        STRINGMAP, 2, n);
    DEF("string-for-each",   STRINGFOREACH, 2, n);

    #undef DEF
}
