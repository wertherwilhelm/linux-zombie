#include "pch.h"
// Environment.cpp: implementation of the Environment class.
//
//////////////////////////////////////////////////////////////////////

#include "Environment.h"
#include "primitive_base.h"
#include "gc_manager.h"
#include "gc_ptr.h"

namespace zombie { namespace runtime {

//////////////////////////////////////////////////////////////////////
// Helper Functions
//////////////////////////////////////////////////////////////////////

// Extract name from either a_string or a_symbol
static std::string get_name(Object* obj) {
    if (!obj || obj == nil) {
        return "";
    }
    if (is_string(obj)) {
        return ((a_string*)obj)->value();
    } else if (is_symbol(obj)) {
        return ((a_symbol*)obj)->value();
    }
    return "";
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Environment::Environment()
: m_vars(NULL)
, m_vals(NULL)
, m_parent(NULL)
{
    m_vars = nil;
    m_vals = nil;

}

Environment::Environment(std::string name)
: m_vars(NULL)
, m_vals(NULL)
, m_parent(NULL)
{

}

Environment::Environment(std::string name, Object* vars, Object* vals, Environment* parent)
{
    m_vars = vars;
    m_vals = vals;
    m_parent = parent;
    if (!number_args_ok(vars, vals)) {
        warn("wrong number of arguments: expected ");
    }
}

Environment::~Environment()
{
    // GC will handle cleanup of m_vars, m_vals, and m_parent
    // Do NOT manually delete them - this caused recursive deletion chains
    // that hung with thousands of objects
    m_vars = nullptr;
    m_vals = nullptr;
    m_parent = nullptr;
}

Object* Environment::lookup(std::string symbol)
{
    return lookup(gc_new<a_string>(symbol));
}
    
Object* Environment::lookup(Object * symbol)
{
    Object *varList = m_vars;
    Object *valList = m_vals;

    // See if the symbol is bound locally
    std::string symbol_name = get_name(symbol);
    while (varList != nil)
    {
        // this has got to be cleaned up.  It should look like the following
        // if (first(varList) == symbol)
        if (get_name(first(varList)) == symbol_name) {
            return first(valList);
        } else if (get_name(varList) == symbol_name) {
            return valList;
        } else {
            varList = rest(varList);
            valList = rest(valList);
        }
    }
    // If not, try to look for the symbol in the parent
    if (m_parent != NULL) {
        return m_parent->lookup(symbol);
    } else {
        return error("Unbound variable: ");
    }
}

bool Environment::number_args_ok(Object *vars, Object *vals) 
{
    return ((vars == NULL && vals == NULL)
        || (!vars->is_atom() && !vals->is_atom()
            && number_args_ok(((ConsCell*)vars)->cdr(), ((ConsCell*)vals)->cdr())));
}

Object* Environment::define(Object *var, Object *val)
{
    // GC SAFETY: until the second cons completes, var and val are only
    // referenced from the C++ stack. The first cons allocates and can
    // trigger a threshold collection, which would reclaim val and bind a
    // dangling pointer (found via the GC obituary hook: a freshly defined
    // value's epitaph was spoken while its binding lived on).
    gc_root<Object> guard_var(var);
    gc_root<Object> guard_val(val);

    m_vars = cons(var, m_vars);
    m_vals = cons(val, m_vals);

    return var;
}

Object* Environment::define(std::string name, Object* val)
{
    return define(gc_new<a_string>(name), val);
}

Object* Environment::define(PrimitiveBase* val)
{
    return define(val->get_name(), val);
}

Object* Environment::set(Object *var, Object* val)
{
    if (!var->is_atom()) {
        return error("Attempt to set a non-symbol: ");
    }

    a_string *symbol = (a_string*) var;
    Object *varList = m_vars;
    Object *valList = m_vals;
    // See if the symbol is bound locally
    while (varList != NULL && varList != nil)
    {
        if (((a_string*)first(varList))->value() == ((a_string*)symbol)->value()) {
            return set_first(valList, val);
        }
        varList = rest(varList);
        valList = rest(valList);
    }
    
    // If not, try to look for the parent
    if (m_parent != NULL)
    {
        return m_parent->set(symbol, val);
    }
    else
    {
        return error("Unbound variable: ");
    }
}

Environment* Environment::extend(Object* params, Object* args)
{
    // Create a new environment with the current environment as its parent.
    Environment* extended = gc_new<Environment>();
    extended->set_parent(this);

    // Loop through the params and args and bind each param to its corresponding arg in the new environment.
    while (params != nullptr && !params->is_nil()) {
        // Check for dotted parameter list (rest args)
        // e.g., (lambda (x . rest) ...) or (lambda rest ...)
        if (params->is_atom()) {
            // params is an atom (not a cons cell) - this is a rest parameter
            // Bind all remaining args to this symbol
            if (!is_string(params) && !is_symbol(params)) {
                throw std::runtime_error("Rest parameter must be a symbol");
            }
            // Bind the rest parameter to the remaining args (or nil if no more args)
            extended->define(get_name(params), args ? args : nil);
            return extended;  // Done binding
        }

        if (args == nullptr || args->is_nil()) {
            throw std::runtime_error("Too few arguments to function");
        }

        Object* param = car(params);

        Object* arg = car(args);
        if (args->is_atom()) {
            arg = args;
        }

        if (!is_string(param) && !is_symbol(param)) {
            throw std::runtime_error("Parameter must be a symbol");
        }

        extended->define(get_name(param), arg);

        params = cdr(params);
        args = cdr(args);
    }

    // If there are more args than params, throw an error.
    if (args != nullptr && !args->is_nil()) {
        throw std::runtime_error("Too many arguments to function");
    }

    return extended;
}

void Environment::trace_references(Tracer& tracer)
{
    // Mark variables list, values list, and parent environment
    tracer.visit_field(m_vars);
    tracer.visit_field(m_vals);
    // Parent is Environment*, need to cast to Object* for tracer
    if (m_parent) {
        Object* parent_obj = (Object*)m_parent;
        tracer.visit_field(parent_obj);
        m_parent = (Environment*)parent_obj;  // Update m_parent after potential move
    }
}

} } // namespace zombie::runtime
