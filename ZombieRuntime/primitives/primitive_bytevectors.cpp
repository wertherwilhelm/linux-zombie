/**
 * @file primitive_bytevectors.cpp
 * @brief Implementation of R7RS bytevector primitives
 */

#include "pch.h"
#include "../pch.h"
#include "primitive_bytevectors.h"
#include "../a_bytevector.h"
#include "../atom.h"
#include "../Base.h"
#include "../cons_cell.h"
#include "../constants.h"
#include "../numeric_runtime.h"
#include <stdexcept>
#include <algorithm>

// ============================================================================
// Constructor
// ============================================================================

PrimitiveBytevectors::PrimitiveBytevectors(ID id, int minArgs, int maxArgs)
    : PrimitiveBase(minArgs, maxArgs), m_id(id) {
    // Set Primitive name based on ID
    switch (id) {
        case BYTEVECTORQ:       m_name = "bytevector?"; break;
        case MAKEBYTEVECTOR:    m_name = "make-bytevector"; break;
        case BYTEVECTOR:        m_name = "bytevector"; break;
        case BYTEVECTORLENGTH:  m_name = "bytevector-length"; break;
        case BYTEVECTORU8REF:   m_name = "bytevector-u8-ref"; break;
        case BYTEVECTORU8SET:   m_name = "bytevector-u8-set!"; break;
        case BYTEVECTORCOPY:    m_name = "bytevector-copy"; break;
        case BYTEVECTORCOPYMUT: m_name = "bytevector-copy!"; break;
        case BYTEVECTORAPPEND:  m_name = "bytevector-append"; break;
        case UTF8TOSTRING:      m_name = "utf8->string"; break;
        case STRINGTOUTF8:      m_name = "string->utf8"; break;
    }
}

// ============================================================================
// Primitive Application
// ============================================================================

Object* PrimitiveBytevectors::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Arity checking
    if (!check_arity(args)) {
        return arity_error("");
    }

    // Extract common arguments
    Object* x = first(args);
    Object* y = (rest(args) && !rest(args)->is_nil()) ? second(args) : nil;
    Object* z = (rest(args) && rest(rest(args)) && !rest(rest(args))->is_nil()) ? third(args) : nil;

    // Dispatch based on Primitive ID
    switch (m_id) {
        // ====================================================================
        // Type Checking
        // ====================================================================
        case BYTEVECTORQ:
            return is_bytevector(x) ? t : static_cast<Object*>(false_obj);

        // ====================================================================
        // Construction
        // ====================================================================
        case MAKEBYTEVECTOR: {
            if (!numeric::is_number(x)) {
                return error("make-bytevector: k must be a number");
            }
            int64_t k = numeric::to_int64(x);
            if (k < 0) {
                return error("make-bytevector: k must be non-negative");
            }

            uint8_t fill = 0;
            if (y && !y->is_nil()) {
                if (!numeric::is_number(y)) {
                    return error("make-bytevector: fill must be a number");
                }
                int64_t byte_val = numeric::to_int64(y);
                if (byte_val < 0 || byte_val > 255) {
                    return error("make-bytevector: fill must be in range [0, 255]");
                }
                fill = static_cast<uint8_t>(byte_val);
            }

            return gc_new<a_bytevector>(static_cast<int>(k), fill);
        }

        case BYTEVECTOR: {
            std::vector<uint8_t> bytes;
            Object* current = args;
            while (current && !current->is_nil()) {
                Object* byte_obj = car(current);
                if (!numeric::is_number(byte_obj)) {
                    return error("bytevector: all arguments must be numbers");
                }
                int64_t byte_val = numeric::to_int64(byte_obj);
                if (byte_val < 0 || byte_val > 255) {
                    return error("bytevector: bytes must be in range [0, 255]");
                }
                bytes.push_back(static_cast<uint8_t>(byte_val));
                current = cdr(current);
            }
            return gc_new<a_bytevector>(bytes);
        }

        // ====================================================================
        // Access and Mutation
        // ====================================================================
        case BYTEVECTORLENGTH: {
            if (!is_bytevector(x)) {
                return error("bytevector-length: argument must be a bytevector");
            }
            a_bytevector* bv = static_cast<a_bytevector*>(x);
            return numeric::make_integer(bv->length());
        }

        case BYTEVECTORU8REF: {
            if (!is_bytevector(x)) {
                return error("bytevector-u8-ref: first argument must be a bytevector");
            }
            if (!numeric::is_number(y)) {
                return error("bytevector-u8-ref: index must be a number");
            }

            a_bytevector* bv = static_cast<a_bytevector*>(x);
            int64_t k = numeric::to_int64(y);

            if (k < 0 || k >= bv->length()) {
                return error("bytevector-u8-ref: index out of range");
            }

            return numeric::make_integer(bv->get(static_cast<int>(k)));
        }

        case BYTEVECTORU8SET: {
            if (!is_bytevector(x)) {
                return error("bytevector-u8-set!: first argument must be a bytevector");
            }
            if (!numeric::is_number(y)) {
                return error("bytevector-u8-set!: index must be a number");
            }
            if (!numeric::is_number(z)) {
                return error("bytevector-u8-set!: byte must be a number");
            }

            a_bytevector* bv = static_cast<a_bytevector*>(x);
            int64_t k = numeric::to_int64(y);
            int64_t byte_val = numeric::to_int64(z);

            if (k < 0 || k >= bv->length()) {
                return error("bytevector-u8-set!: index out of range");
            }

            if (byte_val < 0 || byte_val > 255) {
                return error("bytevector-u8-set!: byte must be in range [0, 255]");
            }

            bv->set(static_cast<int>(k), static_cast<uint8_t>(byte_val));
            return bv;
        }

        // ====================================================================
        // Copying
        // ====================================================================
        case BYTEVECTORCOPY: {
            if (!is_bytevector(x)) {
                return error("bytevector-copy: argument must be a bytevector");
            }

            a_bytevector* bv = static_cast<a_bytevector*>(x);
            int start = 0;
            int end = bv->length();

            if (y && !y->is_nil()) {
                if (!numeric::is_number(y)) {
                    return error("bytevector-copy: start must be a number");
                }
                start = static_cast<int>(numeric::to_int64(y));

                if (z && !z->is_nil()) {
                    if (!numeric::is_number(z)) {
                        return error("bytevector-copy: end must be a number");
                    }
                    end = static_cast<int>(numeric::to_int64(z));
                }
            }

            if (start < 0 || start > bv->length() || end < start || end > bv->length()) {
                return error("bytevector-copy: invalid start/end indices");
            }

            std::vector<uint8_t> copy_bytes(bv->bytes().begin() + start, bv->bytes().begin() + end);
            return gc_new<a_bytevector>(copy_bytes);
        }

        case BYTEVECTORCOPYMUT: {
            // (bytevector-copy! to at from [start [end]])
            if (!is_bytevector(x)) {
                return error("bytevector-copy!: 'to' must be a bytevector");
            }
            if (!numeric::is_number(y)) {
                return error("bytevector-copy!: 'at' must be a number");
            }

            Object* from_obj = z;
            if (!is_bytevector(from_obj)) {
                return error("bytevector-copy!: 'from' must be a bytevector");
            }

            a_bytevector* to_bv = static_cast<a_bytevector*>(x);
            int at = static_cast<int>(numeric::to_int64(y));
            a_bytevector* from_bv = static_cast<a_bytevector*>(from_obj);

            int start = 0;
            int end = from_bv->length();

            // Optional start and end
            Object* rest = cdr(cdr(cdr(args)));  // Skip first 3 args
            if (rest && !rest->is_nil()) {
                Object* start_obj = car(rest);
                if (!numeric::is_number(start_obj)) {
                    return error("bytevector-copy!: start must be a number");
                }
                start = static_cast<int>(numeric::to_int64(start_obj));

                rest = cdr(rest);
                if (rest && !rest->is_nil()) {
                    Object* end_obj = car(rest);
                    if (!numeric::is_number(end_obj)) {
                        return error("bytevector-copy!: end must be a number");
                    }
                    end = static_cast<int>(numeric::to_int64(end_obj));
                }
            }

            if (start < 0 || start > from_bv->length() || end < start || end > from_bv->length()) {
                return error("bytevector-copy!: invalid start/end indices");
            }

            int count = end - start;
            if (at < 0 || at + count > to_bv->length()) {
                return error("bytevector-copy!: invalid 'at' index or insufficient space");
            }

            // Copy bytes
            std::copy(from_bv->bytes().begin() + start,
                      from_bv->bytes().begin() + end,
                      to_bv->bytes().begin() + at);

            return to_bv;
        }

        case BYTEVECTORAPPEND: {
            std::vector<uint8_t> result;

            Object* current = args;
            while (current && !current->is_nil()) {
                Object* bv_obj = car(current);
                if (!is_bytevector(bv_obj)) {
                    return error("bytevector-append: all arguments must be bytevectors");
                }

                a_bytevector* bv = static_cast<a_bytevector*>(bv_obj);
                result.insert(result.end(), bv->bytes().begin(), bv->bytes().end());

                current = cdr(current);
            }

            return gc_new<a_bytevector>(result);
        }

        // ====================================================================
        // UTF-8 Conversion
        // ====================================================================
        case UTF8TOSTRING: {
            if (!is_bytevector(x)) {
                return error("utf8->string: argument must be a bytevector");
            }

            a_bytevector* bv = static_cast<a_bytevector*>(x);
            int start = 0;
            int end = bv->length();

            if (y && !y->is_nil()) {
                if (!numeric::is_number(y)) {
                    return error("utf8->string: start must be a number");
                }
                start = static_cast<int>(numeric::to_int64(y));

                if (z && !z->is_nil()) {
                    if (!numeric::is_number(z)) {
                        return error("utf8->string: end must be a number");
                    }
                    end = static_cast<int>(numeric::to_int64(z));
                }
            }

            if (start < 0 || start > bv->length() || end < start || end > bv->length()) {
                return error("utf8->string: invalid start/end indices");
            }

            // Convert UTF-8 bytes to string
            std::string str(bv->bytes().begin() + start, bv->bytes().begin() + end);
            return gc_new<a_string>(str);
        }

        case STRINGTOUTF8: {
            if (x->get_type() != static_cast<int>(Type::STRING)) {
                return error("string->utf8: argument must be a string");
            }

            a_string* str = static_cast<a_string*>(x);
            const std::string& s = str->value();

            int start = 0;
            int end = static_cast<int>(s.length());

            if (y && !y->is_nil()) {
                if (!numeric::is_number(y)) {
                    return error("string->utf8: start must be a number");
                }
                start = static_cast<int>(numeric::to_int64(y));

                if (z && !z->is_nil()) {
                    if (!numeric::is_number(z)) {
                        return error("string->utf8: end must be a number");
                    }
                    end = static_cast<int>(numeric::to_int64(z));
                }
            }

            if (start < 0 || start > static_cast<int>(s.length()) ||
                end < start || end > static_cast<int>(s.length())) {
                return error("string->utf8: invalid start/end indices");
            }

            // Convert string to UTF-8 bytes
            std::vector<uint8_t> bytes(s.begin() + start, s.begin() + end);
            return gc_new<a_bytevector>(bytes);
        }

        default:
            return error("Unknown bytevector Primitive");
    }
}

// ============================================================================
// Installation
// ============================================================================

void PrimitiveBytevectors::install(Environment* env) {
    const int n = 999999;

    // GC Safety: Use macro to protect name string before allocating primitive
    #define DEF(name, ...) do { \
        gc_root<a_string> _n(gc_new<a_string>(name)); \
        env->define(_n.get(), gc_new<PrimitiveBytevectors>(__VA_ARGS__)); \
    } while(0)

    // Type checking
    DEF("bytevector?", BYTEVECTORQ, 1, 1);

    // Construction
    DEF("make-bytevector", MAKEBYTEVECTOR, 1, 2);
    DEF("bytevector",      BYTEVECTOR, 0, n);

    // Access and mutation
    DEF("bytevector-length",   BYTEVECTORLENGTH, 1, 1);
    DEF("bytevector-u8-ref",   BYTEVECTORU8REF, 2, 2);
    DEF("bytevector-u8-set!",  BYTEVECTORU8SET, 3, 3);

    // Copying
    DEF("bytevector-copy",   BYTEVECTORCOPY, 1, 3);
    DEF("bytevector-copy!",  BYTEVECTORCOPYMUT, 3, 5);
    DEF("bytevector-append", BYTEVECTORAPPEND, 0, n);

    // UTF-8 conversion
    DEF("utf8->string", UTF8TOSTRING, 1, 3);
    DEF("string->utf8", STRINGTOUTF8, 1, 3);

    #undef DEF
}
