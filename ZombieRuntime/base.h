/**
 * @file Base.h
 * @brief Core utility functions and Base class for Scheme operations
 * @author chrislk@gmail.com
 * @date June 6, 2024
 *
 * This file provides the fundamental building blocks for Scheme list manipulation
 * and basic operations. It defines the Base class for higher-level objects and
 * includes essential functions like cons, car, cdr, append, reverse, etc.
 */
#pragma once

#include "atom.h"
#include "cons_cell.h"
#include "constants.h"
#include <vector>

namespace zombie { namespace runtime {

/**
 * @brief Base class for higher-level procedural entities
 *
 * Serves as the foundation for procedures, closures, macros, and other
 * complex Scheme objects. Provides type identification via atype enum.
 *
 * @note Most Scheme values inherit from Object directly; Base is for
 *       special procedural types (closures, primitives, continuations)
 */
class Base : public Object {
public:
    /**
     * @brief Type identifiers for runtime type checking
     *
     * Used by instance_of() to identify specific subclass types without RTTI.
     * Enables safe downcasting and type-specific behavior.
     */
    enum class atype {
        CLOSURE = 0,                    ///< Lambda Closure (user-defined function)
        MACRO,                          ///< Macro transformer
        PROCEDURE,                      ///< Base procedural type
        BASE,                           ///< Generic Base Object
        PRIMITIVE,                      ///< Built-in Primitive function
        LISP,                           ///< Lisp evaluator instance
        LAMBDA,                         ///< Lambda expression (deprecated, use CLOSURE)
        ERROR,                          ///< Error Object (includes source/line info)
        INPUTPORT,                      ///< Input port for reading
        OUTPUTPORT,                     ///< Output port for writing
        TYPE_OBJECT,                    ///< Meta-Object Protocol: First-class types
        SYNTAX_RULES_TRANSFORMER,       ///< R5RS hygienic Macro transformer
        VALUES,                         ///< R5RS multiple return values
    };

public:
    /**
     * @brief Construct a Base Object
     */
    Base();

    /**
     * @brief Copy constructor
     * @param other Object to copy from
     */
    Base(const Base& other);

    /**
     * @brief Destroy the Base Object
     */
    virtual ~Base();

    /**
     * @brief Get runtime type identifier
     * @return atype enum value (BASE for this class)
     */
    virtual int instance_of() { return static_cast<int>(atype::BASE); }

    /**
     * @brief Create a shallow copy of this Object
     * @return Pointer to cloned Object
     */
    virtual Object* clone() const;

    /**
     * @brief Print Object to output stream
     * @param os Output stream
     */
    virtual void print(std::ostream& os) const;

    /**
     * @brief Check if this is an atom
     * @return 1 if atom, 0 otherwise
     */
    virtual int is_atom() const;

    /**
     * @brief Check if this is a pair
     * @return 1 if pair, 0 otherwise
     */
    virtual int pair() const;

    /**
     * @brief Stream insertion operator
     */
    friend std::ostream& operator << (std::ostream& os, const Object* obj);

    /**
     * @brief Get type code
     * @return Type identifier (0 for Base)
     */
    int get_type() const { return 0; }

    /**
     * @brief Trace references for garbage collection
     * @param tracer GC tracer Object
     */
    void trace_references(Tracer& tracer) override;
};

/**
 * @brief Create a Scheme list from a C++ vector
 * @tparam T Element type (must be atom-compatible)
 * @param vec Vector of values to convert
 * @return Scheme list containing the vector elements
 * @note Each element is wrapped in atom<T> before consing
 */
template <class T>
Object* make_list(std::vector<T> vec) {
    Object* list_ret = nullptr;
    for (auto& o : vec)
    {
        if (list_ret == nullptr)
        {
            list_ret = cons(gc_new<atom<T>>(o), nil);
        }
        else
        {
            list_ret = append(list_ret, gc_new<atom<T>>(o));
        }
    }
    return list_ret;
}

// ============================================================================
// Truthiness and Equality Functions
// ============================================================================

/**
 * @brief Convert boolean to Scheme truth value
 * @param x C++ boolean
 * @return #t if true, #f if false
 */
Object* truth(bool x);

/**
 * @brief Test if Object is true in Scheme semantics
 * @param x Object to test
 * @return true unless x is #f (false Object)
 * @note In Scheme, everything except #f is considered true (including nil, 0, "")
 */
bool truth(Object* x);

/**
 * @brief Test if two objects are equivalent (eqv? predicate)
 * @param x First Object
 * @param y Second Object
 * @return true if objects are equivalent
 * @note R5RS eqv?: Same as eq? for most types, compares values for numbers/chars
 */
bool eqv(Object* x, Object* y);

/**
 * @brief Test structural equality (equal? predicate)
 * @param x First Object
 * @param y Second Object
 * @return true if objects are structurally equal
 * @note R5RS equal?: Recursively compares lists and other structures
 */
bool equal(Object* x, Object* y);

// ============================================================================
// List Construction and Manipulation (R5RS primitives)
// ============================================================================

/**
 * @brief Append two lists (R5RS append)
 * @param e1 First list
 * @param e2 Second list (or final cdr element)
 * @return New list with e2 appended to e1
 * @note Creates a new list; does not modify originals
 * @note If e1 is nil, returns e2 directly
 * @note Time complexity: O(n) where n is length of e1
 */
Object* append(const Object* e1, const Object* e2);

/**
 * @brief Reverse a list (R5RS reverse)
 * @param lst List to reverse
 * @return New list with elements in reverse order
 * @note Creates a new list; does not modify original
 * @note Time complexity: O(n)
 */
Object* reverse(Object* lst);

/**
 * @brief Get first element of pair (R5RS car)
 * @param e Pair to extract from
 * @return First element (car) of the pair
 * @note Returns nil and prints error if applied to non-pair
 *
 * Example:
 * @code
 * Object* pair = cons(a, b);
 * Object* first_elem = car(pair);  // Returns a
 * @endcode
 */
Object* car(const Object* e);

/**
 * @brief Get second element of pair (R5RS cdr)
 * @param e Pair to extract from
 * @return Second element (cdr) of the pair
 * @note Returns nil and prints error if applied to non-pair
 *
 * Example:
 * @code
 * Object* pair = cons(a, b);
 * Object* rest = cdr(pair);  // Returns b
 * @endcode
 */
Object* cdr(const Object* e);

/**
 * @brief Test if Object is nil (R5RS null?)
 * @param e Object to test
 * @return #t if nil, #f otherwise
 */
Object* null(Object* e);

/**
 * @brief Check if Object is a proper list
 * @param l Object to test
 * @return 1 if proper list, 0 otherwise
 * @note Detects circular lists (returns 0 if length > 100000)
 */
int is_lisp_list(Object* l);

/**
 * @brief Check if Object is a non-empty proper list
 * @param l Object to test
 * @return 1 if non-empty list, 0 otherwise
 */
int is_nonempty_lisp_list(Object* l);

/**
 * @brief Evaluate cond expression (R5RS cond)
 * @param e Cond clauses ((test1 expr1...) (test2 expr2...) ...)
 * @return Result of first clause whose test succeeds
 * @note Internal helper for cond special form
 */
Object* cond(Object* e);

/**
 * @brief Get length of list (R5RS length)
 * @param x List to measure
 * @return Number of elements in list
 * @note Returns 0 for nil, -1 for non-lists
 */
int length(Object* x);

// ============================================================================
// Error Reporting
// ============================================================================

/**
 * @brief Create error message string
 * @param s Error message
 * @return String Object containing error message
 * @note Returns a_string Object, does not throw exception
 */
a_string* error(std::string s);

/**
 * @brief Create warning message string
 * @param message Warning message
 * @return String Object containing warning
 */
a_string* warn(std::string message);

// ============================================================================
// Common Lisp Style Accessors
// ============================================================================

/**
 * @brief Get first element of list (same as car)
 * @param x List
 * @return First element
 * @note Common Lisp compatibility function
 */
Object* first(const Object* x);

/**
 * @brief Get rest of list (same as cdr)
 * @param e List
 * @return Rest of list after first element
 * @note Common Lisp compatibility function
 */
Object* rest(const Object* e);

/**
 * @brief Get second element of list (cadr)
 * @param x List
 * @return Second element
 */
Object* second(const Object* x);

/**
 * @brief Get third element of list (caddr)
 * @param x List
 * @return Third element
 */
Object* third(const Object* x);

/**
 * @brief Create a two-element list
 * @param a First element
 * @param b Second element
 * @return List containing (a b)
 */
ConsCell* list(Object* a, Object* b);

/**
 * @brief Set first element of pair (set-car!)
 * @param x Pair to modify
 * @param y New first element
 * @return Modified pair
 * @note Mutates the pair in place
 */
Object* set_first(Object* x, Object* y);

/**
 * @brief Set second element of pair (set-cdr!)
 * @param x Pair to modify
 * @param y New second element
 * @return Modified pair
 * @note Mutates the pair in place
 */
Object* set_rest(Object* x, Object* y);

// ============================================================================
// Type Conversion
// ============================================================================

/**
 * @brief Extract numeric value from Object
 * @param x Numeric Object
 * @return Double-precision floating point value
 * @note Assumes x is a numeric type
 */
double num(Object* x);

/**
 * @brief Check if Object is a pair
 * @param x Object to test
 * @return true if pair
 */
bool pair(Object* x);

/**
 * @brief Check if Object is a list
 * @param x Object to test
 * @return true if proper list (nil-terminated)
 */
bool is_list(Object* x);

} } // namespace zombie::runtime

