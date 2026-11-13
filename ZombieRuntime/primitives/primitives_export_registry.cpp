/**
 * @file primitives_export_registry.cpp
 * @brief Implementation of export/signature registry primitives
 */

#include "../pch.h"
#include "primitives_export_registry.h"
#include "../export_registry.h"
#include "../signature_registry.h"
#include "../atom.h"
#include "../cons_cell.h"
#include "../Base.h"
#include "../Environment.h"
#include "../numeric_runtime.h"
#include <string>

// ============================================================================
// Constructor
// ============================================================================

PrimitiveExportRegistry::PrimitiveExportRegistry(ID id)
    : PrimitiveBase(
        // minArgs: list-exports, list-signatures, export-count, signature-count = 0, others = 1
        (id == LIST_EXPORTS || id == LIST_SIGNATURES || id == EXPORT_COUNT || id == SIGNATURE_COUNT) ? 0 : 1,
        // maxArgs: same as minArgs
        (id == LIST_EXPORTS || id == LIST_SIGNATURES || id == EXPORT_COUNT || id == SIGNATURE_COUNT) ? 0 : 1
      ),
      m_id(id)
{
    switch (id) {
        case LIST_EXPORTS:     m_name = "list-exports"; break;
        case LIST_SIGNATURES:  m_name = "list-signatures"; break;
        case HAS_EXPORT:       m_name = "has-export?"; break;
        case HAS_SIGNATURE:    m_name = "has-signature?"; break;
        case EXPORT_ORIGIN:    m_name = "export-origin"; break;
        case EXPORT_TRAITS:    m_name = "export-traits"; break;
        case SIGNATURE_ARITY:  m_name = "signature-arity"; break;
        case SIGNATURE_TYPE:   m_name = "signature-type"; break;
        case EXPORT_COUNT:     m_name = "export-count"; break;
        case SIGNATURE_COUNT:  m_name = "signature-count"; break;
    }
}

// ============================================================================
// Helper: Extract string from name argument (string or symbol)
// ============================================================================

static std::string extract_name(Object* name_obj) {
    if (!name_obj) return "";

    if (auto* s = dynamic_cast<a_string*>(name_obj)) {
        return s->value();
    }
    if (auto* sym = dynamic_cast<a_symbol*>(name_obj)) {
        return sym->value();
    }
    return "";
}

// ============================================================================
// Apply Implementation
// ============================================================================

Object* PrimitiveExportRegistry::apply(IEvaluator* eval, Object* args, Environment* env) {
    (void)eval;
    (void)env;

    // Arity checking
    if (!check_arity(args)) {
        return arity_error("");
    }

    switch (m_id) {
        // --------------------------------------------------------------------
        // Export Registry Primitives
        // --------------------------------------------------------------------

        case LIST_EXPORTS: {
            auto names = ExportRegistry::instance().list_names();
            Object* result = nil;
            for (auto it = names.rbegin(); it != names.rend(); ++it) {
                result = cons(gc_new<a_string>(*it), result);
            }
            return result;
        }

        case HAS_EXPORT: {
            std::string name = extract_name(first(args));
            if (name.empty()) return nil;
            return truth(ExportRegistry::instance().has(name));
        }

        case EXPORT_ORIGIN: {
            std::string name = extract_name(first(args));
            if (name.empty()) return nil;
            auto entry = ExportRegistry::instance().get(name);
            if (!entry) return nil;
            return gc_new<a_string>(entry->origin);
        }

        case EXPORT_TRAITS: {
            std::string name = extract_name(first(args));
            if (name.empty()) return nil;
            auto entry = ExportRegistry::instance().get(name);
            if (!entry) return nil;
            return gc_new<a_string>(entry->traits);
        }

        case EXPORT_COUNT: {
            return numeric::make_integer(static_cast<int64_t>(ExportRegistry::instance().count()));
        }

        // --------------------------------------------------------------------
        // Signature Registry Primitives
        // --------------------------------------------------------------------

        case LIST_SIGNATURES: {
            auto entries = SignatureRegistry::instance().list();
            Object* result = nil;
            for (auto it = entries.rbegin(); it != entries.rend(); ++it) {
                result = cons(gc_new<a_string>(it->name), result);
            }
            return result;
        }

        case HAS_SIGNATURE: {
            std::string name = extract_name(first(args));
            if (name.empty()) return nil;
            return truth(SignatureRegistry::instance().has(name));
        }

        case SIGNATURE_ARITY: {
            std::string name = extract_name(first(args));
            if (name.empty()) return nil;
            const SignatureEntry* entry = SignatureRegistry::instance().find(name);
            if (!entry) return nil;
            return numeric::make_integer(static_cast<int64_t>(entry->arity));
        }

        case SIGNATURE_TYPE: {
            std::string name = extract_name(first(args));
            if (name.empty()) return nil;
            const SignatureEntry* entry = SignatureRegistry::instance().find(name);
            if (!entry) return nil;
            return gc_new<a_string>(entry->signature);
        }

        case SIGNATURE_COUNT: {
            return numeric::make_integer(static_cast<int64_t>(SignatureRegistry::instance().count()));
        }

        default:
            return nil;
    }
}

// ============================================================================
// Installation
// ============================================================================

void PrimitiveExportRegistry::install(Environment* env) {
    // GC Safety: Use macro to protect name string before allocating primitive
    #define DEF(name, id) do { \
        gc_root<a_string> _n(gc_new<a_string>(name)); \
        env->define(_n.get(), gc_new<PrimitiveExportRegistry>(id)); \
    } while(0)

    // Export registry primitives
    DEF("list-exports",  LIST_EXPORTS);
    DEF("has-export?",   HAS_EXPORT);
    DEF("export-origin", EXPORT_ORIGIN);
    DEF("export-traits", EXPORT_TRAITS);
    DEF("export-count",  EXPORT_COUNT);

    // Signature registry primitives
    DEF("list-signatures", LIST_SIGNATURES);
    DEF("has-signature?",  HAS_SIGNATURE);
    DEF("signature-arity", SIGNATURE_ARITY);
    DEF("signature-type",  SIGNATURE_TYPE);
    DEF("signature-count", SIGNATURE_COUNT);

    #undef DEF
}
