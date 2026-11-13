#pragma once

#include "library.h"
#include "Object.h"
#include "gc_ptr.h"
#include <string>

/**
 * @file library_parser.h
 * @brief R7RS library parser for define-library forms.
 *
 * Parses define-library forms into Library objects.
 *
 * @code
 * (define-library (library name)
 *   (import (scheme base))
 *   (export foo bar)
 *   (begin
 *     (define (foo x) (* x x))
 *     (define bar 42)))
 * @endcode
 */

namespace r7rs {

/**
 * @brief Parses R7RS define-library forms into Library objects.
 *
 * Input is a cons list from scheme_reader. Output is a fully populated
 * Library with name, imports, exports, and begin forms ready for evaluation.
 */
class LibraryParser {
public:
    LibraryParser() = default;

    /**
     * @brief Check if an expression is a define-library form.
     * @param expr Expression to inspect.
     * @return True if expr is a define-library form.
     */
    bool is_library_definition(Object* expr) const;

    /**
     * @brief Parse a define-library form into a Library Object.
     * @param expr Expression to parse.
     * @return Parsed Library instance.
     * @throws std::runtime_error on parse errors.
     */
    gc_ptr<Library> parse_library_definition(Object* expr);

    /**
     * @brief Parse import declaration (public for REPL integration).
     *
     * Forms supported:
     * (import (scheme base))
     * (import (scheme base) (libs a))  -- multiple import sets
     * (import (only (scheme base) + -))
     * (import (except (scheme base) eval))
     * (import (prefix (scheme base) Base:))
     * (import (rename (scheme base) (+ plus)))
     *
     * @param import_expr Import expression to parse.
     * @return Vector of ImportSpec (one per import set).
     */
    std::vector<ImportSpec> parse_import_declaration(Object* import_expr);

    /**
     * @brief Parse a single import set (public for evaluator integration).
     *
     * Handles: (lib), (only (lib) ...), (except (lib) ...),
     * (prefix (lib) ...), (rename (lib) ...)
     *
     * @param import_set Import set to parse.
     * @return Parsed ImportSpec.
     */
    ImportSpec parse_import_set(Object* import_set);

private:
    // Parse library name: (scheme base) -> LibraryName
    LibraryName parse_library_name(Object* name_expr);

    // Parse export declaration
    // (export foo bar)
    // (export (rename internal external))
    std::vector<ExportSpec> parse_export_declaration(Object* export_expr);

    // Parse begin block (definitions)
    // Returns list of definition forms to be evaluated
    Object* parse_begin_block(Object* begin_expr);

    // Helpers
    bool is_symbol(Object* obj, const std::string& name) const;
    std::string get_symbol_name(Object* obj) const;
    void expect_list(Object* obj, const std::string& context) const;
    void expect_symbol(Object* obj, const std::string& context) const;
};

} // namespace r7rs
