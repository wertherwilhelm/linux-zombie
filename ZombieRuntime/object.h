/**
 * @file Object.h
 * @brief Base class for all Scheme objects in the Lisp system
 * @author chrislk@gmail.com
 * @date 2024-06
 */
#pragma once

#include <iostream>

#include "number.h"
#include "gc_object.h"  // GC Base class (from GarbageCollector library)

namespace zombie { namespace runtime {

/**
 * @brief Base class for all Scheme objects in the interpreter
 *
 * This is the root of the Object hierarchy in Zombie Scheme. All Scheme values
 * (numbers, strings, lists, closures, etc.) inherit from this class.
 *
 * **Design:**
 * - Inherits from gc_object (GarbageCollector library) for automatic memory management
 * - Provides type system (get_type(), is_atom(), pair())
 * - Supports equality testing (is_equal(), operator==)
 * - Enables printing (print(), operator<<)
 * - Requires cloning capability (clone())
 *
 * **Garbage Collection:**
 * All objects participate in mark-and-sweep GC. Derived classes must implement
 * trace_references() to enumerate their child objects.
 *
 * **Type Hierarchy:**
 * - atom<T> - Atomic values (numbers, strings, symbols, characters)
 * - ConsCell - Pairs and lists
 * - Procedure - Callables (Primitive, Closure, Macro, Continuation)
 * - Environment - Variable bindings
 * - a_vector - Vectors
 *
 * @see gc_object
 * @see atom
 * @see ConsCell
 * @see Procedure
 */
class Object : public gc_object {
public:
    /**
     * @brief Construct a new Object
     */
    Object();

    /**
     * @brief Destroy the Object
     */
    virtual ~Object();

    /**
     * @brief Create a deep copy of this Object
     * @return Pointer to newly allocated clone
     * @note Caller is responsible for GC registration of returned Object
     */
    virtual Object* clone() const = 0;

    /**
     * @brief Print Object to output stream
     * @param os Output stream to write to
     * @note Used by operator<< for display and debugging
     */
    virtual void print(std::ostream& os) const = 0;

    /**
     * @brief Stream insertion operator for printing
     * @param os Output stream
     * @param obj Object to print
     * @return Reference to output stream (for chaining)
     */
    friend std::ostream& operator << (std::ostream& os, const Object* obj);

    /**
     * @brief Get the type identifier of this Object
     * @return Integer type code (see Type enum in subclasses)
     * @note Each subclass returns a unique type code
     */
    virtual int get_type() const = 0;

    /**
     * @brief Check if this Object is an atom (not a pair)
     * @return 1 if atom, 0 if pair
     * @note Atoms include numbers, strings, symbols, booleans, nil, etc.
     */
    virtual int is_atom() const = 0;

    /**
     * @brief Check if this Object is a pair (cons cell)
     * @return 1 if pair, 0 if atom
     * @note Only ConsCell returns 1
     */
    virtual int pair() const = 0;

    /**
     * @brief Get runtime type instance identifier
     * @return Integer identifying the specific subclass type
     * @note Used for dynamic type checking before casting
     */
    virtual int instance_of() { return -1; }

    /**
     * @brief Check if this is the nil singleton
     * @return 1 if this is nil, 0 otherwise
     * @note Only the nil singleton returns 1
     */
    virtual int is_nil() const { return 0; }

    /**
     * @brief Check if this is the #t (true) singleton
     * @return 1 if this is #t, 0 otherwise
     * @note Only the #t singleton returns 1
     */
    virtual int is_t() const { return 0; }

    /**
     * @brief Test semantic equality with another Object
     * @param val Object to compare with
     * @return true if objects are semantically equal
     * @note Default implementation compares memory addresses (identity)
     * @note Subclasses override for value-based comparison
     */
    virtual bool is_equal(Object* val);

    /**
     * @brief Equality operator
     * @param rhs Right-hand side Object
     * @return true if objects are identical or equal
     * @note Checks identity first (same address), then calls is_equal()
     */
    virtual bool operator==(Object& rhs) {
        return this == &rhs || this->is_equal(&rhs);
    }

    /**
     * @brief Trace all objects referenced by this Object for GC marking
     *
     * Each derived class must implement this to call tracer.visit_field() on
     * any Object* fields it contains. The tracer handles the actual marking
     * strategy, completely decoupling Object enumeration from GC implementation.
     *
     * IMPORTANT: Use visit_field() instead of visit() for moving GC support.
     * visit_field() receives the field LOCATION (via reference) and can update
     * it after copying objects. visit() only receives the VALUE and cannot update.
     *
     * @param tracer The GC tracer that receives child references
     *
     * Example implementation:
     * @code
     * void ConsCell::trace_references(Tracer& tracer) override {
     *     tracer.visit_field(car_);  // Field location passed, can be updated by moving GC
     *     tracer.visit_field(cdr_);
     * }
     * @endcode
     */
    virtual void trace_references(Tracer& tracer) override = 0;

    /**
     * @brief Increment reference count (hybrid GC support)
     * @note Currently disabled - pure mark-and-sweep GC in use
     */
    void add_ref() { /* ++_ref_count; */ }

    /**
     * @brief Decrement reference count (hybrid GC support)
     * @note Currently disabled - pure mark-and-sweep GC in use
     */
    void release() { /* ... */ }

    /**
     * @brief Get current reference count
     * @return Reference count (always 0 when disabled)
     * @note Currently disabled - pure mark-and-sweep GC in use
     */
    int ref_count() const { return 0; }

    /**
     * @brief Check if Object has multiple references
     * @return true if shared (always false when disabled)
     * @note Currently disabled - pure mark-and-sweep GC in use
     */
    bool is_shared() const { return false; }

private:
    size_t m_size;      ///< Reserved for future use (Object size tracking)
    // int _ref_count;  ///< TEMPORARILY DISABLED - uncomment when ready for hybrid ref counting
};

// ============================================================================
// Global Object Type Testing Functions
// ============================================================================

/**
 * @brief Test Object identity (pointer equality)
 *
 * Implements Scheme's eq? predicate. Returns true if and only if o1 and o2
 * are the exact same Object in memory (same address).
 *
 * @param o1 First Object
 * @param o2 Second Object
 * @return true if objects are identical (same memory address)
 *
 * @note This is identity testing, not equality. Use equal() for value comparison.
 * @note Scheme semantics: (eq? x y) => same Object
 *
 * @see is_equal() for semantic equality
 */
bool eq(const Object* o1, const Object* o2);

// Future R5RS equality predicates (not yet implemented):
// bool eql(const Object* o1, const Object* o2);     // eql? - same value or same Object
// bool equal(const Object* o1, const Object* o2);   // equal? - structural equality
// bool equalp(const Object* o1, const Object* o2);  // case-insensitive equal?

/**
 * @brief Check if Object is the nil singleton
 * @param o Object to test
 * @return true if Object is nil
 * @note nil represents the empty list '()
 */
inline bool is_nil(const Object* o) {
    return o->is_nil() == 1;
}

/**
 * @brief Check if Object is the #t (true) singleton
 * @param o Object to test
 * @return true if Object is #t
 * @note In Scheme, only #f is false; everything else (including nil) is true
 */
inline bool is_t(const Object* o) {
    return o->is_t() == 1;
}

/**
 * @brief Check if Object is an atom (not a pair)
 * @param o Object to test
 * @return true if atom (number, string, symbol, etc.)
 * @note Atoms are self-evaluating values or symbols
 */
inline bool is_atom(const Object* o) {
    return o->is_atom() == 1;
}

/**
 * @brief Check if Object is a pair (cons cell)
 * @param o Object to test
 * @return true if Object is a pair
 * @note Pairs are the building blocks of lists in Scheme
 */
inline bool is_pair(const Object* o) {
    return o->pair();
}

} } // namespace zombie::runtime
