#pragma once

#include "coreir.h"
#include <string>
#include <vector>
#include <set>
#include <map>

namespace coreir {

// ============================================================================
// CoreIR Verifier - Validates well-formedness of CoreIR
// ============================================================================

class CoreIRVerifier {
private:
    // State tracking
    std::set<std::string> defined_vars;       // Variables in scope (params + let bindings)
    std::vector<std::string> param_list;      // Function parameters (for display)
    std::string current_function;             // Current function name (for error messages)

    // Error accumulation
    std::vector<std::string> errors;
    int error_count = 0;

    // Primitive operation arities (expected number of arguments)
    // -1 means variadic (any number >= minimum)
    std::map<std::string, int> primitive_arities = {
        // Arithmetic (variadic, at least 2)
        {"+", -2}, {"-", -1}, {"*", -2}, {"/", -1},

        // Comparison (exactly 2)
        {"<", 2}, {">", 2}, {"<=", 2}, {">=", 2}, {"=", 2},

        // Boolean (variadic)
        {"and", -2}, {"or", -2}, {"not", 1},

        // List operations
        {"cons", 2}, {"car", 1}, {"cdr", 1},
        {"list", -0},  // variadic, 0 or more
        {"append", -2}, // at least 2
        {"null?", 1}, {"pair?", 1}, {"list?", 1},

        // Vector operations
        {"vector", -0}, {"make-vector", -1}, // 1 or 2 args
        {"vector-ref", 2}, {"vector-set!", 3}, {"vector-length", 1},

        // String operations
        {"string-append", -2}, {"string-length", 1},
        {"string-ref", 2}, {"string-set!", 3},

        // Type predicates
        {"number?", 1}, {"integer?", 1}, {"real?", 1},
        {"string?", 1}, {"symbol?", 1}, {"boolean?", 1},
        {"Procedure?", 1}, {"vector?", 1},

        // Conversions
        {"number->string", 1}, {"string->number", 1},
        {"symbol->string", 1}, {"string->symbol", 1},

        // I/O
        {"display", 1}, {"newline", 0}, {"write", 1},

        // Other
        {"eq?", 2}, {"equal?", 2}, {"eqv?", 2}
    };

    // Helper methods
    void error(const std::string& msg);
    void reset();

    // Type checking helpers
    bool is_numeric_type(TypeHint hint);
    bool is_compatible(TypeHint expected, TypeHint actual);
    void verify_primitive_arg_types(const std::string& op, const std::vector<Atom>& args, const std::string& context);

    // Verification methods
    void verify_atom(const Atom& a, const std::string& context);
    void verify_expr(const Expr& e, const std::string& context);
    void verify_stmt(const Stmt& s);
    void verify_primitive_arity(const std::string& op, int actual_args, const std::string& context);
    bool check_ends_with_return(const Stmt& s);

public:
    // Verify a complete function
    bool verify(const CoreIRFunction& f);

    // Get accumulated errors
    std::string get_errors() const;
    bool has_errors() const { return !errors.empty(); }
    int get_error_count() const { return error_count; }

    // Clear state for reuse
    void clear();
};

} // namespace coreir
