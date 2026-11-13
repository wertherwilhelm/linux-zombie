#include "pch.h"
/**
 * @file primitives_scenegraph.cpp
 * @brief Scene graph primitives for Zombie Scheme
 *
 * Provides Scheme bindings for the scenegraph library, allowing interactive
 * manipulation of the scene graph from the REPL.
 *
 * NOTE: This file requires the SceneGraph library. To enable, define
 * ENABLE_SCENEGRAPH in your project preprocessor definitions and ensure
 * the scenegraph include path is available.
 */

#include "../pch.h"
#include "primitives_scenegraph.h"

#ifdef ENABLE_SCENEGRAPH
#include "../Environment.h"
#include "../primitive.h"
#include "../procedure.h"
#include "../i_evaluator.h"
#include "../cons_cell.h"
#include "../Base.h"
#include "../atom.h"
#include "../constants.h"
#include "../numeric_runtime.h"

// Scene graph includes (using SharedPtrPolicy as used in GraphicsTestApp)
#include "scenegraph/core/node.h"
#include "scenegraph/core/group.h"
#include "scenegraph/policy/memory_policy.h"

namespace primitives {

// ============================================================================
// Function Pointer Procedure Wrapper
// ============================================================================

// Simple Procedure wrapper for C++ function pointers
class func_procedure : public Procedure {
public:
    using FuncPtr = Object* (*)(Object*, Environment*);

    func_procedure(FuncPtr func, int min_args, int max_args = -1)
        : func_(func), min_args_(min_args), max_args_(max_args) {}

    Object* clone() const override {
        return new func_procedure(func_, min_args_, max_args_);
    }

    void print(std::ostream& os) const override {
        os << "<Primitive-Procedure>";
    }

    Object* apply(IEvaluator* evaluator, Object* args, Environment* env) override {
        (void)evaluator; // Unused

        // Check arity
        int arg_count = 0;
        for (Object* p = args; p && p != nil; p = cdr(p)) {
            arg_count++;
        }

        if (arg_count < min_args_) {
            throw std::runtime_error("Too few arguments");
        }
        if (max_args_ >= 0 && arg_count > max_args_) {
            throw std::runtime_error("Too many arguments");
        }

        // Call function
        return func_(args, env);
    }

    void trace_references(Tracer& tracer) override {
        // No Scheme references
    }

private:
    FuncPtr func_;
    int min_args_;
    int max_args_;
};

// ============================================================================
// Type Definitions
// ============================================================================

using SGPolicy = scenegraph::SharedPtrPolicy;
using SGNode = scenegraph::Node<SGPolicy>;
using SGGroup = scenegraph::Group<SGPolicy>;

// Custom atom type to wrap scene graph node pointers
class sg_node_wrapper : public atom<SGNode*> {
public:
    explicit sg_node_wrapper(SGNode* node) : atom<SGNode*>(node) {
        // Keep a shared_ptr to ensure the node stays alive
        if (node) {
            m_shared = node->shared_from_this();
            m_shared_node = std::static_pointer_cast<SGNode>(m_shared);
        }
    }

    ~sg_node_wrapper() override = default;

    Object* clone() const override {
        return new sg_node_wrapper(this->value());
    }

    void print(std::ostream& os) const override {
        SGNode* node = this->value();
        if (!node) {
            os << "<sg-node:null>";
        } else if (dynamic_cast<SGGroup*>(node)) {
            SGGroup* group = static_cast<SGGroup*>(node);
            os << "<sg-group:\"" << node->name() << "\" children=" << group->num_children() << ">";
        } else {
            os << "<sg-node:\"" << node->name() << "\">";
        }
    }

    int get_type() const override {
        return static_cast<int>(Type::CPP_OBJECT);
    }

    void trace_references(Tracer& tracer) override {
        // No Scheme Object references
    }

private:
    std::shared_ptr<scenegraph::Object<SGPolicy>> m_shared;  // Keep node alive
    std::shared_ptr<SGNode> m_shared_node;
};

// ============================================================================
// Helper Functions
// ============================================================================

static SGNode* extract_node(Object* obj, const char* func_name) {
    if (!obj) {
        throw std::runtime_error(std::string(func_name) + ": null argument");
    }

    if (obj->get_type() != static_cast<int>(Type::CPP_OBJECT)) {
        throw std::runtime_error(std::string(func_name) + ": argument is not a scene graph node");
    }

    sg_node_wrapper* wrapper = dynamic_cast<sg_node_wrapper*>(obj);
    if (!wrapper) {
        throw std::runtime_error(std::string(func_name) + ": invalid scene graph node wrapper");
    }

    SGNode* node = wrapper->value();
    if (!node) {
        throw std::runtime_error(std::string(func_name) + ": scene graph node is null");
    }

    return node;
}

static Object* wrap_node(SGNode* node) {
    if (!node) {
        return nil;
    }
    return new sg_node_wrapper(node);
}

// ============================================================================
// Primitive Implementations
// ============================================================================

/**
 * (sg-make-group) => node
 * (sg-make-group name) => node
 *
 * Create a new scene graph group node.
 */
Object* prim_sg_make_group(Object* args, Environment* env) {
    std::string name = "";

    // Optional name argument
    if (args && !is_nil(args)) {
        Object* name_obj = car(args);
        if (name_obj->get_type() == static_cast<int>(Type::STRING)) {
            name = static_cast<a_string*>(name_obj)->value();
        } else if (name_obj->get_type() == static_cast<int>(Type::SYMBOL)) {
            name = static_cast<a_symbol*>(name_obj)->getName();
        }
    }

    // Create group using shared_ptr
    auto group = std::make_shared<SGGroup>();
    if (!name.empty()) {
        group->set_name(name);
    }

    return wrap_node(group.get());
}

/**
 * (sg-add-child! parent child) => #t or #f
 *
 * Add child node to parent group.
 */
Object* prim_sg_add_child(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("sg-add-child!: requires 2 arguments");
    }

    SGNode* parent_node = extract_node(car(args), "sg-add-child!");
    SGGroup* parent = dynamic_cast<SGGroup*>(parent_node);
    if (!parent) {
        throw std::runtime_error("sg-add-child!: parent must be a group node");
    }

    Object* rest = cdr(args);
    if (!rest || is_nil(rest)) {
        throw std::runtime_error("sg-add-child!: requires child argument");
    }

    SGNode* child = extract_node(car(rest), "sg-add-child!");

    bool success = parent->add_child(child);
    return success ? t : static_cast<Object*>(false_obj);
}

/**
 * (sg-remove-child! parent child) => #t or #f
 *
 * Remove child node from parent group.
 */
Object* prim_sg_remove_child(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("sg-remove-child!: requires 2 arguments");
    }

    SGNode* parent_node = extract_node(car(args), "sg-remove-child!");
    SGGroup* parent = dynamic_cast<SGGroup*>(parent_node);
    if (!parent) {
        throw std::runtime_error("sg-remove-child!: parent must be a group node");
    }

    Object* rest = cdr(args);
    if (!rest || is_nil(rest)) {
        throw std::runtime_error("sg-remove-child!: requires child argument");
    }

    SGNode* child = extract_node(car(rest), "sg-remove-child!");

    bool success = parent->remove_child(child);
    return success ? t : static_cast<Object*>(false_obj);
}

/**
 * (sg-get-child parent index) => node or #f
 *
 * Get child at index from parent group.
 */
Object* prim_sg_get_child(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("sg-get-child: requires 2 arguments");
    }

    SGNode* parent_node = extract_node(car(args), "sg-get-child");
    SGGroup* parent = dynamic_cast<SGGroup*>(parent_node);
    if (!parent) {
        throw std::runtime_error("sg-get-child: parent must be a group node");
    }

    Object* rest = cdr(args);
    if (!rest || is_nil(rest)) {
        throw std::runtime_error("sg-get-child: requires index argument");
    }

    Object* index_obj = car(rest);
    long index = 0;

    if (index_obj->get_type() == static_cast<int>(Type::INT)) {
        index = static_cast<atom<int>*>(index_obj)->value();
    } else if (index_obj->get_type() == static_cast<int>(Type::LONG)) {
        index = static_cast<atom<long long>*>(index_obj)->value();
    } else {
        throw std::runtime_error("sg-get-child: index must be an integer");
    }
    if (index < 0) {
        throw std::runtime_error("sg-get-child: index must be non-negative");
    }

    SGNode* child = parent->get_child(static_cast<size_t>(index));
    return wrap_node(child);
}

/**
 * (sg-num-children group) => integer
 *
 * Get number of children in group.
 */
Object* prim_sg_num_children(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("sg-num-children: requires 1 argument");
    }

    SGNode* node = extract_node(car(args), "sg-num-children");
    SGGroup* group = dynamic_cast<SGGroup*>(node);
    if (!group) {
        throw std::runtime_error("sg-num-children: argument must be a group node");
    }

    return numeric::make_integer(static_cast<long>(group->num_children()));
}

/**
 * (sg-set-name! node name) => #t
 *
 * Set the name of a scene graph node.
 */
Object* prim_sg_set_name(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("sg-set-name!: requires 2 arguments");
    }

    SGNode* node = extract_node(car(args), "sg-set-name!");

    Object* rest = cdr(args);
    if (!rest || is_nil(rest)) {
        throw std::runtime_error("sg-set-name!: requires name argument");
    }

    Object* name_obj = car(rest);
    std::string name;
    if (name_obj->get_type() == static_cast<int>(Type::STRING)) {
        name = static_cast<a_string*>(name_obj)->value();
    } else if (name_obj->get_type() == static_cast<int>(Type::SYMBOL)) {
        name = static_cast<a_symbol*>(name_obj)->getName();
    } else {
        throw std::runtime_error("sg-set-name!: name must be a string or symbol");
    }

    node->set_name(name);
    return t;
}

/**
 * (sg-get-name node) => string
 *
 * Get the name of a scene graph node.
 */
Object* prim_sg_get_name(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        throw std::runtime_error("sg-get-name: requires 1 argument");
    }

    SGNode* node = extract_node(car(args), "sg-get-name");
    return gc_new<a_string>(node->name());
}

/**
 * (sg-node? obj) => #t or #f
 *
 * Check if Object is a scene graph node.
 */
Object* prim_sg_node_p(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        return static_cast<Object*>(false_obj);
    }

    Object* obj = car(args);
    if (obj->get_type() != static_cast<int>(Type::CPP_OBJECT)) {
        return static_cast<Object*>(false_obj);
    }

    sg_node_wrapper* wrapper = dynamic_cast<sg_node_wrapper*>(obj);
    return wrapper ? t : static_cast<Object*>(false_obj);
}

/**
 * (sg-group? obj) => #t or #f
 *
 * Check if Object is a scene graph group node.
 */
Object* prim_sg_group_p(Object* args, Environment* env) {
    if (!args || is_nil(args)) {
        return static_cast<Object*>(false_obj);
    }

    Object* obj = car(args);
    if (obj->get_type() != static_cast<int>(Type::CPP_OBJECT)) {
        return static_cast<Object*>(false_obj);
    }

    sg_node_wrapper* wrapper = dynamic_cast<sg_node_wrapper*>(obj);
    if (!wrapper) {
        return static_cast<Object*>(false_obj);
    }

    SGNode* node = wrapper->value();
    return dynamic_cast<SGGroup*>(node) ? t : static_cast<Object*>(false_obj);
}

// ============================================================================
// Installation
// ============================================================================

void install_scenegraph_group(Environment* env) {
    // Node creation
    env->define("sg-make-group", new func_procedure(prim_sg_make_group, 0, 1));

    // Hierarchy management
    env->define("sg-add-child!", new func_procedure(prim_sg_add_child, 2));
    env->define("sg-remove-child!", new func_procedure(prim_sg_remove_child, 2));
    env->define("sg-get-child", new func_procedure(prim_sg_get_child, 2));
    env->define("sg-num-children", new func_procedure(prim_sg_num_children, 1));

    // Properties
    env->define("sg-set-name!", new func_procedure(prim_sg_set_name, 2));
    env->define("sg-get-name", new func_procedure(prim_sg_get_name, 1));

    // Predicates
    env->define("sg-node?", new func_procedure(prim_sg_node_p, 1));
    env->define("sg-group?", new func_procedure(prim_sg_group_p, 1));
}

} // namespace primitives

#else // !ENABLE_SCENEGRAPH

// Stub when SceneGraph is not available
class Environment;
namespace primitives {
void install_scenegraph_group(Environment* /*env*/) {
    // SceneGraph not enabled - no primitives installed
}
} // namespace primitives

#endif // ENABLE_SCENEGRAPH
