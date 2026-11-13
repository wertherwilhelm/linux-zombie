#include "pch.h"
#include "cons_cell.h"
#include "constants.h"
#include "OutputPort.h"
#include "gc_manager.h"
#include "gc_ptr.h"

namespace zombie { namespace runtime {

ConsCell::ConsCell()
: m_car(nil)
, m_cdr(nil) {

}

ConsCell::ConsCell(const Object *e)
: m_car((Object*)e)
, m_cdr(nil) {
}

ConsCell::ConsCell(const Object *e1, const Object * e2)
: m_car((Object*)e1)
, m_cdr((Object*)e2) {
}

ConsCell::ConsCell(const ConsCell& p) {
    m_car = nil;
    m_cdr = nil;
    *this = p;
}

ConsCell::~ConsCell() {
    // GC will handle cleanup - don't manually delete car/cdr
    // Just null out the pointers for safety
    m_car = nullptr;
    m_cdr = nullptr;
}

ConsCell& ConsCell::operator=(const ConsCell& p) {
    if (this != &p) {
        // GC will handle cleanup - just reassign pointers (no cloning!)
        m_car = p.car();
        m_cdr = p.cdr();
    }

    return *this;
}

Object * ConsCell::car() const {
    return m_car;
}

Object * ConsCell::cdr() const {
    return m_cdr;
}

void ConsCell::car(const Object * e) {
    // Just store the pointer - no cloning! GC handles lifetime.
    m_car = (Object*)e;
}

void ConsCell::cdr(const Object * e) {
    // Just store the pointer - no cloning! GC handles lifetime.
    m_cdr = (Object*)e;
}

Object * ConsCell::clone() const {
    ConsCell * p = gc_new<ConsCell>(*this);
    return p;
}

void ConsCell::print(std::ostream &os) const {
    os << "(" << m_car << " . " << m_cdr << ")";
}

int ConsCell::is_atom() const {
    return 0;
}

int ConsCell::pair() const {
    return 1;
}

bool ConsCell::is_equal(Object* val) {
    if (!val->pair()) {
        return false;
    }
    // Structural equality: both car and cdr must be equal
    return equal(car(), ((ConsCell*)val)->car()) &&
           equal(cdr(), ((ConsCell*)val)->cdr());
}

void ConsCell::trace_references(Tracer& tracer) {
    // Mark car and cdr if they're not special constants (nil/t)
    // Note: nil and t are singleton constants that never move, so we skip them
    if (m_car && !m_car->is_nil() && !m_car->is_t()) {
        tracer.visit_field(m_car);
    }
    if (m_cdr && !m_cdr->is_nil() && !m_cdr->is_t()) {
        tracer.visit_field(m_cdr);
    }
}

/**
 * \brief Creates a fresh cons, the car of which is e1 and the cdr of which is e2.
 */
ConsCell* cons(const Object* e1, const Object* e2) {
    ConsCell* p = gc_new<ConsCell>();
    p->car(e1);
    p->cdr(e2);
    return p;
}

/**
 * \brief Append to the end of a List (R5RS compliant with structure sharing)
 *
 * \param e1 First list (will be copied)
 * \param e2 Second list (will be shared, not copied)
 *
 * Per R5RS Scheme specification:
 * - Returns a list consisting of the elements of e1 followed by elements of e2
 * - The resulting list is always newly allocated, EXCEPT it shares structure with e2
 * - This means e2 is NOT copied - it's directly shared
 *
 * Example: (append '(1 2) '(3 4)) => (1 2 3 4)
 * The (3 4) portion is the SAME Object as the original e2 (structure sharing)
 */
Object* append(const Object* e1, const Object* e2) {
    // Handle empty list case: (append '() x) => x (return e2 directly)
    if (e1 == nil || e1->is_nil()) {
        return (Object*)e2;
    }

    if (!e1->is_atom()) {
        // Build new list by copying elements from e1
        // Use gc_root to protect the list during construction
        gc_root<ConsCell> result;
        gc_root<ConsCell> tail;

        // Traverse e1 and copy each cons cell
        const ConsCell* current = (const ConsCell*)e1;
        while (current && !is_nil((Object*)current)) {
            // Create new cons cell with cloned car (protected by gc_root)
            gc_root<ConsCell> new_cell(gc_new<ConsCell>(current->car(), nil));

            if (!result) {
                // First cell
                result = new_cell;
                tail = new_cell;
            } else {
                // Link to previous cell (use set_cdr_noclone to avoid cloning)
                tail->set_cdr_noclone(new_cell.get());
                tail = new_cell;
            }

            // Move to next cell in e1
            Object* next = current->cdr();
            if (next->pair()) {
                current = (const ConsCell*)next;
            } else if (!is_nil(next)) {
                // R5RS: all but the last argument to append must be proper lists
                error("append: first argument must be a proper list (improper tail detected)");
                return nil;
            } else {
                // Proper termination with nil
                break;
            }
        }

        // Now attach e2 (SHARE if list, WRAP if atom) - R5RS semantics
        if (tail) {
            // Check if e2 is nil first - empty list should not be wrapped
            if (e2 == nil || e2->is_nil()) {
                // e2 is empty list - tail already ends with nil, nothing to do
            }
            // Check if e2 is an atom - if so, we need to wrap it in a cons cell
            // This handles cases like (append '(1 2) 3) where 3 is an atom
            // We want (1 2 3) not (1 2 . 3) which would be an improper list
            else if (e2->is_atom()) {
                // e2 is a non-nil atom, wrap it: (1 2) + 3 => (1 2 3)
                tail->set_cdr_noclone(cons(e2, nil));
            } else {
                // e2 is a list, share it directly: (1 2) + (3 4) => (1 2 3 4)
                tail->set_cdr_noclone((Object*)e2);
            }
        }

        return result.get() ? result.get() : (Object*)e2;
    }

    error("First arguement of append must be a List");
    return nil;
}

std::ostream& operator << (std::ostream& os, const ConsCell* p) {
    if (p != NULL) {
        p->print(os);
        return os;
    }
    os << "nil";
    return os;
}

} } // namespace zombie::runtime
