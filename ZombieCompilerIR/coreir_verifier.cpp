#include "pch.h"
#include "coreir_verifier.h"
#include <sstream>
#include <algorithm>

namespace coreir {

// ============================================================================
// Error Reporting
// ============================================================================

void CoreIRVerifier::error(const std::string& msg) {
    std::ostringstream oss;
    if (!current_function.empty()) {
        oss << "Error in function '" << current_function << "': " << msg;
    } else {
        oss << "Error: " << msg;
    }
    errors.push_back(oss.str());
    error_count++;
}

void CoreIRVerifier::reset() {
    defined_vars.clear();
    param_list.clear();
    current_function.clear();
    errors.clear();
    error_count = 0;
}

void CoreIRVerifier::clear() {
    reset();
}

std::string CoreIRVerifier::get_errors() const {
    std::ostringstream oss;
    for (size_t i = 0; i < errors.size(); i++) {
        oss << errors[i];
        if (i + 1 < errors.size()) {
            oss << "\n";
        }
    }
    return oss.str();
}

// ============================================================================
// Atom Verification
// ============================================================================

void CoreIRVerifier::verify_atom(const Atom& a, const std::string& context) {
    if (a.isVar()) {
        const auto& var = a.asVar();
        if (defined_vars.find(var.name) == defined_vars.end()) {
            error(context + ": Use of undefined variable '" + var.name + "'");
        }
    }
    // Lit atoms are always valid (they hold Object* values)
}

// ============================================================================
// Expression Verification
// ============================================================================

void CoreIRVerifier::verify_expr(const Expr& e, const std::string& context) {
    if (e.isPrim()) {
        const auto& prim = e.asPrim();

        // Check all arguments are valid atoms
        for (size_t i = 0; i < prim.args.size(); i++) {
            std::ostringstream arg_ctx;
            arg_ctx << context << " (arg " << (i + 1) << " of Primitive '" << prim.op << "')";
            verify_atom(prim.args[i], arg_ctx.str());
        }

        // Check Primitive arity
        verify_primitive_arity(prim.op, static_cast<int>(prim.args.size()), context);

        // Check Primitive argument types (optional type hints)
        verify_primitive_arg_types(prim.op, prim.args, context);
    }
    else if (e.isCall()) {
        const auto& call = e.asCall();

        // Check function operand is valid
        verify_atom(call.func, context + " (function position in call)");

        // Check all arguments are valid atoms
        for (size_t i = 0; i < call.args.size(); i++) {
            std::ostringstream arg_ctx;
            arg_ctx << context << " (arg " << (i + 1) << " of call)";
            verify_atom(call.args[i], arg_ctx.str());
        }
    }
    else if (e.isGetGlobal()) {
        // GetGlobal just references a name - always valid
        // (we don't track global scope, only local scope)
    }
    else if (e.isSetGlobal()) {
        const auto& setg = e.asSetGlobal();
        verify_atom(setg.value, context + " (value in set-global)");
    }
    else if (e.isSetLocal()) {
        const auto& setl = e.asSetLocal();
        verify_atom(setl.value, context + " (value in set-local)");
    }
    else if (e.isLambda()) {
        const auto& lambda = e.asLambda();

        // Save outer scope
        auto outer_defined = defined_vars;
        auto outer_params = param_list;
        std::string outer_function = current_function;

        // Set up lambda scope
        current_function = "<lambda>";
        defined_vars.clear();
        param_list.clear();

        // Add lambda parameters to scope
        std::set<std::string> param_set;
        for (const auto& param : lambda.params) {
            if (param_set.find(param) != param_set.end()) {
                error("Duplicate parameter name '" + param + "' in lambda");
            }
            param_set.insert(param);
            defined_vars.insert(param);
            param_list.push_back(param);
        }

        // Verify lambda body
        if (lambda.body) {
            verify_stmt(*lambda.body);

            // Check that body ends with Return
            if (!check_ends_with_return(*lambda.body)) {
                error("Lambda body does not end with Return statement");
            }
        } else {
            error("Lambda has null body");
        }

        // Restore outer scope
        defined_vars = outer_defined;
        param_list = outer_params;
        current_function = outer_function;
    }
    else if (e.isValues()) {
        const auto& vals = e.asValues();
        // Verify all value atoms
        for (size_t i = 0; i < vals.values.size(); i++) {
            verify_atom(vals.values[i], context + " (value " + std::to_string(i) + " in values)");
        }
    }
    else if (e.isCallWithValues()) {
        const auto& cwv = e.asCallWithValues();
        // Verify producer and consumer are valid atoms
        verify_atom(cwv.producer, context + " (producer in call-with-values)");
        verify_atom(cwv.consumer, context + " (consumer in call-with-values)");
    }
}

// ============================================================================
// Statement Verification
// ============================================================================

void CoreIRVerifier::verify_stmt(const Stmt& s) {
    if (s.isLet()) {
        const auto& let = s.asLet();

        // Check for duplicate binding name
        if (defined_vars.find(let.name) != defined_vars.end()) {
            // Allow shadowing in nested scopes (common in IR)
            // Just issue a warning via error message (optional)
            // For now, allow it silently
        }

        // Verify the expression being bound
        if (let.expr) {
            verify_expr(*let.expr, "Let binding '" + let.name + "'");
        } else {
            error("Let binding '" + let.name + "' has null expression");
        }

        // Add binding to scope
        defined_vars.insert(let.name);

        // Verify Continuation
        if (let.next) {
            verify_stmt(*let.next);
        } else {
            error("Let binding '" + let.name + "' has null Continuation");
        }
    }
    else if (s.isIf()) {
        const auto& if_stmt = s.asIf();

        // Verify condition is a valid atom
        verify_atom(if_stmt.cond, "If condition");

        // Verify both branches
        if (if_stmt.then_branch) {
            verify_stmt(*if_stmt.then_branch);

            // Check that then branch ends with Return
            if (!check_ends_with_return(*if_stmt.then_branch)) {
                error("If then-branch does not end with Return statement");
            }
        } else {
            error("If statement has null then-branch");
        }

        if (if_stmt.else_branch) {
            verify_stmt(*if_stmt.else_branch);

            // Check that else branch ends with Return
            if (!check_ends_with_return(*if_stmt.else_branch)) {
                error("If else-branch does not end with Return statement");
            }
        } else {
            error("If statement has null else-branch");
        }
    }
    else if (s.isReturn()) {
        const auto& ret = s.asReturn();

        // Verify return value is a valid atom
        verify_atom(ret.value, "Return statement");
    }
}

// ============================================================================
// Primitive Arity Checking
// ============================================================================

void CoreIRVerifier::verify_primitive_arity(const std::string& op, int actual_args, const std::string& context) {
    auto it = primitive_arities.find(op);

    if (it == primitive_arities.end()) {
        // Unknown Primitive - issue warning but don't fail
        // (allows extensibility)
        error(context + ": Warning - unknown Primitive '" + op + "' (cannot verify arity)");
        return;
    }

    int expected = it->second;

    if (expected >= 0) {
        // Fixed arity
        if (actual_args != expected) {
            std::ostringstream oss;
            oss << context << ": Primitive '" << op << "' expects "
                << expected << " argument" << (expected != 1 ? "s" : "")
                << ", got " << actual_args;
            error(oss.str());
        }
    } else {
        // Variadic: -N means at least N arguments
        int minimum = -expected;
        if (actual_args < minimum) {
            std::ostringstream oss;
            oss << context << ": Primitive '" << op << "' expects at least "
                << minimum << " argument" << (minimum != 1 ? "s" : "")
                << ", got " << actual_args;
            error(oss.str());
        }
    }
}

// ============================================================================
// Type Checking Helpers
// ============================================================================

bool CoreIRVerifier::is_numeric_type(TypeHint hint) {
    return hint == TypeHint::INT || hint == TypeHint::NUM;
}

bool CoreIRVerifier::is_compatible(TypeHint expected, TypeHint actual) {
    // UNKNOWN is compatible with everything (no type info)
    if (expected == TypeHint::UNKNOWN || actual == TypeHint::UNKNOWN) {
        return true;
    }

    // Exact match
    if (expected == actual) {
        return true;
    }

    // INT is compatible with NUM
    if (expected == TypeHint::NUM && actual == TypeHint::INT) {
        return true;
    }

    return false;
}

void CoreIRVerifier::verify_primitive_arg_types(const std::string& op, const std::vector<Atom>& args, const std::string& context) {
    // Type checking for common primitives
    // Only check if type hints are available

    if (op == "+" || op == "-" || op == "*" || op == "/") {
        // Arithmetic operations expect numeric arguments
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i].type_hint != TypeHint::UNKNOWN && !is_numeric_type(args[i].type_hint)) {
                std::ostringstream oss;
                oss << context << ": Arithmetic operation '" << op << "' expects numeric argument "
                    << (i + 1) << ", got " << type_hint_name(args[i].type_hint);
                error(oss.str());
            }
        }
    } else if (op == "car" || op == "cdr") {
        // List operations expect list argument
        if (!args.empty() && args[0].type_hint != TypeHint::UNKNOWN && args[0].type_hint != TypeHint::LIST) {
            error(context + ": Primitive '" + op + "' expects list argument, got " +
                  type_hint_name(args[0].type_hint));
        }
    } else if (op == "string-append" || op == "string-ref" || op == "string-length") {
        // String operations expect string argument(s)
        if (!args.empty() && args[0].type_hint != TypeHint::UNKNOWN && args[0].type_hint != TypeHint::STRING) {
            error(context + ": Primitive '" + op + "' expects string argument, got " +
                  type_hint_name(args[0].type_hint));
        }
    }
    // Add more type checks as needed
}

// ============================================================================
// Return Statement Checking
// ============================================================================

bool CoreIRVerifier::check_ends_with_return(const Stmt& s) {
    if (s.isReturn()) {
        return true;
    } else if (s.isLet()) {
        const auto& let = s.asLet();
        if (let.next) {
            return check_ends_with_return(*let.next);
        }
        return false;
    } else if (s.isIf()) {
        // If statement ends with return if BOTH branches do
        const auto& if_stmt = s.asIf();
        bool then_ok = if_stmt.then_branch && check_ends_with_return(*if_stmt.then_branch);
        bool else_ok = if_stmt.else_branch && check_ends_with_return(*if_stmt.else_branch);
        return then_ok && else_ok;
    }
    return false;
}

// ============================================================================
// Function Verification
// ============================================================================

bool CoreIRVerifier::verify(const CoreIRFunction& f) {
    reset();

    current_function = f.name;

    // Check for duplicate parameter names
    std::set<std::string> param_set;
    for (const auto& param : f.params) {
        if (param_set.find(param) != param_set.end()) {
            error("Duplicate parameter name '" + param + "' in function signature");
        }
        param_set.insert(param);
        defined_vars.insert(param);
        param_list.push_back(param);
    }

    // Verify function body
    if (f.body) {
        verify_stmt(*f.body);

        // Check that function body ends with Return
        if (!check_ends_with_return(*f.body)) {
            error("Function body does not end with Return statement");
        }
    } else {
        error("Function has null body");
    }

    return !has_errors();
}

} // namespace coreir
