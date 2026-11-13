/**
 * @file ConsCell.h
 * @brief Cons cell implementation - the fundamental building block of Scheme lists
 * @author chrislk@gmail.com
 * @date 2024-06
 *
 * In Scheme, lists are built from cons cells (pairs). Each cons cell contains
 * two pointers: car (first element) and cdr (rest of list). A proper list is
 * terminated by nil in the final cdr.
 *
 * Examples:
 * - (cons 1 2) creates a pair: (1 . 2)
 * - (cons 1 nil) creates a single-element list: (1)
 * - (cons 1 (cons 2 nil)) creates: (1 2)
 */
#pragma once

#include "Object.h"

namespace zombie { namespace runtime {

/**
 * @brief Pair/cons cell - the fundamental list structure in Scheme
 *
 * A cons cell is a record structure containing two components called the car
 * and the cdr. Cons cells are the building blocks of Scheme lists.
 *
 * **Structure:**
 * - car: First element (can be any Object)
 * - cdr: Rest of list (typically another ConsCell or nil)
 *
 * **List Representation:**
 * - Proper list: (1 2 3) = (cons 1 (cons 2 (cons 3 nil)))
 * - Improper list: (1 . 2) = (cons 1 2)
 * - Empty list: nil (singleton, not a ConsCell)
 *
 * **Memory Management:**
 * - Participates in garbage collection via trace_references()
 * - Car and cdr are traced during GC mark phase
 *
 * @see cons() for construction
 * @see car() and cdr() for access
 * @see append() for list concatenation
 */
class ConsCell : public Object
{
public:
    /**
     * @brief Construct empty cons cell (car and cdr are nil)
     */
    ConsCell();

    /**
     * @brief Construct cons cell with car only (cdr is nil)
     * @param car_val First element
     * @note Creates single-element list
     */
    ConsCell(const Object* car_val);

    /**
     * @brief Construct cons cell with both car and cdr
     * @param car_val First element
     * @param cdr_val Rest of list (or second element for dotted pair)
     * @note Most common constructor for list building
     */
    ConsCell(const Object* car_val, const Object* cdr_val);

    /**
     * @brief Copy constructor
     * @param other Cons cell to copy
     * @note Creates shallow copy (car/cdr point to same objects)
     */
    ConsCell(const ConsCell& other);

    /**
     * @brief Destructor
     */
    virtual ~ConsCell();

    /**
     * @brief Assignment operator
     * @param other Cons cell to assign from
     * @return Reference to this
     */
    ConsCell& operator = (const ConsCell& other);

    /**
     * @brief Get first element (car accessor)
     * @return Pointer to car Object
     * @note R5RS car operation
     */
    Object* car() const;

    /**
     * @brief Get rest of list (cdr accessor)
     * @return Pointer to cdr Object
     * @note R5RS cdr operation
     */
    Object* cdr() const;

    /**
     * @brief Set first element (set-car! mutator)
     * @param newm_car New car value
     * @note Mutates the cons cell in place
     */
    void car(const Object* newm_car);

    /**
     * @brief Set rest of list (set-cdr! mutator)
     * @param newm_cdr New cdr value
     * @note Mutates the cons cell in place
     */
    void cdr(const Object* newm_cdr);

    /**
     * @brief Set cdr without cloning (internal optimization)
     * @param e New cdr value
     * @note Used by append() to avoid unnecessary cloning
     * @warning Internal use only - does not clone the argument
     */
    void set_cdr_noclone(Object* e) { m_cdr = e; }

    /**
     * @brief Create a shallow copy of this cons cell
     * @return Pointer to new cons cell
     * @note Car and cdr point to same objects as original
     */
    Object* clone() const;

    /**
     * @brief Print cons cell to output stream
     * @param os Output stream
     * @note Prints as (car . cdr) for pairs, (1 2 3) for lists
     */
    void print(std::ostream& os) const;

    /**
     * @brief Check if this is an atom
     * @return 0 (cons cells are never atoms)
     */
    int is_atom() const;

    /**
     * @brief Check if this is a pair
     * @return 1 (cons cells are always pairs)
     */
    int pair() const;

    /**
     * @brief Stream insertion operator
     * @param os Output stream
     * @param cell Cons cell to print
     * @return Reference to stream
     */
    friend std::ostream& operator<<(std::ostream& os, const ConsCell* cell);

    /**
     * @brief Get type code
     * @return 0 (generic type code)
     */
    int get_type() const { return 0; }

    /**
     * @brief Test structural equality with another Object
     * @param val Object to compare with
     * @return true if both are cons cells with equal car and cdr
     * @note Recursively compares list structure
     */
    bool is_equal(Object* val);

    /**
     * @brief Trace car and cdr for garbage collection
     * @param tracer GC tracer
     * @note Visits both car and cdr if non-null
     */
    void trace_references(Tracer& tracer) override;

private:
    Object* m_car;  ///< First element (head of list)
    Object* m_cdr;  ///< Rest of list (tail) or second element of pair
};

// ============================================================================
// Global List Construction Functions
// ============================================================================

/**
 * @brief Construct a cons cell (R5RS cons)
 * @param e1 First element (car)
 * @param e2 Second element (cdr)
 * @return New cons cell containing (e1 . e2)
 *
 * Examples:
 * @code
 * cons(a, b)              // Creates pair (a . b)
 * cons(a, nil)            // Creates single-element list (a)
 * cons(a, cons(b, nil))   // Creates list (a b)
 * @endcode
 */
ConsCell* cons(const Object* e1, const Object* e2);

/**
 * @brief Append two lists (R5RS append)
 * @param e1 First list
 * @param e2 Second list or element
 * @return New list with e2 appended to e1
 * @note Creates new spine; does not modify originals
 * @note Time complexity: O(length(e1))
 *
 * Examples:
 * @code
 * append((1 2), (3 4))  // Returns (1 2 3 4)
 * append((1 2), nil)    // Returns (1 2)
 * append(nil, (3 4))    // Returns (3 4)
 * @endcode
 */
Object* append(const Object* e1, const Object* e2);

/**
 * @brief Create a two-element list (R5RS list)
 * @param a First element
 * @param b Second element
 * @return List (a b)
 * @note Convenience function: equivalent to cons(a, cons(b, nil))
 */
ConsCell* list(Object* a, Object* b);

} } // namespace zombie::runtime
