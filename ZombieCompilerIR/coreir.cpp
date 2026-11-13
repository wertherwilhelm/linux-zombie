#include "pch.h"
#include "coreir.h"
#include "Object.h"
#include "atom.h"
#include <sstream>

namespace coreir {

// ============================================================================
// Type Hint Utilities
// ============================================================================

const char* type_hint_name(TypeHint hint) {
    switch (hint) {
        case TypeHint::UNKNOWN:  return "unknown";
        case TypeHint::INT:      return "int";
        case TypeHint::NUM:      return "num";
        case TypeHint::BOOL:     return "bool";
        case TypeHint::STRING:   return "string";
        case TypeHint::SYMBOL:   return "symbol";
        case TypeHint::CLOSURE:  return "Closure";
        case TypeHint::LIST:     return "list";
        case TypeHint::VECTOR:   return "vector";
        case TypeHint::VOID:     return "void";
        default:                 return "???";
    }
}

// ============================================================================
// Pretty-printing for debugging
// ============================================================================

std::string print_atom(const Atom& atom) {
    if (atom.isVar()) {
        return atom.asVar().name;
    } else {
        // Lit - would need to print the Object*, simplified for now
        return "<literal>";
    }
}

std::string print_expr(const Expr& expr) {
    std::ostringstream oss;

    if (expr.isPrim()) {
        const auto& prim = expr.asPrim();
        oss << "Prim(" << prim.op;
        for (const auto& arg : prim.args) {
            oss << ", " << print_atom(arg);
        }
        oss << ")";
    }
    else if (expr.isCall()) {
        const auto& call = expr.asCall();
        oss << "Call(" << print_atom(call.func);
        for (const auto& arg : call.args) {
            oss << ", " << print_atom(arg);
        }
        oss << ")";
    }
    else if (expr.isGetGlobal()) {
        oss << "GetGlobal(" << expr.asGetGlobal().name << ")";
    }
    else if (expr.isSetGlobal()) {
        const auto& sg = expr.asSetGlobal();
        oss << "SetGlobal(" << sg.name << ", " << print_atom(sg.value) << ")";
    }
    else if (expr.isLambda()) {
        const auto& lambda = expr.asLambda();
        oss << "Lambda([";
        for (size_t i = 0; i < lambda.params.size(); i++) {
            if (i > 0) oss << ", ";
            oss << lambda.params[i];
        }
        oss << "], <body>)";
    }
    else if (expr.isValues()) {
        const auto& vals = expr.asValues();
        oss << "Values(";
        for (size_t i = 0; i < vals.values.size(); i++) {
            if (i > 0) oss << ", ";
            oss << print_atom(vals.values[i]);
        }
        oss << ")";
    }
    else if (expr.isCallWithValues()) {
        const auto& cwv = expr.asCallWithValues();
        oss << "CallWithValues(" << print_atom(cwv.producer) << ", " << print_atom(cwv.consumer) << ")";
    }

    return oss.str();
}

std::string indent_str(int indent) {
    return std::string(indent * 2, ' ');
}

std::string print_stmt(const Stmt& stmt, int indent) {
    std::ostringstream oss;
    std::string ind = indent_str(indent);

    if (stmt.isLet()) {
        const auto& let = stmt.asLet();
        oss << ind << "Let " << let.name << " = " << print_expr(*let.expr) << "\n";
        oss << print_stmt(*let.next, indent);
    }
    else if (stmt.isIf()) {
        const auto& if_stmt = stmt.asIf();
        oss << ind << "If " << print_atom(if_stmt.cond) << "\n";
        oss << ind << "Then:\n";
        oss << print_stmt(*if_stmt.then_branch, indent + 1);
        oss << ind << "Else:\n";
        oss << print_stmt(*if_stmt.else_branch, indent + 1);
    }
    else if (stmt.isReturn()) {
        oss << ind << "Return " << print_atom(stmt.asReturn().value) << "\n";
    }

    return oss.str();
}

std::string print_function(const CoreIRFunction& func) {
    std::ostringstream oss;
    oss << "Function: " << func.name << " [";
    for (size_t i = 0; i < func.params.size(); i++) {
        if (i > 0) oss << ", ";
        oss << func.params[i];
    }
    oss << "]\n";
    oss << print_stmt(*func.body, 1);
    return oss.str();
}

} // namespace coreir
