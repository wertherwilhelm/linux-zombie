#pragma once

#include "Object.h"
#include "atom.h"  // For Type enum (BOOL constant)

namespace zombie { namespace runtime {

/**
 * @file constants.h
 * @brief Scheme singletons and symbol interning helpers.
 *
 * R5RS Scheme Compliance Update (December 2025):
 *
 * Unlike Common Lisp, R5RS Scheme distinguishes between:
 * - Empty list '() - A distinct value (not false)
 * - Boolean #f - The only false value
 * - Boolean #t - The canonical true value
 *
 * R5RS Section 6.3.2: "The empty list is a special Object of its own type.
 * It is not a pair, it has no elements, and its length is zero."
 *
 * R5RS Section 6.1: "Of all the Scheme values, only #f counts as false in
 * conditional expressions. Except for #f, all Scheme values, including #t,
 * pairs, the empty list, symbols, numbers, strings, vectors, and procedures,
 * count as true."
 *
 * This is a BREAKING CHANGE from the previous Common Lisp-style implementation
 * where nil served as both empty list and false.
 */

// ============================================================================
// Empty List Singleton
// ============================================================================
/**
 * @brief Represents the empty list '() in R5RS Scheme.
 *
 * Distinct from #f and considered true in boolean contexts.
 */
class empty_list_class : public Object
{
public:
    empty_list_class() {}

    void print(std::ostream& s) const {
        s << "()";
    }

    Object* clone(void) const {
        return (Object*)this;
    }

    int is_atom(void) const {
        return 1;
    }

    int pair(void) const {
        return 0;
    }

    int get_type() const {
        return 0;  // Special type for empty list
    }

    // Empty list specific predicate
    virtual int is_nil() const {
        return 1;  // For backward compatibility
    }

    void trace_references(Tracer& tracer) override {
        // Empty list is a singleton with no references
    }
};

// ============================================================================
// Boolean False Singleton
// ============================================================================
/**
 * @brief Represents #f in R5RS Scheme (the only false value).
 */
class boolean_false_class : public Object
{
public:
    boolean_false_class() {}

    void print(std::ostream& s) const {
        s << "#f";
    }

    Object* clone(void) const {
        return (Object*)this;
    }

    int is_atom(void) const {
        return 1;
    }

    int pair(void) const {
        return 0;
    }

    int get_type() const {
        return static_cast<int>(Type::BOOL);
    }

    // This is NOT the empty list
    virtual int is_nil() const {
        return 0;
    }

    void trace_references(Tracer& tracer) override {
        // Boolean false is a singleton with no references
    }
};

// ============================================================================
// Boolean True Singleton
// ============================================================================
/**
 * @brief Represents #t in R5RS Scheme.
 */
class __nil_and_t_lisp_class : public Object
{
public:
    __nil_and_t_lisp_class() {}

    void print(std::ostream& s) const {
        s << "#t";
    }

    Object* clone(void) const {
        return (Object*)this;
    }

    int is_atom(void) const {
        return 1;
    }

    int pair(void) const {
        return 0;
    }

    int get_type() const {
        return static_cast<int>(Type::BOOL);
    }

    // #t is not nil
    virtual int is_nil() const {
        return 0;
    }

    virtual int is_t() const {
        return 1;
    }

    void trace_references(Tracer& tracer) override {
        // #t is a singleton with no references
    }
};

// ============================================================================
// Global Singletons
// ============================================================================

/// R5RS Scheme values.
extern empty_list_class* empty_list;      // '() - The empty list (truthy!)
extern boolean_false_class* false_obj;    // #f - Boolean false (the ONLY false value)
extern __nil_and_t_lisp_class* t;         // #t - Boolean true

// BACKWARD COMPATIBILITY ALIAS:
// During migration, 'nil' points to empty_list to minimize breakage
// Most code should be migrated to use empty_list or false_obj explicitly
extern empty_list_class* nil;             // DEPRECATED: Use empty_list or false_obj

/**
 * @brief Initialize empty_list, false_obj, and t with GC.
 *
 * Must be called before using the Lisp system.
 */
void init_lisp_constants();

// ============================================================================
// Symbol Interning
// ============================================================================

// Forward declaration
class a_symbol;

/**
 * @brief Get or create an interned symbol
 * @param name Symbol name (without leading quote)
 * @return Pointer to interned symbol (always the same Object for same name)
 *
 * Symbol interning ensures that all symbols with the same name share the same
 * Object identity. This is required for R5RS compliance:
 * - (eq? 'foo 'foo) must return #t
 * - Symbols can be compared with fast pointer equality (eq?)
 *
 * Implementation:
 * - Maintains a global symbol table (name -> symbol Object)
 * - Thread-safe using mutex
 * - All symbols registered as GC roots (never collected)
 *
 * Example:
 *   a_symbol* sym1 = intern_symbol("foo");
 *   a_symbol* sym2 = intern_symbol("foo");
 *   // sym1 == sym2 (same pointer)
 */
a_symbol* intern_symbol(const std::string& name);

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Check if Object is the empty list '().
 */
inline bool is_empty_list(const Object* obj) {
    return obj == empty_list;
}

/**
 * @brief Check if Object is boolean #f (the only false value in R5RS).
 */
inline bool is_false(const Object* obj) {
    return obj == false_obj;
}

/**
 * @brief Check if Object is boolean #t.
 */
inline bool is_true(const Object* obj) {
    return obj == t;
}

/**
 * @brief Check if Object is a boolean (#t or #f).
 */
inline bool is_boolean(const Object* obj) {
    return obj == t || obj == false_obj;
}

// Note: is_nil() is a virtual method on Object class
// Use obj->is_nil() or is_empty_list(obj) depending on intent

Object* lnot(Object*);

} } // namespace zombie::runtime
