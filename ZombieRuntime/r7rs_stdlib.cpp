#include "pch.h"
#include "r7rs_stdlib.h"
#include "library.h"
#include "library_registry.h"
#include "Base.h"

namespace r7rs {

// ============================================================================
// (scheme base) - Core R7RS Library
// ============================================================================

void bootstrap_scheme_base(Environment* base_env) {
    auto lib = make_gc_ptr<Library>(LibraryName{"scheme", "base"});

    // Arithmetic operations
    lib->add_export("+");
    lib->add_export("-");
    lib->add_export("*");
    lib->add_export("/");
    lib->add_export("=");
    lib->add_export("<");
    lib->add_export(">");
    lib->add_export("<=");
    lib->add_export(">=");

    // List operations
    lib->add_export("cons");
    lib->add_export("car");
    lib->add_export("cdr");
    lib->add_export("list");
    lib->add_export("length");
    lib->add_export("append");
    lib->add_export("reverse");
    lib->add_export("null?");
    lib->add_export("pair?");

    // Type predicates
    lib->add_export("number?");
    lib->add_export("integer?");
    lib->add_export("real?");
    lib->add_export("rational?");
    lib->add_export("complex?");
    lib->add_export("exact?");
    lib->add_export("inexact?");
    lib->add_export("boolean?");
    lib->add_export("symbol?");
    lib->add_export("string?");
    lib->add_export("char?");
    lib->add_export("vector?");
    lib->add_export("Procedure?");

    // Boolean operations
    lib->add_export("not");
    lib->add_export("and");
    lib->add_export("or");

    // Equivalence
    lib->add_export("eq?");
    lib->add_export("eqv?");
    lib->add_export("equal?");

    // String operations
    lib->add_export("string");
    lib->add_export("string?");
    lib->add_export("string-length");
    lib->add_export("string-ref");
    lib->add_export("string-set!");
    lib->add_export("string-append");
    lib->add_export("substring");
    lib->add_export("string->list");
    lib->add_export("list->string");

    // Vector operations
    lib->add_export("vector");
    lib->add_export("vector?");
    lib->add_export("vector-length");
    lib->add_export("vector-ref");
    lib->add_export("vector-set!");
    lib->add_export("make-vector");

    // Character operations
    lib->add_export("char?");
    lib->add_export("char=?");
    lib->add_export("char<?");
    lib->add_export("char>?");
    lib->add_export("char<=?");
    lib->add_export("char>=?");

    // Symbol operations
    lib->add_export("symbol?");
    lib->add_export("symbol->string");
    lib->add_export("string->symbol");

    // Control flow (exported for documentation, handled by eval)
    // Note: These are syntax, not procedures, but we export them
    // so that code can reference them
    lib->add_export("if");
    lib->add_export("cond");
    lib->add_export("case");
    lib->add_export("and");
    lib->add_export("or");
    lib->add_export("when");
    lib->add_export("unless");
    lib->add_export("begin");
    lib->add_export("lambda");
    lib->add_export("define");
    lib->add_export("set!");
    lib->add_export("let");
    lib->add_export("let*");
    lib->add_export("letrec");
    lib->add_export("letrec*");
    lib->add_export("do");

    // Numerical operations
    lib->add_export("quotient");
    lib->add_export("remainder");
    lib->add_export("modulo");
    lib->add_export("floor");
    lib->add_export("ceiling");
    lib->add_export("truncate");
    lib->add_export("round");
    lib->add_export("abs");
    lib->add_export("max");
    lib->add_export("min");
    lib->add_export("gcd");
    lib->add_export("lcm");
    lib->add_export("numerator");
    lib->add_export("denominator");
    lib->add_export("rationalize");
    lib->add_export("sqrt");
    lib->add_export("expt");
    lib->add_export("exp");
    lib->add_export("log");
    lib->add_export("sin");
    lib->add_export("cos");
    lib->add_export("tan");
    lib->add_export("asin");
    lib->add_export("acos");
    lib->add_export("atan");

    // Conversion
    lib->add_export("number->string");
    lib->add_export("string->number");

    // List operations (continued)
    lib->add_export("map");
    lib->add_export("for-each");
    lib->add_export("member");
    lib->add_export("memq");
    lib->add_export("memv");
    lib->add_export("assoc");
    lib->add_export("assq");
    lib->add_export("assv");

    // Apply
    lib->add_export("apply");

    // Values and call-with-values (for multiple return values)
    lib->add_export("values");
    lib->add_export("call-with-values");

    // Dynamic-wind (for resource management with continuations)
    lib->add_export("dynamic-wind");

    // Continuations
    lib->add_export("call-with-current-Continuation");
    lib->add_export("call/cc");

    // Error handling
    lib->add_export("error");

    // Initialize library with Base environment
    // This gives the library access to all primitives
    std::string error;
    if (!lib->initialize(base_env, error)) {
        throw std::runtime_error("Failed to initialize (scheme base): " + error);
    }

    // Mark as initialized (it has no begin blocks to evaluate)
    lib->set_initialized(true);

    // Register in global registry
    LibraryRegistry::instance().register_library(std::move(lib));
}

// ============================================================================
// (scheme read) - Reading S-expressions
// ============================================================================

void bootstrap_scheme_read(Environment* base_env) {
    auto lib = make_gc_ptr<Library>(LibraryName{"scheme", "read"});

    lib->add_export("read");
    lib->add_export("read-char");
    lib->add_export("peek-char");
    lib->add_export("char-ready?");
    lib->add_export("eof-Object?");

    std::string error;
    if (!lib->initialize(base_env, error)) {
        throw std::runtime_error("Failed to initialize (scheme read): " + error);
    }

    lib->set_initialized(true);
    LibraryRegistry::instance().register_library(std::move(lib));
}

// ============================================================================
// (scheme write) - Writing S-expressions
// ============================================================================

void bootstrap_scheme_write(Environment* base_env) {
    auto lib = make_gc_ptr<Library>(LibraryName{"scheme", "write"});

    lib->add_export("write");
    lib->add_export("display");
    lib->add_export("newline");
    lib->add_export("write-char");

    std::string error;
    if (!lib->initialize(base_env, error)) {
        throw std::runtime_error("Failed to initialize (scheme write): " + error);
    }

    lib->set_initialized(true);
    LibraryRegistry::instance().register_library(std::move(lib));
}

// ============================================================================
// (scheme file) - File operations
// ============================================================================

void bootstrap_scheme_file(Environment* base_env) {
    auto lib = make_gc_ptr<Library>(LibraryName{"scheme", "file"});

    lib->add_export("call-with-input-file");
    lib->add_export("call-with-output-file");
    lib->add_export("open-input-file");
    lib->add_export("open-output-file");
    lib->add_export("close-input-port");
    lib->add_export("close-output-port");
    lib->add_export("input-port?");
    lib->add_export("output-port?");
    lib->add_export("file-exists?");
    lib->add_export("delete-file");

    std::string error;
    if (!lib->initialize(base_env, error)) {
        throw std::runtime_error("Failed to initialize (scheme file): " + error);
    }

    lib->set_initialized(true);
    LibraryRegistry::instance().register_library(std::move(lib));
}

// ============================================================================
// Bootstrap All Standard Libraries
// ============================================================================

void bootstrap_standard_libraries(Environment* base_env) {
    bootstrap_scheme_base(base_env);
    bootstrap_scheme_read(base_env);
    bootstrap_scheme_write(base_env);
    bootstrap_scheme_file(base_env);
}

} // namespace r7rs
