#include "pch.h"
#include "type_object.h"
#include "atom.h"
#include "a_vector.h"
#include "cons_cell.h"
#include "closure.h"
#include "primitive.h"

namespace zombie { namespace runtime {

/**
 * TypeObject implementation
 *
 * This provides the Meta-Object Protocol - reifying C++ types as Scheme objects
 */

// Helper: Create a type Object for a given C++ type
TypeObject* make_type_descriptor(const std::string& name, int type_id) {
    TypeObject* type_desc = new TypeObject(name, type_id);

    // Add field information based on type
    if (name == "ConsCell" || name == "pair") {
        type_desc->add_field("car");
        type_desc->add_field("cdr");
        type_desc->set_size(sizeof(ConsCell));
    }
    else if (name == "a_double" || name == "number") {
        type_desc->add_field("value");
        type_desc->set_size(sizeof(a_double));
    }
    else if (name == "a_int" || name == "integer") {
        type_desc->add_field("value");
        type_desc->set_size(sizeof(a_int));
    }
    else if (name == "a_string" || name == "string" || name == "symbol") {
        type_desc->add_field("value");
        type_desc->set_size(sizeof(a_string));
    }
    else if (name == "a_char" || name == "char") {
        type_desc->add_field("value");
        type_desc->set_size(sizeof(a_char));
    }
    else if (name == "a_bool" || name == "boolean") {
        type_desc->add_field("value");
        type_desc->set_size(sizeof(a_bool));
    }
    else if (name == "a_vector" || name == "vector") {
        type_desc->add_field("elements");
        type_desc->add_field("length");
        type_desc->set_size(sizeof(a_vector));
    }
    else if (name == "Closure") {
        type_desc->add_field("params");
        type_desc->add_field("body");
        type_desc->add_field("env");
        type_desc->set_size(sizeof(Closure));
    }
    else if (name == "Primitive") {
        type_desc->add_field("id");
        type_desc->add_field("name");
        type_desc->add_field("minArgs");
        type_desc->add_field("maxArgs");
        type_desc->set_size(sizeof(Primitive));
    }
    else if (name == "Environment") {
        type_desc->add_field("name");
        type_desc->add_field("vars");
        type_desc->add_field("vals");
        type_desc->add_field("parent");
        type_desc->set_size(0);  // Size not available without full definition
    }
    else if (name == "Continuation") {
        type_desc->add_field("saved_stack");
        type_desc->add_field("saved_env");
        type_desc->set_size(0);  // Size not available without full definition
    }

    return type_desc;
}

// Get type descriptor for any Object
TypeObject* get_type_of(Object* obj) {
    if (!obj || obj == nil) {
        return make_type_descriptor("nil", -1);
    }

    int type_id = obj->instance_of();

    // Map instance_of values to type names
    switch (type_id) {
        case static_cast<int>(Base::atype::CLOSURE):
            return make_type_descriptor("Closure", type_id);
        case static_cast<int>(Base::atype::MACRO):
            return make_type_descriptor("Macro", type_id);
        case static_cast<int>(Base::atype::PROCEDURE):
            return make_type_descriptor("Procedure", type_id);
        case static_cast<int>(Base::atype::BASE):
            return make_type_descriptor("base", type_id);
        case static_cast<int>(Base::atype::PRIMITIVE):
            return make_type_descriptor("Primitive", type_id);
        case static_cast<int>(Base::atype::LISP):
            return make_type_descriptor("Lisp", type_id);
        case static_cast<int>(Base::atype::INPUTPORT):
            return make_type_descriptor("input-port", type_id);
        case static_cast<int>(Base::atype::OUTPUTPORT):
            return make_type_descriptor("output-port", type_id);
        case static_cast<int>(Base::atype::TYPE_OBJECT):
            return make_type_descriptor("type", type_id);
        default:
            // Check for atom types
            if (obj->is_atom()) {
                // Try to determine specific atom type
                if (dynamic_cast<a_double*>(obj)) {
                    return make_type_descriptor("a_double", obj->get_type());
                }
                if (dynamic_cast<a_int*>(obj)) {
                    return make_type_descriptor("a_int", obj->get_type());
                }
                if (dynamic_cast<a_string*>(obj)) {
                    return make_type_descriptor("a_string", obj->get_type());
                }
                if (dynamic_cast<a_char*>(obj)) {
                    return make_type_descriptor("a_char", obj->get_type());
                }
                if (dynamic_cast<a_bool*>(obj)) {
                    return make_type_descriptor("a_bool", obj->get_type());
                }
                if (dynamic_cast<a_vector*>(obj)) {
                    return make_type_descriptor("a_vector", obj->get_type());
                }
                // Note: Environment and Continuation checks would require full type definition
                // They are handled by the instance_of() switch cases above
                return make_type_descriptor("atom", obj->get_type());
            }
            // Must be a ConsCell
            return make_type_descriptor("ConsCell", obj->get_type());
    }
}

} } // namespace zombie::runtime
