#include "pch.h"
#include "../pch.h"
#include "primitive_symbols.h"
#include "../Base.h"
#include "../atom.h"
#include "../constants.h"  // For intern_symbol()

/**
 * @file primitive_symbols.cpp
 * @brief Implementation of symbol primitives (R5RS Section 6.4)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 */

// ============================================================================
// Constructor
// ============================================================================

PrimitiveSymbols::PrimitiveSymbols(ID id)
    : PrimitiveBase(1, 1),  // All symbol primitives take exactly 1 argument
      m_id(id) {
    // Set Primitive name based on ID
    switch (id) {
        case SYMBOLQ:
            m_name = "symbol?";
            break;

        case SYMBOLTOSTRING:
            m_name = "symbol->string";
            break;

        case STRINGTOSYMBOL:
            m_name = "string->symbol";
            break;
    }
}

// ============================================================================
// Primitive Application
// ============================================================================

Object* PrimitiveSymbols::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Arity checking
    if (!check_arity(args)) {
        return arity_error("");
    }

    // Extract the single argument
    Object* x = first(args);

    // Dispatch based on Primitive ID
    switch (m_id) {
        case SYMBOLQ: {
            // (symbol? obj)
            // Symbols have type SYMBOL, strings have type STRING
            // Check specifically for SYMBOL type
            return truth(x->get_type() == static_cast<int>(Type::SYMBOL));
        }

        case SYMBOLTOSTRING: {
            // (symbol->string symbol)
            // Convert symbol to string (creates a new string Object with same value)
            if (!is_symbol(x)) {
                return error("symbol->string: expected a symbol");
            }
            // Return a new string with the same value
            return gc_new<a_string>(((a_symbol*)x)->value());
        }

        case STRINGTOSYMBOL: {
            // (string->symbol string)
            // Convert string to symbol
            if (!is_string(x)) {
                return error("string->symbol: expected a string");
            }
            // Return a new symbol Object
            return intern_symbol(((a_string*)x)->value());
        }

        default:
            return error("Unknown symbol Primitive");
    }
}

// ============================================================================
// Registration
// ============================================================================

void PrimitiveSymbols::install(Environment* env) {
    // Register all symbol primitives
    // GC Safety: Protect name string before allocating primitive
    gc_root<a_string> name_symbolq(gc_new<a_string>("symbol?"));
    env->define(name_symbolq.get(), gc_new<PrimitiveSymbols>(SYMBOLQ));

    gc_root<a_string> name_sym2str(gc_new<a_string>("symbol->string"));
    env->define(name_sym2str.get(), gc_new<PrimitiveSymbols>(SYMBOLTOSTRING));

    gc_root<a_string> name_str2sym(gc_new<a_string>("string->symbol"));
    env->define(name_str2sym.get(), gc_new<PrimitiveSymbols>(STRINGTOSYMBOL));
}
