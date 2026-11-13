#include "pch.h"
#include "Object.h"
#include "atom.h"

namespace zombie { namespace runtime {

Object::Object()
    : gc_object()  // Base class handles m_marked initialization
    , m_size(0)
    , m_value_id(0)  // Not assigned to ledger yet
{

}

Object::~Object() {

}

bool Object::is_equal(Object* val) {
    return this == val;
}

std::ostream& operator <<(std::ostream& os, const Object* e) {
    if (e != NULL) {
        e->print(os);
        return os;
    }
    os << "nil";
    return os;
}

bool eq(const Object* o1, const Object* o2) {
    // Pointer equality is the primary check
    if (o1 == o2) return true;

    // For symbols, compare by name (workaround for lack of symbol interning)
    // This makes memq and assq work correctly with symbols
    if (is_symbol(o1) && is_symbol(o2)) {
        return ((a_symbol*)o1)->value() == ((a_symbol*)o2)->value();
    }

    // Everything else is strict pointer equality
    return false;
}

} } // namespace zombie::runtime
