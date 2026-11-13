#pragma once

#include "../primitive_base.h"

/**
 * @file primitive_value_ledger.h
 * @brief Value Ledger primitives for provenance tracking
 * @date 2026-01-20
 *
 * Implements Value Ledger operations for tracking value origins,
 * backends, and transformations (skins).
 */

/**
 * @brief Value Ledger primitives
 *
 * Handles all Value Ledger operations for provenance tracking.
 *
 * **Example Usage:**
 * @code
 * (value-info obj)            ; Get info about a value
 * (value-id obj)              ; Get stable ID for a value
 * (value-backend obj)         ; Get backend tag (scheme/native/bytecode)
 * (value-origin obj)          ; Get origin string
 * (value-traits obj)          ; Get traits string
 * (use-skin obj skin-name)    ; Apply a skin transformation
 * (capture-snapshot obj)      ; Capture value snapshot
 * (restore-snapshot id)       ; Restore from snapshot
 * (mirror? obj)               ; Check if value is a mirror
 * (list-skins)                ; List registered skins
 * (backend-stats)             ; Get backend statistics
 * (find-by-trait trait)       ; Find values by trait
 * @endcode
 */
class PrimitiveValueLedger : public PrimitiveBase {
public:
    /**
     * @brief Primitive IDs for Value Ledger operations
     */
    enum ID {
        VALUE_INFO = 0,       ///< value-info
        VALUE_ID = 1,         ///< value-id
        VALUE_BACKEND = 2,    ///< value-backend
        VALUE_ORIGIN = 3,     ///< value-origin
        VALUE_TRAITS = 4,     ///< value-traits
        USE_SKIN = 5,         ///< use-skin
        CAPTURE_SNAPSHOT = 6, ///< capture-snapshot
        RESTORE_SNAPSHOT = 7, ///< restore-snapshot
        IS_MIRROR = 8,        ///< mirror?
        LIST_SKINS = 9,       ///< list-skins
        BACKEND_STATS = 10,   ///< backend-stats
        FIND_BY_TRAIT = 11    ///< find-by-trait
    };

    /**
     * @brief Construct Value Ledger Primitive
     * @param id Which Value Ledger Primitive
     * @param min_args Minimum argument count
     * @param max_args Maximum argument count (-1 for unlimited)
     */
    PrimitiveValueLedger(ID id, int min_args, int max_args);

    /**
     * @brief Apply the Value Ledger Primitive
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all Value Ledger primitives into environment
     */
    static void install(Environment* env);

private:
    ID m_id;  ///< Which Value Ledger Primitive this is
};
