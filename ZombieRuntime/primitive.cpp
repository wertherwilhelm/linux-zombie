#include "pch.h"
// Primitive.cpp: implementation of the Primitive class.
//
//////////////////////////////////////////////////////////////////////

#include "primitive.h"
#include "procedure.h"
#include "Environment.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include "Macro.h"
#include "a_vector.h"
#include "constants.h"
#include "numeric_runtime.h"
#include "a_rational.h"
#include "a_complex.h"
#include "InputPort.h"
#include "OutputPort.h"
#include "FileInputPort.h"
#include "FileOutputPort.h"
#include "type_object.h"
#include "scheme_reader.h"
#include "values_object.h"
#include "bytecode_vm.h"
#include "i_debugger.h"
// New inheritance-based Primitive classes
#include "primitives/primitive_booleans.h"
#include "primitives/primitive_equivalence.h"
#include "primitives/primitive_lists.h"
#include "primitives/primitive_symbols.h"
#include "primitives/primitive_numbers.h"
#include "primitives/primitive_characters.h"
#include "primitives/primitive_strings.h"
#include "primitives/primitive_vectors.h"
#include "primitives/primitive_bytevectors.h"
#include "primitives/primitive_exceptions.h"
#include "primitives/primitive_parameters.h"
#include "primitives/primitive_control.h"
#include "primitives/primitive_io.h"
#include "primitives/primitive_system.h"
#include "primitives/primitive_value_ledger.h"
#include "primitives/primitive_necromancy.h"
// Legacy Primitive groups (still needed for scenegraph, etc.)
#include "primitives/primitives_value_ledger.h"  // For implementation functions
#include "primitives/primitives_scenegraph.h"
#include "primitives/primitives_export_registry.h"
#include <cmath>
#include <cctype>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <bitset>


namespace zombie { namespace runtime {

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

// R5RS: #f is the only false value
#define FALSE false_obj
#define ZERO numeric::make_integer(0)
#define ONE numeric::make_integer(1)

Primitive::Primitive() {
    m_id_number = -1; // should be something like noop
    m_minArgs = 0;
    m_maxArgs = 0;
}

Primitive::Primitive(std::string name) {
    m_name = name;
    m_id_number = -1; // should be something like noop
    m_minArgs = 0;
    m_maxArgs = 0;
}


Primitive::Primitive(const Primitive& p) {
    *this = p;
}

Primitive::Primitive(int id, int minArgs, int maxArgs) {
    m_id_number = id;
    m_minArgs = minArgs;
    m_maxArgs = maxArgs;
}

Primitive::~Primitive() {

}

Object* Primitive::clone() const {
    Primitive* p = new Primitive(*this);
    return p;
}

void Primitive::print(std::ostream& os) const {
    os << "(**PRIMITIVE**" << m_id_number << "," << m_minArgs << "," << m_maxArgs << ")";
}

Environment* Primitive::install_primitives(Environment* env) {
    // Install all R5RS Primitive groups using new inheritance-based classes
    PrimitiveBooleans::install(env);
    PrimitiveEquivalence::install(env);
    PrimitiveLists::install(env);
    PrimitiveSymbols::install(env);
    PrimitiveNumbers::install(env);
    PrimitiveCharacters::install(env);
    PrimitiveStrings::install(env);
    PrimitiveVectors::install(env);
    PrimitiveBytevectors::install(env);  // R7RS bytevectors
    PrimitiveExceptions::install(env);   // R7RS exception handling
    PrimitiveParameters::install(env);   // R7RS parameters
    PrimitiveControl::install(env);
    PrimitiveIO::install(env);
    PrimitiveSystem::install(env);

    // Install value-ledger primitives (OOP class)
    PrimitiveValueLedger::install(env);
    // Install GC graveyard primitives (graveyard, epitaph, seance, ...)
    PrimitiveNecromancy::install(env);
    // Install export/signature registry primitives
    PrimitiveExportRegistry::install(env);
    // Note: scenegraph excluded - embedders must register explicitly
    // Note: void is now in PrimitiveSystem

    return env;

}

// ============================================================================
// SANDBOX AND DEBUG PRIMITIVE INSTALLATION
// Uses OOP classes directly - no legacy def_prim needed
// ============================================================================

// Install safe primitives for sandbox (no reflection, no sub-interpreters)
Environment* Primitive::install_sandbox_primitives(Environment* env) {
    // Core primitives safe for sandboxed environments
    PrimitiveBooleans::install(env);
    PrimitiveEquivalence::install(env);
    PrimitiveNumbers::install(env);
    PrimitiveLists::install(env);
    PrimitiveSymbols::install(env);
    PrimitiveCharacters::install(env);
    PrimitiveStrings::install(env);
    PrimitiveVectors::install(env);
    PrimitiveControl::install(env);  // apply, map, for-each, eval, call/cc, dynamic-wind
    PrimitiveIO::install(env);       // Note: may want to restrict file I/O later
    PrimitiveSystem::install(env);   // exit, command-line, features, void

    // NOT included for sandbox (security/isolation):
    // - PrimitiveExceptions (error handling control flow)
    // - PrimitiveParameters (parameterize system control)
    // - PrimitiveBytevectors (R7RS extension, not essential)
    // - PrimitiveValueLedger (value introspection)
    // - PrimitiveExportRegistry (module system)
    // - make-interpreter, reflection primitives (not in OOP classes)

    return env;
}

// Install debug-enhanced primitives (same as install_primitives)
Environment* Primitive::install_debug_primitives(Environment* env) {
    // Debug mode uses full primitive set
    // Note: Legacy debug tools (time-call, macro-expand) were removed
    // as they were unused. Add to PrimitiveSystem if needed.
    return install_primitives(env);
}

// Helper function to convert list to vector
a_vector* list_to_vector(Object* lst) {
    if (lst == NULL || lst->is_nil()) {
        return gc_new<a_vector>();
    }

    std::vector<Object*> elements;
    Object* current = lst;

    while (current != NULL && !current->is_nil() && !current->is_atom()) {
        elements.push_back(car(current));
        current = cdr(current);
    }

    return gc_new<a_vector>(elements);
}

// Helper function to convert vector to list
Object* vector_to_list(Object* vec) {
    if (!is_vector(vec)) {
        return error("vector_to_list: argument must be a vector");
    }

    a_vector* v = (a_vector*)vec;
    Object* result = nil;

    // Protect accumulator from GC during list building
    gc_root<Object> result_guard(result);

    // Build list in reverse order
    for (int i = v->length() - 1; i >= 0; i--) {
        result = cons(v->get(i), result);
    }

    return result;
}

// Helper function to extract char from Object
char chr(Object* obj) {
    if (obj == NULL || obj->is_nil()) {
        return '\0';
    }
    a_char* ch = dynamic_cast<a_char*>(obj);
    if (ch) {
        return ch->value();
    }
    return '\0';
}

// Helper function to create char Object
Object* chr(char c) {
    return gc_new<a_char>(c);
}

// Helper function to compare characters
int char_compare(Object* x, Object* y, bool caseInsensitive) {
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

// Helper function to compare strings
static int cmp_strings(Object* x, Object* y, bool caseInsensitive) {
    if (!is_string(x) || !is_string(y)) {
        return 0;
    }

    a_string* s1 = dynamic_cast<a_string*>(x);
    a_string* s2 = dynamic_cast<a_string*>(y);

    if (!s1 || !s2) {
        return 0;
    }

    std::string str1 = s1->value();
    std::string str2 = s2->value();

    if (caseInsensitive) {
        // Convert both strings to lowercase for comparison
        std::transform(str1.begin(), str1.end(), str1.begin(), ::tolower);
        std::transform(str2.begin(), str2.end(), str2.begin(), ::tolower);
    }

    if (str1 < str2) return -1;
    if (str1 > str2) return 1;
    return 0;
}

// Helper function: number->string
Object* number_to_string(Object* x, Object* y) {
    if (!numeric::is_number(x)) {
        return error("number->string: expected a number");
    }

    int radix = 10;
    if (y != NULL && y != nil) {
        if (!numeric::is_number(y)) {
            return error("number->string: radix must be a number");
        }
        radix = (int)num(y);
        if (radix != 2 && radix != 8 && radix != 10 && radix != 16) {
            return error("number->string: radix must be 2, 8, 10, or 16");
        }
    }

    long long value = (long long)num(x);
    std::ostringstream oss;

    if (radix == 10) {
        // For Base 10, handle floating point
        double dval = num(x);
        if (dval == (double)value) {
            // Integer value
            oss << value;
        } else {
            // Floating point value
            oss << dval;
        }
    } else {
        // For other bases, use integer conversion
        if (radix == 2) {
            oss << std::bitset<64>(value);
            // Remove leading zeros
            std::string str = oss.str();
            size_t pos = str.find('1');
            if (pos != std::string::npos) {
                return gc_new<a_string>(str.substr(pos));
            } else {
                return gc_new<a_string>("0");
            }
        } else if (radix == 8) {
            oss << std::oct << value;
        } else if (radix == 16) {
            oss << std::hex << value;
        }
    }

    return gc_new<a_string>(oss.str());
}

// Helper function: string->number
Object* string_to_number(Object* x, Object* y) {
    if (!is_string(x)) {
        return error("string->number: expected a string");
    }

    int radix = 10;
    if (y != NULL && y != nil) {
        if (!is_double(y)) {
            return error("string->number: radix must be a number");
        }
        radix = (int)num(y);
        if (radix != 2 && radix != 8 && radix != 10 && radix != 16) {
            return error("string->number: radix must be 2, 8, 10, or 16");
        }
    }

    std::string str = ((a_string*)x)->value();

    try {
        if (radix == 10) {
            // Try to parse as double
            size_t pos;
            double value = std::stod(str, &pos);
            if (pos == str.length()) {
                return numeric::make_inexact(value);
            }
        } else {
            // Parse as integer in given radix
            size_t pos;
            long long value = std::stoll(str, &pos, radix);
            if (pos == str.length()) {
                return numeric::make_integer(value);
            }
        }
    } catch (const std::invalid_argument&) {
        // Return #f for invalid conversion
        return nil;  // R5RS: string->number returns #f on failure
    } catch (const std::out_of_range&) {
        return nil;  // Number too large to represent
    }

    // Return #f if parsing failed
    return nil;
}

// Helper function: compute GCD of two numbers
long long gcd_helper(long long a, long long b) {
    a = std::abs(a);
    b = std::abs(b);
    while (b != 0) {
        long long temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

// Helper function: gcd of multiple numbers
Object* gcd(Object* args) {
    if (args == NULL || args == nil) {
        return ZERO;
    }

    // Get first number
    if (!numeric::is_number(first(args))) {
        return error("gcd: expected numbers");
    }

    long long result = (long long)num(first(args));
    args = rest(args);

    while (args != NULL && args != nil) {
        if (!numeric::is_number(first(args))) {
            return error("gcd: expected numbers");
        }
        long long next = (long long)num(first(args));
        result = gcd_helper(result, next);
        args = rest(args);
    }

    return numeric::make_integer(std::abs(result));
}

// Helper function: compute LCM of two numbers
long long lcm_helper(long long a, long long b) {
    if (a == 0 || b == 0) {
        return 0;
    }
    return std::abs(a * b) / gcd_helper(a, b);
}

// Helper function: lcm of multiple numbers
Object* lcm(Object* args) {
    if (args == NULL || args == nil) {
        return ONE;
    }

    // Get first number
    if (!numeric::is_number(first(args))) {
        return error("lcm: expected numbers");
    }

    long long result = (long long)num(first(args));
    args = rest(args);

    while (args != NULL && args != nil) {
        if (!numeric::is_number(first(args))) {
            return error("lcm: expected numbers");
        }
        long long next = (long long)num(first(args));
        result = lcm_helper(result, next);
        args = rest(args);
    }

    return numeric::make_integer(std::abs(result));
}

// Helper function: check if a number is exact
bool is_exact(Object* x) {
    return numeric::is_exact(x);
}

// Helper function: check if an Object is a Procedure
bool is_procedure(Object* x) {
    if (x == NULL || x == nil) {
        return false;
    }
    Base* b = dynamic_cast<Base*>(x);
    if (b == NULL) {
        return false;
    }
    int type = b->instance_of();
    return (type == static_cast<int>(Base::atype::PROCEDURE) ||
            type == static_cast<int>(Base::atype::PRIMITIVE) ||
            type == static_cast<int>(Base::atype::CLOSURE) ||
            type == static_cast<int>(Base::atype::MACRO));
}

// Helper function for APPLY: properly append two lists
// This manually builds a new list to avoid the append() function's bug
Object* append_lists_for_apply(Object* list1, Object* list2) {
    // If first list is nil, return second list
    if (list1 == NULL || list1 == nil) {
        return list2;
    }

    // Build a new list by consing elements from list1 in reverse, then reversing
    // Or better: recursively build it
    Object* first_elem = first(list1);
    Object* rest_of_list1 = rest(list1);

    if (rest_of_list1 == NULL || rest_of_list1 == nil) {
        // Last element of list1 - cons it with list2
        return gc_new<ConsCell>(first_elem, list2);
    } else {
        // Recursively append - protect intermediate result
        Object* rest_result = append_lists_for_apply(rest_of_list1, list2);
        gc_root<Object> rest_guard(rest_result);
        return gc_new<ConsCell>(first_elem, rest_result);
    }
}

Object* Primitive::apply(IEvaluator * evaluator, Object *args, Environment* env)
{
    // ========================================================================
    // LEGACY DISPATCH - NO LONGER USED
    // ========================================================================
    // All primitive dispatch now goes through OOP classes:
    // - PrimitiveBooleans, PrimitiveNumbers, PrimitiveLists, etc.
    //
    // This method should never be called. If you see this error, a Primitive
    // object was created with an ID instead of using the OOP primitive classes.
    // ========================================================================

    std::cerr << "ERROR: Primitive::apply() called on legacy Primitive with ID: "
              << m_id_number << ", name: " << m_name << std::endl;
    std::cerr << "This indicates a bug - all primitives should use OOP classes." << std::endl;

    return error("Internal error: Legacy Primitive::apply() called. ID=" +
                 std::to_string(m_id_number) + " name=" + m_name);
}

void Primitive::trace_references(Tracer& tracer) {
    // Primitives don't hold references to other objects
    // They're stateless functions
}

} } // namespace zombie::runtime

/**/