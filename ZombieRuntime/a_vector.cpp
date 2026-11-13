#include "pch.h"
#include "a_vector.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include "cons_cell.h"
#include "constants.h"

namespace zombie { namespace runtime {

a_vector::a_vector() : m_elements() {
}

a_vector::a_vector(int size) : m_elements(size, nil) {
}

a_vector::a_vector(int size, Object* fill) : m_elements(size, fill) {
}

a_vector::a_vector(const std::vector<Object*>& elements) : m_elements(elements) {
}

a_vector::~a_vector() {
}

Object* a_vector::clone() const {
    return gc_new<a_vector>(m_elements);
}

void a_vector::print(std::ostream& os) const {
    os << "#(";
    for (size_t i = 0; i < m_elements.size(); ++i) {
        if (i > 0) os << " ";
        if (m_elements[i]) {
            m_elements[i]->print(os);
        } else {
            os << "nil";
        }
    }
    os << ")";
}

Object* a_vector::get(int index) const {
    if (index < 0 || index >= static_cast<int>(m_elements.size())) {
        throw std::out_of_range("Vector index out of bounds");
    }
    return m_elements[index];
}

void a_vector::set(int index, Object* value) {
    if (index < 0 || index >= static_cast<int>(m_elements.size())) {
        throw std::out_of_range("Vector index out of bounds");
    }
    m_elements[index] = value;
}

void a_vector::trace_references(Tracer& tracer) {
    // Iterate by reference to allow in-place updates for moving GC
    for (Object*& elem : m_elements) {
        tracer.visit_field(elem);
    }
}

bool is_vector(const Object* obj) {
    return dynamic_cast<const a_vector*>(obj) != nullptr;
}

} } // namespace zombie::runtime
