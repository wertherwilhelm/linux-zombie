/**
 * @file atom.h
 * 
 * @brief Implementation of Base Object for Lisp system.
 * 
 * @author Chris King
 */
#pragma once

#include "Object.h"
#include "gc_ptr.h"  // For gc_new


// should it have been atom->value type
// instead of value type->atom

// that way it can me atom->ValueType
//                        ->symbol
//                        ->string


// Undefine Windows SDK macros that conflict with our type names
#ifdef INT
#undef INT
#endif
#ifdef LONG
#undef LONG
#endif
#ifdef STRING
#undef STRING
#endif
#ifdef CHAR
#undef CHAR
#endif
#ifdef BOOL
#undef BOOL
#endif

namespace zombie { namespace runtime {

// Converted to enum class to avoid Windows type conflicts (MFC integration)
enum class Type
{
    NOTYPE = 0,
    CHAR,
    BOOL,
    INT,
    STRING,
    LONG,
    DOUBLE,
    SYMBOL,
    NUMBER,
    VECTOR,
    BYTEVECTOR,  // R7RS bytevector - array of bytes (0-255)
    RATIONAL,    // Exact rational number (numerator/denominator)
    COMPLEX,     // Complex number (real + imaginary parts)
    CPP_OBJECT,  // Wrapped C++ Object pointer (scene graph nodes, etc.)
    EXCEPTION,   // R7RS error-Object (exception with message and irritants)
};

std::ostream& operator<<(std::ostream& os, const Type& type);

// ============================================================================
// ValueType - Base class for all typed atomic values
// ============================================================================
//
// **Purpose:**
//   ValueType is the abstract Base class for all Scheme objects that are:
//   1. Atomic (not pairs/lists) - is_atom() returns 1
//   2. Typed (have a runtime type discriminant via get_type())
//
// **Conceptual Model:**
//   In Scheme, an "atom" is anything that's not a pair (cons cell).
//   ValueType represents the subset of atoms that have explicit types:
//   - Numbers: integers, doubles, rationals, complex
//   - Strings: atom<std::string>
//   - Symbols: a_symbol
//   - Vectors: a_vector
//   - Characters: atom<char>
//   - Booleans: atom<bool>
//
// **Contrast with Non-Atomic Objects:**
//   Objects that derive from Object directly (not ValueType):
//   - ConsCell: pairs and lists (pair() returns 1, is_atom() returns 0)
//   - Closure: lambda functions (callable, but not simple values)
//   - Primitive: built-in functions (callable, not atomic values)
//   - Continuation: call/cc continuations (control flow, not values)
//   - Macro: syntax transformers (not runtime values)
//
// **Type Hierarchy:**
//   Object (Base for all Scheme objects)
//     ├── ValueType (typed atomic values) ← THIS CLASS
//     │     ├── a_symbol (symbols)
//     │     ├── atom<T> (template for simple values: int, double, string, char, bool)
//     │     ├── a_rational (exact rationals: numerator/denominator)
//     │     ├── a_complex (complex numbers: real + imaginary)
//     │     └── a_vector (vectors: resizable arrays)
//     ├── ConsCell (pairs and lists)
//     ├── Closure (lambda functions)
//     ├── Primitive (built-in functions)
//     ├── Continuation (call/cc)
//     └── Macro (syntax transformers)
//
// **Contract:**
//   Derived classes MUST implement:
//   - get_type() → return a Type enum value (INT, DOUBLE, SYMBOL, VECTOR, etc.)
//
//   Derived classes typically implement:
//   - is_atom() → return 1 (inherited from Object, but enforced semantically)
//   - pair() → return 0 (not a pair)
//   - print() → display value
//   - clone() → deep copy
//   - trace_references() → GC tracing (if contains Object pointers)
//
// **Usage:**
//   To create a new typed atomic value:
//
//   class a_mytype : public ValueType {
//   public:
//       int get_type() const override { return static_cast<int>(Type::MYTYPE); }
//       int is_atom() const override { return 1; }
//       int pair() const override { return 0; }
//       // ... other Object methods
//   };
//
// **History:**
//   Originally, all atomic types derived directly from Object.
//   ValueType was introduced to provide a semantic middle layer for typed atoms,
//   making the type hierarchy clearer and enforcing the get_type() contract.
//
class ValueType : public Object {
public:
    /**
     * @brief Get the runtime type discriminant for this value
     * @return Type enum value (INT, DOUBLE, SYMBOL, VECTOR, RATIONAL, COMPLEX, etc.)
     *
     * This is the key method that distinguishes ValueType from raw objects.
     * Every typed atomic value must be able to report its type at runtime.
     */
    virtual int get_type() const = 0;
};

/**
 * @brief Symbol atom representing interned identifiers.
 */
class a_symbol : public ValueType {
public:
    /**
     * @brief Construct an empty symbol.
     */
    a_symbol() : m_name("") {}

    /**
     * @brief Construct a symbol from a string.
     * @param name Symbol name.
     */
    a_symbol(const std::string& name) : m_name(name) {}

    /**
     * @brief Copy constructor.
     */
    a_symbol(const a_symbol& other) : m_name(other.m_name) {}

    /**
     * @brief Get symbol name (legacy API).
     */
    std::string getName() const { return m_name; }

    /**
     * @brief Get symbol name.
     */
    const std::string& value() const { return m_name; }

    /**
     * @brief Return type code for symbols.
     */
    int get_type() const override { return static_cast<int>(Type::SYMBOL); }

    /**
     * @brief Symbols are atoms.
     */
    int is_atom() const override { return 1; }

    /**
     * @brief Symbols are not pairs.
     */
    int pair() const override { return 0; }

    /**
     * @brief Clone this symbol.
     */
    Object* clone() const override { return gc_new<a_symbol>(*this); }

    /**
     * @brief Print symbol to stream.
     */
    void print(std::ostream& os) const override { os << m_name; }

    /**
     * @brief Equality check for symbols (by name).
     */
    bool is_equal(Object* val) override {
        if (val && ((ValueType*)val)->get_type() == static_cast<int>(Type::SYMBOL)) {
            return m_name == ((a_symbol*)val)->m_name;
        }
        return false;
    }

    /**
     * @brief Symbols have no GC-traced references.
     */
    void trace_references(Tracer& tracer) override {}

private:
    std::string m_name;
};

template <class U>
struct atom_type {
    static constexpr Type Type_value = Type::NOTYPE;
};

template<>
struct atom_type<int> {
    static constexpr Type Type_value = Type::INT;
};

template<>
struct atom_type<char> {
    static constexpr Type Type_value = Type::CHAR;
};

// 64-bit exact integer storage. Note: this is `long long`, not `long`,
// because long is only 32 bits on Windows and would silently truncate.
template<>
struct atom_type<long long> {
    static constexpr Type Type_value = Type::LONG;
};

template<>
struct atom_type<double> {
    static constexpr Type Type_value = Type::DOUBLE;
};

template<>
struct atom_type<bool> {
    static constexpr Type Type_value = Type::BOOL;
};

template<>
struct atom_type<std::string> {
    static constexpr Type Type_value = Type::STRING;
};

/**
 * @rief This class implements an atom, i.e. not a pair.
 */
template <class T> 
class atom : public ValueType {
public:
    atom();
    atom(T);
    atom(const atom<T>&);
    virtual ~atom();

    const T& value() const;
    T& value();  // Non-const version for mutation

    void print(std::ostream& os) const;
    atom<T>& operator=(const atom<T>&);
    Object * clone() const;
    int is_atom() const;
    int pair() const;

    // NOTE: operator<< is defined as a free function template below (line ~345)
    // No friend declaration needed since print() is public

    virtual int get_type() const;// { return get_type<T>::type; }

    virtual bool is_equal(Object* val);

    void trace_references(Tracer& tracer) override;

protected:
    T m_value;
};

// Utility typedefs
using a_bool =  atom<bool>;
using a_int = atom<int>;
using a_long = atom<long long>;
using a_uint = atom<unsigned int>;
using a_short = atom<short>;
using a_float = atom<float>;
using a_double = atom<double>;
using a_char = atom<char>;
using a_string = atom<std::string>;

/**
 * \brief Default constructor
 */
template <class T>
atom<T>::atom() : m_value(T{}) {}

template <class T> 
atom<T>::atom(T t) :m_value(t) {}

template <class T>
atom<T>::atom(const atom<T>& t) {
    if (this != &t)
        m_value = t.value();
}

template <class T>
atom<T>::~atom(){}

template <class T>
const T& atom<T>::value() const {
    return m_value;
}

template <class T>
T& atom<T>::value() {
    return m_value;
}

template <class T>
int atom<T>::is_atom() const {
    return 1;
}

template <class T>
int atom<T>::pair() const {
    return 0; 
}

template <class T>
Object* atom<T>::clone() const {
    // gc_new (not raw new): the runtime never deletes clones — GC owns their
    // lifetime — so a raw new here leaks. Matches a_symbol::clone().
    atom<T> *t = gc_new<atom<T>>(*this);
    return (Object*)t;
}

template <class T>
void atom<T>::print(std::ostream& os) const {
    os << m_value;
}

template <class T>
std::ostream& operator<<(std::ostream& os, const atom<T>* t) {
    t->print(os);
    return os;
}

template <class T>
inline int atom<T>::get_type() const {
    return static_cast<int>(atom_type<T>::Type_value);
}

template <class T, class U>
atom<T>* cast_atom(Object* o) {
    return gc_new<atom<T>>(((atom<U>*)o)->value());
}

template <class T>
atom<T>* cast_atom(Object* o) {
    return gc_new<atom<T>>(((atom<T>*)o)->value());
}

template <class T>
T atom_value(Object* o) {
    return cast_atom<T>(o)->value();
}

template <class T>
bool atom<T>::is_equal(Object* val) {
    if (get_type() == ((ValueType*)val)->get_type()) {
        if (value() == cast_atom<T>(val)->value())
            return true;
    }
    return false;
}

template <class T>
void atom<T>::trace_references(Tracer& tracer) {
    // Atoms contain Primitive values (int, double, string, etc.)
    // They don't reference other objects, so nothing to trace
}

/**
  * The eql predicate is true if its arguments are eq, or if they are numbers of the same type with the same value, 
  * or if they are character objects that represent the same character.
  */
bool eql(const Object * o1, const Object * o2);

bool is_int(const Object* o);
bool is_double(const Object* o);
bool is_string(const Object* o);
bool is_char(const Object* o);
bool is_bool(const Object* o);
bool is_number(const Object* o);
bool is_symbol(const Object* o);

} } // namespace zombie::runtime
