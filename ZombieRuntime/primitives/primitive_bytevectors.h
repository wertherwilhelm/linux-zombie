#pragma once

/**
 * @file primitive_bytevectors.h
 * @brief R7RS Bytevector primitives (Section 6.9)
 */

#include "../primitive_base.h"
#include "../Environment.h"

/**
 * @brief R7RS Bytevector primitives
 *
 * Implements all R7RS-small bytevector operations:
 * - Type checking: bytevector?
 * - Construction: make-bytevector, bytevector
 * - Access: bytevector-length, bytevector-u8-ref, bytevector-u8-set!
 * - Copying: bytevector-copy, bytevector-copy!, bytevector-append
 * - UTF-8 conversion: utf8->string, string->utf8
 */
class PrimitiveBytevectors : public PrimitiveBase {
public:
    enum ID {
        BYTEVECTORQ,         // bytevector?
        MAKEBYTEVECTOR,      // make-bytevector
        BYTEVECTOR,          // bytevector
        BYTEVECTORLENGTH,    // bytevector-length
        BYTEVECTORU8REF,     // bytevector-u8-ref
        BYTEVECTORU8SET,     // bytevector-u8-set!
        BYTEVECTORCOPY,      // bytevector-copy
        BYTEVECTORCOPYMUT,   // bytevector-copy!
        BYTEVECTORAPPEND,    // bytevector-append
        UTF8TOSTRING,        // utf8->string
        STRINGTOUTF8         // string->utf8
    };

    PrimitiveBytevectors(ID id, int minArgs, int maxArgs);

    /**
     * @brief Apply the Primitive operation
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all bytevector primitives into environment
     * @param env Environment to install into
     */
    static void install(Environment* env);

private:
    ID m_id;
    std::string m_name;
};
