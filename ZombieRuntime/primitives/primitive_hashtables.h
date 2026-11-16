#pragma once

#include "../primitive_base.h"

namespace zombie { namespace runtime {

/**
 * @file primitive_hashtables.h
 * @brief Hash table primitives (SRFI-69 subset), backed by HashTable.
 */
class PrimitiveHashTables : public PrimitiveBase {
public:
    enum ID {
        MAKE = 0,          ///< make-hash-table [comparator ...] (args ignored)
        HTQ = 1,           ///< hash-table?
        SET = 2,           ///< hash-table-set! h key value
        REF = 3,           ///< hash-table-ref h key [default]
        REFDEFAULT = 4,    ///< hash-table-ref/default h key default
        DELETE = 5,        ///< hash-table-delete! h key
        CONTAINS = 6,      ///< hash-table-contains? h key
        SIZE = 7,          ///< hash-table-size
        KEYS = 8,          ///< hash-table-keys
        VALUES_ = 9,       ///< hash-table-values
        TOALIST = 10       ///< hash-table->alist
    };

    PrimitiveHashTables(ID id, int minArgs, int maxArgs);
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    static void install(Environment* env);

private:
    ID m_id;
};

} } // namespace zombie::runtime
