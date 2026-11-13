#pragma once

#include "../primitive_base.h"

/**
 * @file primitives_export_registry.h
 * @brief Scheme primitives for accessing the ExportRegistry and SignatureRegistry
 *
 * Provides Scheme-level introspection of:
 * - ExportRegistry: Named function exports with provenance
 * - SignatureRegistry: Function signatures with arity
 *
 * Primitives:
 *   (list-exports)         - Returns list of all export names
 *   (list-signatures)      - Returns list of all signature names
 *   (has-export? name)     - Check if export exists
 *   (has-signature? name)  - Check if signature exists
 *   (export-origin name)   - Get origin of an export
 *   (export-traits name)   - Get traits of an export
 *   (signature-arity name) - Get arity of a signature
 *   (signature-type name)  - Get type signature string
 *   (export-count)         - Get number of exports
 *   (signature-count)      - Get number of signatures
 */

/**
 * @brief Export/Signature Registry primitives
 *
 * Allows Scheme code to introspect the function export and signature registries.
 * Useful for debugging, reflection, and dynamic function discovery.
 *
 * **Example Usage:**
 * @code
 * (list-exports)            ; => ("cpp-square" "scheme-add" ...)
 * (has-export? "cpp-square") ; => #t
 * (export-origin "cpp-square") ; => "cpp"
 * (signature-arity "cpp-square") ; => 1
 * @endcode
 */
class PrimitiveExportRegistry : public PrimitiveBase {
public:
    /**
     * @brief Primitive IDs for export registry operations
     */
    enum ID {
        LIST_EXPORTS = 0,      ///< (list-exports)
        LIST_SIGNATURES = 1,   ///< (list-signatures)
        HAS_EXPORT = 2,        ///< (has-export? name)
        HAS_SIGNATURE = 3,     ///< (has-signature? name)
        EXPORT_ORIGIN = 4,     ///< (export-origin name)
        EXPORT_TRAITS = 5,     ///< (export-traits name)
        SIGNATURE_ARITY = 6,   ///< (signature-arity name)
        SIGNATURE_TYPE = 7,    ///< (signature-type name)
        EXPORT_COUNT = 8,      ///< (export-count)
        SIGNATURE_COUNT = 9    ///< (signature-count)
    };

    /**
     * @brief Construct export registry Primitive
     * @param id Which Primitive operation
     */
    PrimitiveExportRegistry(ID id);

    /**
     * @brief Apply Primitive to arguments
     * @param eval Evaluator (not used)
     * @param args Argument list
     * @param env Environment (not used)
     * @return Result of operation
     */
    Object* apply(IEvaluator* eval, Object* args, Environment* env) override;

    /**
     * @brief Install all export registry primitives into environment
     * @param env Environment to install primitives into
     */
    static void install(Environment* env);

private:
    ID m_id;  ///< Which Primitive this instance represents
};
