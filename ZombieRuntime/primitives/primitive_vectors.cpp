#include "pch.h"
#include "../pch.h"
#include "primitive_vectors.h"
#include "../Base.h"
#include "../atom.h"
#include "../a_vector.h"
#include "../numeric_runtime.h"
#include <vector>

/**
 * @file primitive_vectors.cpp
 * @brief Implementation of vector primitives (R5RS Section 6.8)
 * @author Inheritance-based refactoring 2026-01
 * @date 2026-01-09
 */

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Convert list to vector
 * @param lst List to convert
 * @return Vector containing list elements
 */
static a_vector* list_to_vector(Object* lst) {
    if (lst == NULL || lst->is_nil()) {
        return gc_new<a_vector>();
    }

    std::vector<Object*> elements;
    Object* current = lst;

    while (current != NULL && !current->is_nil() && !current->is_atom()) {
        elements.push_back(car(current));
        current = cdr(current);
    }

    return gc_new<a_vector>(elements);
}

/**
 * @brief Convert vector to list
 * @param vec Vector to convert
 * @return List containing vector elements
 */
static Object* vector_to_list(Object* vec) {
    if (!is_vector(vec)) {
        return error("vector_to_list: argument must be a vector");
    }

    a_vector* v = (a_vector*)vec;
    Object* result = nil;

    // Build list in reverse order
    for (int i = v->length() - 1; i >= 0; i--) {
        result = cons(v->get(i), result);
    }

    return result;
}

// ============================================================================
// Constructor
// ============================================================================

PrimitiveVectors::PrimitiveVectors(ID id, int minArgs, int maxArgs)
    : PrimitiveBase(minArgs, maxArgs), m_id(id) {
    // Set Primitive name based on ID
    switch (id) {
        case VECTORQ:       m_name = "vector?"; break;
        case MAKEVECTOR:    m_name = "make-vector"; break;
        case VECTOR:        m_name = "vector"; break;
        case VECTORLENGTH:  m_name = "vector-length"; break;
        case VECTORREF:     m_name = "vector-ref"; break;
        case VECTORSET:     m_name = "vector-set!"; break;
        case VECTORFILL:    m_name = "vector-fill!"; break;
        case VECTORTOLIST:  m_name = "vector->list"; break;
        case LISTTOVECTOR:  m_name = "list->vector"; break;
        case VECTORCOPY:    m_name = "vector-copy"; break;
        case VECTORAPPEND:  m_name = "vector-append"; break;
        case VECTORMAP:     m_name = "vector-map"; break;
        case VECTORFOREACH: m_name = "vector-for-each"; break;
        case STRINGTOVECTOR: m_name = "string->vector"; break;
        case VECTORTOSTRING: m_name = "vector->string"; break;
    }
}

// ============================================================================
// Primitive Application
// ============================================================================

Object* PrimitiveVectors::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Arity checking
    if (!check_arity(args)) {
        return arity_error("");
    }

    // Extract arguments
    Object* x = first(args);
    Object* y = (rest(args) && !rest(args)->is_nil()) ? second(args) : nil;

    // Dispatch based on Primitive ID
    switch (m_id) {
        // ====================================================================
        // Predicate
        // ====================================================================

        case VECTORQ:
            return truth(is_vector(x));

        // ====================================================================
        // Construction
        // ====================================================================

        case MAKEVECTOR: {
            if (x == NULL || x->is_nil()) {
                return error("make-vector: requires size argument");
            }
            int size = (int)num(x);
            if (size < 0) {
                return error("make-vector: size must be non-negative");
            }
            Object* fill = (y != NULL && !y->is_nil()) ? y : nil;
            return gc_new<a_vector>(size, fill);
        }

        case VECTOR:
            // (vector elem1 elem2 ...) - construct vector from elements
            return list_to_vector(args);

        // ====================================================================
        // Access and Mutation
        // ====================================================================

        case VECTORLENGTH: {
            if (!is_vector(x)) {
                return error("vector-length: argument must be a vector");
            }
            return numeric::make_integer(((a_vector*)x)->length());
        }

        case VECTORREF: {
            if (!is_vector(x)) {
                return error("vector-ref: first argument must be a vector");
            }
            if (y == NULL || y->is_nil()) {
                return error("vector-ref: requires index argument");
            }
            int index = (int)num(y);
            a_vector* vec = (a_vector*)x;
            if (index < 0 || index >= vec->length()) {
                return error("vector-ref: index out of bounds");
            }
            return vec->get(index);
        }

        case VECTORSET: {
            if (!is_vector(x)) {
                return error("vector-set!: first argument must be a vector");
            }
            if (y == NULL || y->is_nil()) {
                return error("vector-set!: requires index argument");
            }
            int index = (int)num(y);
            a_vector* vec = (a_vector*)x;
            if (index < 0 || index >= vec->length()) {
                return error("vector-set!: index out of bounds");
            }
            Object* value = third(args);
            vec->set(index, value);
            return nil;
        }

        case VECTORFILL: {
            if (!is_vector(x)) {
                return error("vector-fill!: first argument must be a vector");
            }
            if (y == NULL || y->is_nil()) {
                return error("vector-fill!: requires fill value argument");
            }
            a_vector* vec = (a_vector*)x;
            // Fill all elements with the value
            for (int i = 0; i < vec->length(); i++) {
                vec->set(i, y);
            }
            return nil;
        }

        // ====================================================================
        // Conversions
        // ====================================================================

        case VECTORTOLIST:
            return vector_to_list(x);

        case LISTTOVECTOR:
            return list_to_vector(x);

        // ====================================================================
        // R7RS additions
        // ====================================================================

        case VECTORCOPY: {
            // (vector-copy vec [start [end]])
            // R7RS Section 6.8
            if (!is_vector(x)) {
                return error("vector-copy: argument must be a vector");
            }

            a_vector* vec = (a_vector*)x;
            int start = 0;
            int end = vec->length();

            // Optional start argument
            if (y && !y->is_nil()) {
                start = (int)num(y);
                if (start < 0 || start > vec->length()) {
                    return error("vector-copy: start index out of bounds");
                }

                // Optional end argument
                Object* z = (rest(rest(args)) && !rest(rest(args))->is_nil())
                    ? first(rest(rest(args)))
                    : nil;
                if (z && !z->is_nil()) {
                    end = (int)num(z);
                    if (end < start || end > vec->length()) {
                        return error("vector-copy: end index out of bounds");
                    }
                }
            }

            // Create new vector with copied elements
            std::vector<Object*> elements;
            for (int i = start; i < end; i++) {
                elements.push_back(vec->get(i));
            }
            return gc_new<a_vector>(elements);
        }

        case VECTORAPPEND: {
            // (vector-append vec ...)
            // R7RS Section 6.8
            // Concatenate all vectors into one
            std::vector<Object*> result_elements;

            Object* current_args = args;
            while (current_args && !current_args->is_nil()) {
                Object* vec_arg = first(current_args);
                if (!is_vector(vec_arg)) {
                    return error("vector-append: all arguments must be vectors");
                }

                a_vector* vec = (a_vector*)vec_arg;
                for (int i = 0; i < vec->length(); i++) {
                    result_elements.push_back(vec->get(i));
                }

                current_args = rest(current_args);
            }

            return gc_new<a_vector>(result_elements);
        }

        case VECTORMAP: {
            // (vector-map proc vec1 vec2 ...)
            // R7RS Section 6.8
            // Apply proc to corresponding elements of vectors
            if (!eval) {
                return error("vector-map: requires evaluator");
            }

            Object* proc = x;  // First argument is the Procedure
            Object* vec_args = rest(args);  // Remaining arguments are vectors

            if (!vec_args || vec_args->is_nil()) {
                return error("vector-map: requires at least one vector");
            }

            // Get first vector to determine length
            Object* first_vec_obj = first(vec_args);
            if (!is_vector(first_vec_obj)) {
                return error("vector-map: arguments must be vectors");
            }
            a_vector* first_vec = (a_vector*)first_vec_obj;
            int len = first_vec->length();

            // Verify all vectors have same length
            Object* check_args = rest(vec_args);
            while (check_args && !check_args->is_nil()) {
                Object* vec_obj = first(check_args);
                if (!is_vector(vec_obj)) {
                    return error("vector-map: arguments must be vectors");
                }
                a_vector* vec = (a_vector*)vec_obj;
                if (vec->length() != len) {
                    return error("vector-map: all vectors must have same length");
                }
                check_args = rest(check_args);
            }

            // Apply proc to each set of corresponding elements
            std::vector<Object*> result_elements;
            for (int i = 0; i < len; i++) {
                // Gather elements at index i from all vectors
                Object* proc_args = nil;
                Object* tail = nil;

                Object* vec_list = vec_args;
                while (vec_list && !vec_list->is_nil()) {
                    a_vector* vec = (a_vector*)first(vec_list);
                    Object* element = vec->get(i);
                    Object* new_cell = cons(element, nil);

                    if (proc_args == nil) {
                        proc_args = new_cell;
                        tail = new_cell;
                    } else {
                        set_rest(tail, new_cell);
                        tail = new_cell;
                    }

                    vec_list = rest(vec_list);
                }

                // Apply Procedure
                Object* result;
                if (proc->is_atom()) {
                    return error("vector-map: first argument must be a Procedure");
                }
                Procedure* p = dynamic_cast<Procedure*>(proc);
                if (!p) {
                    return error("vector-map: first argument must be a Procedure");
                }
                result = p->apply(eval, proc_args, env);

                result_elements.push_back(result);
            }

            return gc_new<a_vector>(result_elements);
        }

        case VECTORFOREACH: {
            // (vector-for-each proc vec1 vec2 ...)
            // R7RS Section 6.8
            // Apply proc to corresponding elements for side effects
            if (!eval) {
                return error("vector-for-each: requires evaluator");
            }

            Object* proc = x;  // First argument is the Procedure
            Object* vec_args = rest(args);  // Remaining arguments are vectors

            if (!vec_args || vec_args->is_nil()) {
                return error("vector-for-each: requires at least one vector");
            }

            // Get first vector to determine length
            Object* first_vec_obj = first(vec_args);
            if (!is_vector(first_vec_obj)) {
                return error("vector-for-each: arguments must be vectors");
            }
            a_vector* first_vec = (a_vector*)first_vec_obj;
            int len = first_vec->length();

            // Verify all vectors have same length
            Object* check_args = rest(vec_args);
            while (check_args && !check_args->is_nil()) {
                Object* vec_obj = first(check_args);
                if (!is_vector(vec_obj)) {
                    return error("vector-for-each: arguments must be vectors");
                }
                a_vector* vec = (a_vector*)vec_obj;
                if (vec->length() != len) {
                    return error("vector-for-each: all vectors must have same length");
                }
                check_args = rest(check_args);
            }

            // Apply proc to each set of corresponding elements
            for (int i = 0; i < len; i++) {
                // Gather elements at index i from all vectors
                Object* proc_args = nil;
                Object* tail = nil;

                Object* vec_list = vec_args;
                while (vec_list && !vec_list->is_nil()) {
                    a_vector* vec = (a_vector*)first(vec_list);
                    Object* element = vec->get(i);
                    Object* new_cell = cons(element, nil);

                    if (proc_args == nil) {
                        proc_args = new_cell;
                        tail = new_cell;
                    } else {
                        set_rest(tail, new_cell);
                        tail = new_cell;
                    }

                    vec_list = rest(vec_list);
                }

                // Apply Procedure (discard result)
                if (proc->is_atom()) {
                    return error("vector-for-each: first argument must be a Procedure");
                }
                Procedure* p = dynamic_cast<Procedure*>(proc);
                if (!p) {
                    return error("vector-for-each: first argument must be a Procedure");
                }
                p->apply(eval, proc_args, env);
            }

            return nil;  // R7RS: vector-for-each returns unspecified value
        }

        // ====================================================================
        // String/Vector Conversions (R7RS)
        // ====================================================================

        case STRINGTOVECTOR: {
            // (string->vector string [start [end]])
            // R7RS Section 6.8
            // Convert string to vector of characters
            if (!is_string(x)) {
                return error("string->vector: argument must be a string");
            }

            a_string* str = dynamic_cast<a_string*>(x);
            std::string s = str->value();
            int start = 0;
            int end = static_cast<int>(s.length());

            // Optional start argument
            if (y && !y->is_nil()) {
                start = (int)num(y);
                if (start < 0 || start > (int)s.length()) {
                    return error("string->vector: start index out of bounds");
                }

                // Optional end argument
                Object* z = (rest(rest(args)) && !rest(rest(args))->is_nil())
                    ? first(rest(rest(args)))
                    : nil;
                if (z && !z->is_nil()) {
                    end = (int)num(z);
                    if (end < start || end > (int)s.length()) {
                        return error("string->vector: end index out of bounds");
                    }
                }
            }

            // Create vector with characters from string
            std::vector<Object*> elements;
            for (int i = start; i < end; i++) {
                elements.push_back(gc_new<a_char>(s[i]));
            }
            return gc_new<a_vector>(elements);
        }

        case VECTORTOSTRING: {
            // (vector->string vector [start [end]])
            // R7RS Section 6.8
            // Convert vector of characters to string
            if (!is_vector(x)) {
                return error("vector->string: argument must be a vector");
            }

            a_vector* vec = (a_vector*)x;
            int start = 0;
            int end = vec->length();

            // Optional start argument
            if (y && !y->is_nil()) {
                start = (int)num(y);
                if (start < 0 || start > vec->length()) {
                    return error("vector->string: start index out of bounds");
                }

                // Optional end argument
                Object* z = (rest(rest(args)) && !rest(rest(args))->is_nil())
                    ? first(rest(rest(args)))
                    : nil;
                if (z && !z->is_nil()) {
                    end = (int)num(z);
                    if (end < start || end > vec->length()) {
                        return error("vector->string: end index out of bounds");
                    }
                }
            }

            // Build string from vector characters
            std::string result;
            for (int i = start; i < end; i++) {
                Object* elem = vec->get(i);
                if (!is_char(elem)) {
                    return error("vector->string: vector must contain only characters");
                }
                a_char* ch = dynamic_cast<a_char*>(elem);
                result += ch->value();
            }
            return gc_new<a_string>(result);
        }

        default:
            return error("Unknown vector Primitive");
    }
}

// ============================================================================
// Registration
// ============================================================================

void PrimitiveVectors::install(Environment* env) {
    const int n = 999999;

    // GC Safety: Use macro to protect name string before allocating primitive
    #define DEF(name, ...) do { \
        gc_root<a_string> _n(gc_new<a_string>(name)); \
        env->define(_n.get(), gc_new<PrimitiveVectors>(__VA_ARGS__)); \
    } while(0)

    // Predicate
    DEF("vector?", VECTORQ, 1, 1);

    // Construction
    DEF("make-vector", MAKEVECTOR, 1, 2);
    DEF("vector",      VECTOR, 0, n);

    // Access and mutation
    DEF("vector-length", VECTORLENGTH, 1, 1);
    DEF("vector-ref",    VECTORREF, 2, 2);
    DEF("vector-set!",   VECTORSET, 3, 3);
    DEF("vector-fill!",  VECTORFILL, 2, 2);

    // Conversions
    DEF("vector->list", VECTORTOLIST, 1, 1);
    DEF("list->vector", LISTTOVECTOR, 1, 1);

    // R7RS additions
    DEF("vector-copy",       VECTORCOPY, 1, 3);
    DEF("vector-append",     VECTORAPPEND, 0, n);
    DEF("vector-map",        VECTORMAP, 2, n);
    DEF("vector-for-each",   VECTORFOREACH, 2, n);
    DEF("string->vector",    STRINGTOVECTOR, 1, 3);
    DEF("vector->string",    VECTORTOSTRING, 1, 3);

    #undef DEF
}
