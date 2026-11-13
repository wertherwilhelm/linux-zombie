#pragma once

#include "Base.h"
#include <string>
#include <vector>

namespace zombie { namespace runtime {

/**
 * TypeObject - A first-class representation of C++ types
 *
 * This class reifies the C++ type system, making it visible and manipulable
 * from Scheme code. It represents the boundary between Scheme and C++,
 * allowing introspection of the implementation substrate.
 *
 * The Meta-Object Protocol made manifest!
 */
class TypeObject : public Base {
public:
    TypeObject(const std::string& name, int type_id)
        : m_type_name(name), m_type_id(type_id) {}

    TypeObject(const TypeObject& other)
        : m_type_name(other.m_type_name), m_type_id(other.m_type_id),
          m_field_names(other.m_field_names), m_size_bytes(other.m_size_bytes) {}

    virtual ~TypeObject() {}

    // Object interface
    Object* clone() const override {
        return new TypeObject(*this);
    }

    void print(std::ostream& os) const override {
        os << "<type:" << m_type_name << ">";
    }

    int instance_of() override {
        return static_cast<int>(Base::atype::TYPE_OBJECT);  // New type ID
    }

    int get_type() const override { return static_cast<int>(Base::atype::TYPE_OBJECT); }
    int is_atom() const override { return 1; }
    int pair() const override { return 0; }

    // Type introspection interface
    std::string type_name() const { return m_type_name; }
    int type_id() const { return m_type_id; }

    void add_field(const std::string& field_name) {
        m_field_names.push_back(field_name);
    }

    const std::vector<std::string>& field_names() const {
        return m_field_names;
    }

    void set_size(size_t bytes) { m_size_bytes = bytes; }
    size_t size_bytes() const { return m_size_bytes; }

    void trace_references(Tracer& tracer) override {
        // TypeObject has no Scheme Object references
    }

private:
    std::string m_type_name;
    int m_type_id;
    std::vector<std::string> m_field_names;
    size_t m_size_bytes = 0;
};

// Helper functions for type introspection
TypeObject* make_type_descriptor(const std::string& name, int type_id);
TypeObject* get_type_of(Object* obj);

} } // namespace zombie::runtime
