#include "pch.h"
#include "library_parser.h"
#include "Base.h"
#include "atom.h"
#include <sstream>
#include <stdexcept>

namespace r7rs {

// ============================================================================
// Helper Methods
// ============================================================================

bool LibraryParser::is_symbol(Object* obj, const std::string& name) const {
    if (!obj || obj->get_type() != static_cast<int>(Type::SYMBOL)) {
        return false;
    }
    a_symbol* sym = static_cast<a_symbol*>(obj);
    return sym->getName() == name;
}

std::string LibraryParser::get_symbol_name(Object* obj) const {
    if (!obj || obj->get_type() != static_cast<int>(Type::SYMBOL)) {
        throw std::runtime_error("Expected symbol");
    }
    a_symbol* sym = static_cast<a_symbol*>(obj);
    return sym->getName();
}

void LibraryParser::expect_list(Object* obj, const std::string& context) const {
    if (!obj || !obj->pair()) {
        throw std::runtime_error(context + ": expected list");
    }
}

void LibraryParser::expect_symbol(Object* obj, const std::string& context) const {
    if (!obj || obj->get_type() != static_cast<int>(Type::SYMBOL)) {
        throw std::runtime_error(context + ": expected symbol");
    }
}

// ============================================================================
// Top-Level Library Definition Parsing
// ============================================================================

bool LibraryParser::is_library_definition(Object* expr) const {
    if (!expr || !expr->pair()) {
        return false;
    }

    // Check if first element is 'define-library
    Object* first = car(expr);
    return is_symbol(first, "define-library");
}

gc_ptr<Library> LibraryParser::parse_library_definition(Object* expr) {
    if (!is_library_definition(expr)) {
        throw std::runtime_error("Not a define-library form");
    }

    // (define-library (name ...) declarations ...)
    //  ^^^^^^^^^^^^^^^
    Object* rest = cdr(expr);
    if (is_nil(rest)) {
        throw std::runtime_error("define-library: missing library name");
    }

    // Parse library name
    Object* name_expr = car(rest);
    LibraryName lib_name = parse_library_name(name_expr);

    auto lib = make_gc_ptr<Library>(lib_name);

    // Parse declarations
    rest = cdr(rest);
    while (!is_nil(rest)) {
        Object* decl = car(rest);
        expect_list(decl, "Library declaration");

        Object* decl_type = car(decl);
        expect_symbol(decl_type, "Declaration type");

        std::string decl_name = get_symbol_name(decl_type);

        if (decl_name == "import") {
            // Parse import declaration (can have multiple import sets)
            auto imports = parse_import_declaration(decl);
            for (auto& import : imports) {
                lib->add_import(std::move(import));
            }
        }
        else if (decl_name == "export") {
            // Parse export declaration
            auto exports = parse_export_declaration(decl);
            for (auto& exp : exports) {
                lib->add_export(exp);
            }
        }
        else if (decl_name == "begin") {
            // Parse begin block (definitions)
            // Store the forms (without the 'begin keyword) for later evaluation
            Object* forms = parse_begin_block(decl);
            lib->add_begin_block(forms);
        }
        else {
            throw std::runtime_error("Unknown library declaration: " + decl_name);
        }

        rest = cdr(rest);
    }

    return lib;
}

// ============================================================================
// Library Name Parsing
// ============================================================================

LibraryName LibraryParser::parse_library_name(Object* name_expr) {
    // Library name is a list of symbols: (scheme base)
    expect_list(name_expr, "Library name");

    LibraryName result;
    Object* current = name_expr;

    while (!is_nil(current)) {
        expect_list(current, "Library name");
        Object* elem = car(current);
        expect_symbol(elem, "Library name component");

        std::string component = get_symbol_name(elem);
        result.push_back(component);

        current = cdr(current);
    }

    if (result.empty()) {
        throw std::runtime_error("Library name cannot be empty");
    }

    return result;
}

// ============================================================================
// Import Declaration Parsing
// ============================================================================

std::vector<ImportSpec> LibraryParser::parse_import_declaration(Object* import_expr) {
    // (import import-set ...)
    // Can have multiple import sets: (import (scheme base) (libs a))
    expect_list(import_expr, "Import declaration");

    Object* import_keyword = car(import_expr);
    if (!is_symbol(import_keyword, "import")) {
        throw std::runtime_error("Expected 'import' keyword");
    }

    std::vector<ImportSpec> result;

    // Loop through all import sets
    Object* import_sets = cdr(import_expr);
    if (is_nil(import_sets)) {
        throw std::runtime_error("import: missing import set");
    }

    while (!is_nil(import_sets)) {
        Object* import_set = car(import_sets);
        result.push_back(parse_import_set(import_set));
        import_sets = cdr(import_sets);
    }

    return result;
}

ImportSpec LibraryParser::parse_import_set(Object* import_set) {
    // Import set can be:
    // 1. Simple: (library name)
    // 2. Only: (only (library name) identifier ...)
    // 3. Except: (except (library name) identifier ...)
    // 4. Prefix: (prefix (library name) prefix)
    // 5. Rename: (rename (library name) (old new) ...)

    expect_list(import_set, "Import set");

    Object* first = car(import_set);

    // Check if it's a modified import (only, except, prefix, rename)
    if (first && first->get_type() == static_cast<int>(Type::SYMBOL)) {
        std::string modifier = get_symbol_name(first);

        if (modifier == "only") {
            // (only (lib) sym1 sym2 ...)
            Object* lib_expr = car(cdr(import_set));
            LibraryName lib_name = parse_library_name(lib_expr);

            std::set<std::string> symbols;
            Object* syms = cdr(cdr(import_set));
            while (!is_nil(syms)) {
                expect_symbol(car(syms), "only: symbol");
                symbols.insert(get_symbol_name(car(syms)));
                syms = cdr(syms);
            }

            return ImportSpec::make_only(lib_name, symbols);
        }
        else if (modifier == "except") {
            // (except (lib) sym1 sym2 ...)
            Object* lib_expr = car(cdr(import_set));
            LibraryName lib_name = parse_library_name(lib_expr);

            std::set<std::string> symbols;
            Object* syms = cdr(cdr(import_set));
            while (!is_nil(syms)) {
                expect_symbol(car(syms), "except: symbol");
                symbols.insert(get_symbol_name(car(syms)));
                syms = cdr(syms);
            }

            return ImportSpec::make_except(lib_name, symbols);
        }
        else if (modifier == "prefix") {
            // (prefix (lib) prefix-string)
            Object* lib_expr = car(cdr(import_set));
            LibraryName lib_name = parse_library_name(lib_expr);

            Object* prefix_expr = car(cdr(cdr(import_set)));
            expect_symbol(prefix_expr, "prefix: prefix identifier");
            std::string prefix = get_symbol_name(prefix_expr);

            return ImportSpec::make_prefix(lib_name, prefix);
        }
        else if (modifier == "rename") {
            // (rename (lib) (old new) ...)
            Object* lib_expr = car(cdr(import_set));
            LibraryName lib_name = parse_library_name(lib_expr);

            std::map<std::string, std::string> renames;
            Object* rename_pairs = cdr(cdr(import_set));
            while (!is_nil(rename_pairs)) {
                Object* pair = car(rename_pairs);
                expect_list(pair, "rename: (old new) pair");

                Object* old_name = car(pair);
                Object* new_name = car(cdr(pair));
                expect_symbol(old_name, "rename: old name");
                expect_symbol(new_name, "rename: new name");

                renames[get_symbol_name(old_name)] = get_symbol_name(new_name);
                rename_pairs = cdr(rename_pairs);
            }

            return ImportSpec::make_rename(lib_name, renames);
        }
    }

    // Simple import: (library name)
    LibraryName lib_name = parse_library_name(import_set);
    return ImportSpec::make_simple(lib_name);
}

// ============================================================================
// Export Declaration Parsing
// ============================================================================

std::vector<ExportSpec> LibraryParser::parse_export_declaration(Object* export_expr) {
    // (export export-spec ...)
    // export-spec can be:
    // - identifier
    // - (rename internal external)

    expect_list(export_expr, "Export declaration");

    Object* export_keyword = car(export_expr);
    if (!is_symbol(export_keyword, "export")) {
        throw std::runtime_error("Expected 'export' keyword");
    }

    std::vector<ExportSpec> result;
    Object* exports = cdr(export_expr);

    while (!is_nil(exports)) {
        Object* export_spec = car(exports);

        if (export_spec->pair()) {
            // (rename internal external)
            expect_list(export_spec, "Export spec");
            Object* rename_keyword = car(export_spec);
            if (is_symbol(rename_keyword, "rename")) {
                Object* internal = car(cdr(export_spec));
                Object* external = car(cdr(cdr(export_spec)));
                expect_symbol(internal, "export rename: internal name");
                expect_symbol(external, "export rename: external name");

                result.emplace_back(get_symbol_name(internal), get_symbol_name(external));
            } else {
                throw std::runtime_error("Unknown export form");
            }
        } else {
            // Simple identifier
            expect_symbol(export_spec, "Export identifier");
            std::string name = get_symbol_name(export_spec);
            result.emplace_back(name);
        }

        exports = cdr(exports);
    }

    return result;
}

// ============================================================================
// Begin Block Parsing
// ============================================================================

Object* LibraryParser::parse_begin_block(Object* begin_expr) {
    // (begin form1 form2 ...)
    // Returns the list of forms (without the 'begin keyword)

    expect_list(begin_expr, "Begin block");

    Object* begin_keyword = car(begin_expr);
    if (!is_symbol(begin_keyword, "begin")) {
        throw std::runtime_error("Expected 'begin' keyword");
    }

    return cdr(begin_expr);  // Return forms without 'begin
}

} // namespace r7rs
