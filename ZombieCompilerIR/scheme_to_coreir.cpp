#include "pch.h"
#include "scheme_to_coreir.h"
#include "cons_cell.h"
#include "Base.h"
#include "atom.h"
#include "constants.h"
#include "numeric_runtime.h"
#include "expander.h"
#include <stdexcept>
#include <vector>

namespace coreir {

// ============================================================================
// Constructor
// ============================================================================

SchemeToCoreirLowering::SchemeToCoreirLowering(Environment* env)
    : env_(env)
    , expander_(env ? new Expander(env) : nullptr)
    , temp_counter_(0) {}

// ============================================================================
// Temporary Variable Generation
// ============================================================================

std::string SchemeToCoreirLowering::gen_temp() {
    return "$tmp" + std::to_string(temp_counter_++);
}

// ============================================================================
// Macro Expansion
// ============================================================================

Object* SchemeToCoreirLowering::expand(Object* expr) {
    if (!expander_) {
        // No expander - return original expression
        return expr;
    }

    // Expand macros and desugar (let*, letrec, case, do, quasiquote, etc.)
    return expander_->expand(expr);
}

// ============================================================================
// Atomicity Check
// ============================================================================

bool SchemeToCoreirLowering::is_atomic(Object* expr) {
    if (!expr) return true;
    if (expr == nil || expr == empty_list) return true;
    if (expr == t || expr == false_obj) return true;

    // Numbers are atomic
    if (expr->get_type() == static_cast<int>(Type::INT) || expr->get_type() == static_cast<int>(Type::LONG) ||
        expr->get_type() == static_cast<int>(Type::DOUBLE) || expr->get_type() == static_cast<int>(Type::RATIONAL)) {
        return true;
    }

    // Strings and characters are atomic
    if (expr->get_type() == static_cast<int>(Type::STRING) || expr->get_type() == static_cast<int>(Type::CHAR)) {
        return true;
    }

    // Symbols (variables) are atomic
    if (expr->get_type() == static_cast<int>(Type::SYMBOL)) {
        return true;
    }

    // Quoted values are atomic
    if (expr->pair()) {
        Object* first = car(expr);
        if (first && first->get_type() == static_cast<int>(Type::SYMBOL)) {
            std::string sym_name = ((a_symbol*)first)->getName();
            if (sym_name == "quote") {
                return true;
            }
        }
    }

    return false;
}

// ============================================================================
// Symbol Name Extraction
// ============================================================================

std::string SchemeToCoreirLowering::get_symbol_name(Object* sym) {
    if (sym->get_type() == static_cast<int>(Type::SYMBOL)) {
        return ((a_symbol*)sym)->getName();
    } else if (sym->get_type() == static_cast<int>(Type::STRING)) {
        return ((a_string*)sym)->value();
    }
    throw std::runtime_error("Expected symbol or string");
}

// ============================================================================
// Primitive Operation Detection
// ============================================================================

bool SchemeToCoreirLowering::is_primitive_op(const std::string& name) {
    // List of Primitive operations that map to CoreIR Prim nodes
    // Include all operations that have JIT/AOT compiler support
    static const std::vector<std::string> primitives = {
        // Arithmetic
        "+", "-", "*", "/",
        "modulo", "quotient", "remainder",
        // Comparison
        "=", "<", ">", "<=", ">=",
        "eq?", "eqv?", "equal?",
        // Boolean
        "not",
        // List operations
        "cons", "car", "cdr", "length", "list-ref",
        // Predicates
        "null?", "pair?", "list?",
        "number?", "string?", "symbol?", "boolean?", "Procedure?"
    };

    for (const auto& prim : primitives) {
        if (name == prim) return true;
    }
    return false;
}

// ============================================================================
// Type Inference
// ============================================================================

TypeHint SchemeToCoreirLowering::infer_type(Object* obj) {
    if (!obj) return TypeHint::UNKNOWN;

    // Boolean values
    if (obj == t) return TypeHint::BOOL;
    if (obj == false_obj) return TypeHint::BOOL;

    // Numbers
    if (obj->get_type() == static_cast<int>(Type::INT) || obj->get_type() == static_cast<int>(Type::LONG)) {
        return TypeHint::INT;
    }
    if (obj->get_type() == static_cast<int>(Type::DOUBLE) || obj->get_type() == static_cast<int>(Type::RATIONAL)) {
        return TypeHint::NUM;
    }

    // Strings
    if (obj->get_type() == static_cast<int>(Type::STRING)) {
        return TypeHint::STRING;
    }

    // Symbols
    if (obj->get_type() == static_cast<int>(Type::SYMBOL)) {
        return TypeHint::SYMBOL;
    }

    // Lists (cons cells or nil)
    if (obj == nil || obj == empty_list) {
        return TypeHint::LIST;
    }
    if (obj->pair()) {
        return TypeHint::LIST;
    }

    // Vectors
    if (obj->get_type() == static_cast<int>(Type::VECTOR)) {
        return TypeHint::VECTOR;
    }

    // Procedures/closures - check instance_of() method instead of get_type()
    // Note: We don't check for Procedure types here because Object->get_type()
    // doesn't return CLOSURE or PRIMITIVE directly. For procedures, we return UNKNOWN.
    // Future: Add proper Procedure type checking via instance_of()

    // Default: unknown
    return TypeHint::UNKNOWN;
}

// ============================================================================
// Lower to Atom (with ANF Normalization)
// ============================================================================

Atom SchemeToCoreirLowering::lower_to_atom(Object* scheme_expr,
                                            std::unique_ptr<Stmt>& Continuation) {
    // If already atomic, return directly
    if (is_atomic(scheme_expr)) {
        // Literal values
        if (scheme_expr == nil || scheme_expr == empty_list ||
            scheme_expr == t || scheme_expr == false_obj ||
            scheme_expr->get_type() == static_cast<int>(Type::INT) || scheme_expr->get_type() == static_cast<int>(Type::LONG) ||
            scheme_expr->get_type() == static_cast<int>(Type::DOUBLE) || scheme_expr->get_type() == static_cast<int>(Type::STRING) ||
            scheme_expr->get_type() == static_cast<int>(Type::CHAR) || scheme_expr->get_type() == static_cast<int>(Type::RATIONAL)) {
            // Infer type and create literal with type hint
            TypeHint hint = infer_type(scheme_expr);
            return Atom::makeLit(scheme_expr, hint);
        }

        // Symbol (variable reference) - no type info for variables yet
        if (scheme_expr->get_type() == static_cast<int>(Type::SYMBOL)) {
            return Atom::makeVar(get_symbol_name(scheme_expr), TypeHint::UNKNOWN);
        }

        // Quoted value
        if (scheme_expr->pair()) {
            Object* first = car(scheme_expr);
            if (first && first->get_type() == static_cast<int>(Type::SYMBOL) &&
                get_symbol_name(first) == "quote") {
                Object* quoted = car(cdr(scheme_expr));
                TypeHint hint = infer_type(quoted);
                return Atom::makeLit(quoted, hint);
            }
        }
    }

    // Complex expression: need to normalize
    // Generate temporary variable
    std::string temp = gen_temp();

    // Lower the expression to an Expr
    auto expr = lower_expr_to_expr(scheme_expr);

    // Wrap in Let binding
    auto let_stmt = Stmt::makeLet(temp, std::move(expr), std::move(Continuation));
    Continuation = std::move(let_stmt);

    // Return variable (type unknown - will be inferred from expr)
    return Atom::makeVar(temp, TypeHint::UNKNOWN);
}

// ============================================================================
// Lower Expression to CoreIR Expr
// ============================================================================

std::unique_ptr<Expr> SchemeToCoreirLowering::lower_expr_to_expr(Object* scheme_expr) {
    // Handle atomic expressions
    if (is_atomic(scheme_expr)) {
        // For atomic values, we can't directly create an Expr
        // Wrap in a no-op Primitive (+ x 0)
        std::vector<Atom> args;

        if (scheme_expr->get_type() == static_cast<int>(Type::SYMBOL)) {
            args.push_back(Atom::makeVar(get_symbol_name(scheme_expr)));
        } else {
            args.push_back(Atom::makeLit(scheme_expr));
        }
        args.push_back(Atom::makeLit(numeric::make_integer(0)));

        return std::make_unique<Expr>(Expr::makePrim("+", std::move(args)));
    }

    // Handle list expressions
    if (!scheme_expr->pair()) {
        throw std::runtime_error("Expected list expression");
    }

    Object* first = car(scheme_expr);
    Object* rest = cdr(scheme_expr);

    // Check for special forms
    if (first && first->get_type() == static_cast<int>(Type::SYMBOL)) {
        std::string op = get_symbol_name(first);

        // Lambda
        if (op == "lambda") {
            Object* params = car(rest);
            Object* body = cdr(rest);
            return lower_lambda(params, body);
        }

        // Primitive operation
        if (is_primitive_op(op)) {
            return lower_primitive_call(op, rest);
        }
    }

    // General function call
    return lower_call(first, rest);
}

// ============================================================================
// Lower Primitive Call
// ============================================================================

std::unique_ptr<Expr> SchemeToCoreirLowering::lower_primitive_call(const std::string& op,
                                                                     Object* args) {
    // Normalize all arguments to atoms
    std::vector<Atom> atom_args;

    Object* current = args;
    while (current && current != nil && current != empty_list) {
        Object* arg = car(current);

        // Convert argument to atom
        if (is_atomic(arg)) {
            if (arg->get_type() == static_cast<int>(Type::SYMBOL)) {
                atom_args.push_back(Atom::makeVar(get_symbol_name(arg)));
            } else {
                atom_args.push_back(Atom::makeLit(arg));
            }
        } else {
            // Non-atomic argument: this will be handled by caller
            // For now in Expr context, we can't create Let bindings
            // This should be called from a context that can handle it
            throw std::runtime_error("Primitive call with complex arguments must be normalized by caller");
        }

        current = cdr(current);
    }

    return std::make_unique<Expr>(Expr::makePrim(op, std::move(atom_args)));
}

// ============================================================================
// Lower Primitive Call with Complex Arguments (Statement Context)
// ============================================================================

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_primitive_call_stmt(const std::string& op,
                                                                          Object* args) {
    // Step 1: Collect all arguments and normalize complex ones
    std::vector<Atom> atom_args;
    std::vector<std::unique_ptr<Stmt>> arg_stmt_chains;

    Object* current = args;
    while (current && current != nil && current != empty_list) {
        Object* arg = car(current);

        if (is_atomic(arg)) {
            // Atomic argument: use directly
            if (arg->get_type() == static_cast<int>(Type::SYMBOL)) {
                atom_args.push_back(Atom::makeVar(get_symbol_name(arg)));
            } else {
                atom_args.push_back(Atom::makeLit(arg));
            }
        } else {
            // Complex argument: lower to full Stmt chain
            auto arg_stmt = lower_expr(arg);

            // Extract the variable that holds the result
            // The result is always in a Return statement at the end
            std::string result_var = extract_result_var(arg_stmt.get());

            atom_args.push_back(Atom::makeVar(result_var));
            arg_stmt_chains.push_back(std::move(arg_stmt));
        }

        current = cdr(current);
    }

    // Step 2: Build final Primitive call
    std::string result_temp = gen_temp();
    auto prim_call = std::make_unique<Expr>(Expr::makePrim(op, std::move(atom_args)));
    auto final_stmt = Stmt::makeReturn(Atom::makeVar(result_temp));
    auto result_let = Stmt::makeLet(result_temp, std::move(prim_call), std::move(final_stmt));

    // Step 3: Chain all argument statements before the final Primitive call
    std::unique_ptr<Stmt> stmt = std::move(result_let);
    for (auto it = arg_stmt_chains.rbegin(); it != arg_stmt_chains.rend(); ++it) {
        stmt = inline_stmt_chain(std::move(*it), std::move(stmt));
    }

    return stmt;
}

// ============================================================================
// Lower Lambda
// ============================================================================

std::unique_ptr<Expr> SchemeToCoreirLowering::lower_lambda(Object* params, Object* body) {
    // Extract parameter names
    std::vector<std::string> param_names;
    Object* current = params;

    while (current && current != nil && current != empty_list) {
        Object* param = car(current);
        if (param->get_type() != static_cast<int>(Type::SYMBOL)) {
            throw std::runtime_error("Lambda parameter must be symbol");
        }
        param_names.push_back(get_symbol_name(param));
        current = cdr(current);
    }

    // Lower body
    auto body_stmt = lower_lambda_body(body);

    return std::make_unique<Expr>(
        Expr::makeLambda(std::move(param_names), std::move(body_stmt))
    );
}

// ============================================================================
// Lower Lambda Body
// ============================================================================

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_lambda_body(Object* body) {
    // Handle empty body
    if (!body || body == nil || body == empty_list) {
        return Stmt::makeReturn(Atom::makeLit(nil, TypeHint::LIST));
    }

    Object* first_expr = car(body);
    Object* rest = cdr(body);

    // If only one expression, return it
    if (!rest || rest == nil || rest == empty_list) {
        return lower_expr(first_expr);
    }

    // Multiple expressions: sequence them with Let bindings
    // Evaluate first expression to a temp, then continue with rest
    std::string temp = gen_temp();
    auto rest_stmt = lower_lambda_body(rest);

    if (is_atomic(first_expr)) {
        // Atomic expression: just evaluate for side effects, continue
        auto first_expr_node = lower_expr_to_expr(first_expr);
        return Stmt::makeLet(temp, std::move(first_expr_node), std::move(rest_stmt));
    } else {
        // Complex expression: lower to full Stmt chain
        auto first_stmts = lower_expr(first_expr);

        // Inline the complex expression's statements before the rest
        return inline_stmt_chain(std::move(first_stmts), std::move(rest_stmt));
    }
}

// ============================================================================
// Lower Function Call
// ============================================================================

std::unique_ptr<Expr> SchemeToCoreirLowering::lower_call(Object* func, Object* args) {
    // Note: This function returns an Expr, so it can't create Let bindings
    // The caller must handle normalization if needed

    // Normalize function to atom
    Atom func_atom;
    if (is_atomic(func)) {
        if (func->get_type() == static_cast<int>(Type::SYMBOL)) {
            func_atom = Atom::makeVar(get_symbol_name(func));
        } else {
            func_atom = Atom::makeLit(func);
        }
    } else {
        throw std::runtime_error("Complex function expressions must be normalized by caller");
    }

    // Normalize arguments
    std::vector<Atom> atom_args;
    Object* current = args;

    while (current && current != nil && current != empty_list) {
        Object* arg = car(current);

        if (is_atomic(arg)) {
            if (arg->get_type() == static_cast<int>(Type::SYMBOL)) {
                atom_args.push_back(Atom::makeVar(get_symbol_name(arg)));
            } else {
                atom_args.push_back(Atom::makeLit(arg));
            }
        } else {
            throw std::runtime_error("Non-atomic call arguments must be normalized by caller");
        }

        current = cdr(current);
    }

    return std::make_unique<Expr>(
        Expr::makeCall(std::move(func_atom), std::move(atom_args))
    );
}

// ============================================================================
// Lower If Expression
// ============================================================================

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_if(Object* cond,
                                                         Object* then_expr,
                                                         Object* else_expr) {
    // Normalize condition to atom
    Atom cond_atom;
    std::unique_ptr<Stmt> cond_stmts = nullptr;

    if (is_atomic(cond)) {
        if (cond->get_type() == static_cast<int>(Type::SYMBOL)) {
            cond_atom = Atom::makeVar(get_symbol_name(cond));
        } else {
            cond_atom = Atom::makeLit(cond);
        }
    } else {
        // Complex condition: lower to statement chain and extract result
        cond_stmts = lower_expr(cond);
        std::string result_var = extract_result_var(cond_stmts.get());
        cond_atom = Atom::makeVar(result_var);
    }

    // Lower then and else branches
    auto then_stmt = lower_expr(then_expr);
    auto else_stmt = lower_expr(else_expr);

    // Create the If statement
    auto if_stmt = Stmt::makeIf(std::move(cond_atom),
                                 std::move(then_stmt),
                                 std::move(else_stmt));

    // If we had a complex condition, inline its statement chain before the If
    if (cond_stmts) {
        return inline_stmt_chain(std::move(cond_stmts), std::move(if_stmt));
    }

    return if_stmt;
}

// ============================================================================
// Lower Let Expression
// ============================================================================

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_let(Object* bindings, Object* body) {
    // Let bindings: ((var1 val1) (var2 val2) ...)
    // Lower each binding and chain them together

    // Base case: no more bindings, evaluate body
    if (!bindings || bindings == nil || bindings == empty_list) {
        return lower_lambda_body(body);
    }

    // Process first binding
    Object* first_binding = car(bindings);
    Object* rest_bindings = cdr(bindings);

    // Extract var and value from binding
    Object* var = car(first_binding);
    Object* val_expr = car(cdr(first_binding));

    if (var->get_type() != static_cast<int>(Type::SYMBOL)) {
        throw std::runtime_error("Let binding variable must be symbol");
    }

    std::string var_name = get_symbol_name(var);

    // Lower the value expression
    // Use lower_expr_to_expr which handles lambdas, primitives, and calls
    // This is simpler and matches how letrec works
    auto val_expr_node = lower_expr_to_expr(val_expr);
    auto rest_stmt = lower_let(rest_bindings, body);
    return Stmt::makeLet(var_name, std::move(val_expr_node), std::move(rest_stmt));
}

// ============================================================================
// Lower begin - Sequence of expressions
// ============================================================================

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_begin(Object* exprs) {
    // (begin expr1 expr2 ... exprN)
    // Execute each expression in sequence, return result of last one
    // Only the last expression is in tail position

    if (is_nil(exprs)) {
        // Empty begin - return nil
        TypeHint hint = infer_type(nil);
        return Stmt::makeReturn(Atom::makeLit(nil, hint));
    }

    Object* first = car(exprs);
    Object* rest = cdr(exprs);

    if (is_nil(rest)) {
        // Single expression - just lower it (in tail position)
        return lower_expr(first);
    }

    // Multiple expressions - execute first for side effects, then continue
    auto first_stmt = lower_expr(first);
    auto rest_stmt = lower_begin(rest);

    // Inline first_stmt before rest_stmt
    return inline_stmt_chain(std::move(first_stmt), std::move(rest_stmt));
}

// ============================================================================
// Lower cond - Multi-way conditional
// ============================================================================

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_cond(Object* clauses) {
    // (cond (test1 expr1...) (test2 expr2...) ... (else exprN...))
    // Desugar to nested ifs

    if (is_nil(clauses)) {
        // No clauses - return nil (undefined behavior in R5RS, but we need something)
        TypeHint hint = infer_type(nil);
        return Stmt::makeReturn(Atom::makeLit(nil, hint));
    }

    Object* first_clause = car(clauses);
    Object* rest_clauses = cdr(clauses);

    if (!first_clause->pair()) {
        throw std::runtime_error("cond clause must be a list");
    }

    Object* test = car(first_clause);
    Object* actions = cdr(first_clause);

    // Check for else clause
    if (test->get_type() == static_cast<int>(Type::SYMBOL) && get_symbol_name(test) == "else") {
        // Else clause - execute actions as begin
        if (is_nil(actions)) {
            TypeHint hint = infer_type(t);
            return Stmt::makeReturn(Atom::makeLit(t, hint));
        } else if (is_nil(cdr(actions))) {
            // Single action
            return lower_expr(car(actions));
        } else {
            // Multiple actions - wrap in begin
            return lower_begin(actions);
        }
    }

    // Regular clause: (test => expr)
    // Lower test condition
    auto test_stmt = lower_expr(test);

    // Lower actions (multiple expressions = begin)
    std::unique_ptr<Stmt> then_branch;
    if (is_nil(actions)) {
        // No actions - return the test result
        std::string result_var = extract_result_var(test_stmt.get());
        then_branch = Stmt::makeReturn(Atom::makeVar(result_var));
    } else if (is_nil(cdr(actions))) {
        // Single action
        then_branch = lower_expr(car(actions));
    } else {
        // Multiple actions - wrap in begin
        then_branch = lower_begin(actions);
    }

    // Lower rest of clauses (else branch)
    auto else_branch = lower_cond(rest_clauses);

    // Extract test result atom from test_stmt
    std::string test_result = extract_result_var(test_stmt.get());
    Atom cond_atom = Atom::makeVar(test_result);

    // Build if statement
    auto if_stmt = Stmt::makeIf(cond_atom, std::move(then_branch), std::move(else_branch));

    // Chain test evaluation before if
    return inline_stmt_chain(std::move(test_stmt), std::move(if_stmt));
}

// ============================================================================
// Lower let* - Sequential let bindings
// ============================================================================

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_let_star(Object* bindings, Object* body) {
    // (let* ((var1 val1) (var2 val2) ...) body...)
    // Desugar to nested lets: (let ((var1 val1)) (let ((var2 val2)) ... body...))

    if (is_nil(bindings)) {
        // No bindings - just evaluate body
        return lower_lambda_body(body);
    }

    Object* first_binding = car(bindings);
    Object* rest_bindings = cdr(bindings);

    if (!first_binding->pair()) {
        throw std::runtime_error("let* binding must be a list");
    }

    Object* var = car(first_binding);
    Object* val_expr = car(cdr(first_binding));

    if (var->get_type() != static_cast<int>(Type::SYMBOL)) {
        throw std::runtime_error("let* variable must be a symbol");
    }

    std::string var_name = get_symbol_name(var);

    // Recursively lower rest of bindings and body
    auto rest_stmt = lower_let_star(rest_bindings, body);

    // Lower the value expression
    if (is_atomic(val_expr)) {
        // Atomic value - can use lower_expr_to_expr directly
        auto val_expr_node = lower_expr_to_expr(val_expr);
        return Stmt::makeLet(var_name, std::move(val_expr_node), std::move(rest_stmt));
    } else {
        // Complex expression - lower to statement chain
        auto val_stmts = lower_expr(val_expr);

        // Extract result variable (this works because lower_expr returns Return <var>  for non-atomic)
        std::string result_var = extract_result_var(val_stmts.get());

        // Create binding that copies the result
        auto copy_expr = std::make_unique<Expr>(
            Expr::makePrim("+", {Atom::makeVar(result_var), Atom::makeLit(numeric::make_integer(0))})
        );
        auto binding = Stmt::makeLet(var_name, std::move(copy_expr), std::move(rest_stmt));

        // Inline the value computation before the binding
        return inline_stmt_chain(std::move(val_stmts), std::move(binding));
    }
}

// ============================================================================
// Lower letrec - Mutually recursive let bindings
// ============================================================================

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_letrec(Object* bindings, Object* body) {
    // (letrec ((var1 val1) (var2 val2) ...) body...)
    // Transform to:
    //   (let ((var1 nil) (var2 nil) ...)
    //     (set! var1 val1)
    //     (set! var2 val2)
    //     ...
    //     body...)

    if (is_nil(bindings)) {
        // No bindings - just evaluate body
        return lower_lambda_body(body);
    }

    // Pass 1: Create Let bindings with nil initial values
    std::vector<std::pair<std::string, Object*>> binding_list;

    for (Object* b = bindings; !is_nil(b); b = cdr(b)) {
        Object* binding = car(b);
        if (!binding->pair()) {
            throw std::runtime_error("letrec binding must be a list");
        }

        Object* var = car(binding);
        Object* val_expr = car(cdr(binding));

        if (var->get_type() != static_cast<int>(Type::SYMBOL)) {
            throw std::runtime_error("letrec variable must be a symbol");
        }

        std::string var_name = get_symbol_name(var);
        binding_list.push_back({var_name, val_expr});
    }

    // Pass 2: Build the body with set! statements followed by actual body
    auto final_body = lower_lambda_body(body);

    // Add set! statements in reverse order so we can chain them
    for (auto it = binding_list.rbegin(); it != binding_list.rend(); ++it) {
        const std::string& var_name = it->first;
        Object* val_expr = it->second;

        // Create a temporary to hold the value
        std::string val_temp = gen_temp();

        // Create set! expression
        auto set_expr = std::make_unique<Expr>(Expr::makeSetLocal(var_name, Atom::makeVar(val_temp)));
        std::string set_temp = gen_temp();

        // Lower the value expression
        if (is_atomic(val_expr)) {
            // Atomic value (unlikely in letrec, but handle it)
            auto val_lowered_expr = lower_expr_to_expr(val_expr);
            final_body = Stmt::makeLet(set_temp, std::move(set_expr), std::move(final_body));
            final_body = Stmt::makeLet(val_temp, std::move(val_lowered_expr), std::move(final_body));
        } else {
            // Complex expression (typically a lambda)
            // Use lower_expr_to_expr for now since lambdas are handled correctly
            // The lambda body fix above will handle complex expressions inside the lambda
            auto val_lowered_expr = lower_expr_to_expr(val_expr);
            final_body = Stmt::makeLet(set_temp, std::move(set_expr), std::move(final_body));
            final_body = Stmt::makeLet(val_temp, std::move(val_lowered_expr), std::move(final_body));
        }
    }

    // Pass 3: Wrap in Let bindings with nil initial values (in reverse order)
    for (auto it = binding_list.rbegin(); it != binding_list.rend(); ++it) {
        const std::string& var_name = it->first;
        TypeHint hint = infer_type(nil);
        auto nil_expr = std::make_unique<Expr>(Expr::makePrim("quote", {Atom::makeLit(nil, hint)}));
        final_body = Stmt::makeLet(var_name, std::move(nil_expr), std::move(final_body));
    }

    return final_body;
}

// ============================================================================
// Lower set! - Variable mutation
// ============================================================================

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_set(Object* var, Object* value) {
    // (set! var value)
    // For now, only handle global variables (SetGlobal)
    // TODO: Add SetLocal for local variables once we track lexical scope

    if (var->get_type() != static_cast<int>(Type::SYMBOL)) {
        throw std::runtime_error("set! variable must be a symbol");
    }

    std::string var_name = get_symbol_name(var);

    // Lower the value expression
    auto value_stmts = lower_expr(value);

    // Extract value atom from result
    Atom value_atom;
    std::unique_ptr<Stmt> prep_stmt = nullptr;

    if (value_stmts->isReturn()) {
        auto& ret = value_stmts->asReturn();
        value_atom = ret.value;
    } else {
        // Complex expression - need to extract result
        std::string result_var = extract_result_var(value_stmts.get());
        value_atom = Atom::makeVar(result_var);
        prep_stmt = std::move(value_stmts);
    }

    // Create SetGlobal expression
    auto set_global = std::make_unique<Expr>(
        Expr::makeSetGlobal(var_name, value_atom)
    );

    // Bind to temp and return it (set! returns unspecified, we return nil-like result)
    std::string temp = gen_temp();
    auto ret = Stmt::makeReturn(Atom::makeVar(temp));
    auto set_stmt = Stmt::makeLet(temp, std::move(set_global), std::move(ret));

    // Chain value evaluation before the set (if needed)
    if (prep_stmt) {
        return inline_stmt_chain(std::move(prep_stmt), std::move(set_stmt));
    } else {
        return set_stmt;
    }
}

// ============================================================================
// Lower values - Multiple return values
// ============================================================================

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_values(Object* args) {
    // (values arg1 arg2 ...)
    // Normalize all arguments to atoms

    std::vector<Atom> atom_args;
    std::vector<std::unique_ptr<Stmt>> arg_stmt_chains;

    Object* current = args;
    while (current && current != nil && current != empty_list) {
        Object* arg = car(current);

        if (is_atomic(arg)) {
            // Atomic argument: use directly
            if (arg->get_type() == static_cast<int>(Type::SYMBOL)) {
                atom_args.push_back(Atom::makeVar(get_symbol_name(arg)));
            } else {
                TypeHint hint = infer_type(arg);
                atom_args.push_back(Atom::makeLit(arg, hint));
            }
        } else {
            // Complex argument: lower to full Stmt chain
            auto arg_stmt = lower_expr(arg);

            // Extract the variable that holds the result
            std::string result_var = extract_result_var(arg_stmt.get());

            atom_args.push_back(Atom::makeVar(result_var));
            arg_stmt_chains.push_back(std::move(arg_stmt));
        }

        current = cdr(current);
    }

    // Build Values expression
    std::string result_temp = gen_temp();
    auto values_expr = std::make_unique<Expr>(Expr::makeValues(std::move(atom_args)));
    auto final_stmt = Stmt::makeReturn(Atom::makeVar(result_temp));
    auto result_let = Stmt::makeLet(result_temp, std::move(values_expr), std::move(final_stmt));

    // Chain all argument statements before the values expression
    std::unique_ptr<Stmt> stmt = std::move(result_let);
    for (auto it = arg_stmt_chains.rbegin(); it != arg_stmt_chains.rend(); ++it) {
        stmt = inline_stmt_chain(std::move(*it), std::move(stmt));
    }

    return stmt;
}

// ============================================================================
// Lower call-with-values - Consume multiple return values
// ============================================================================

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_call_with_values(Object* producer, Object* consumer) {
    // (call-with-values producer consumer)
    // Normalize producer and consumer to atoms

    // Normalize producer
    Atom producer_atom;
    std::unique_ptr<Stmt> producer_stmts = nullptr;

    if (is_atomic(producer)) {
        if (producer->get_type() == static_cast<int>(Type::SYMBOL)) {
            producer_atom = Atom::makeVar(get_symbol_name(producer));
        } else {
            producer_atom = Atom::makeLit(producer);
        }
    } else {
        // Complex producer expression - normalize it
        producer_stmts = lower_expr(producer);
        std::string result_var = extract_result_var(producer_stmts.get());
        producer_atom = Atom::makeVar(result_var);
    }

    // Normalize consumer
    Atom consumer_atom;
    std::unique_ptr<Stmt> consumer_stmts = nullptr;

    if (is_atomic(consumer)) {
        if (consumer->get_type() == static_cast<int>(Type::SYMBOL)) {
            consumer_atom = Atom::makeVar(get_symbol_name(consumer));
        } else {
            consumer_atom = Atom::makeLit(consumer);
        }
    } else {
        // Complex consumer expression - normalize it
        consumer_stmts = lower_expr(consumer);
        std::string result_var = extract_result_var(consumer_stmts.get());
        consumer_atom = Atom::makeVar(result_var);
    }

    // Build CallWithValues expression
    std::string result_temp = gen_temp();
    auto cwv_expr = std::make_unique<Expr>(
        Expr::makeCallWithValues(std::move(producer_atom), std::move(consumer_atom))
    );
    auto final_stmt = Stmt::makeReturn(Atom::makeVar(result_temp));
    auto result_let = Stmt::makeLet(result_temp, std::move(cwv_expr), std::move(final_stmt));

    // Chain producer and consumer statements if present
    std::unique_ptr<Stmt> stmt = std::move(result_let);

    if (consumer_stmts) {
        stmt = inline_stmt_chain(std::move(consumer_stmts), std::move(stmt));
    }

    if (producer_stmts) {
        stmt = inline_stmt_chain(std::move(producer_stmts), std::move(stmt));
    }

    return stmt;
}

// ============================================================================
// Lower Expression to Statement
// ============================================================================

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_expr(Object* scheme_expr) {
    // STEP 1: Expand macros and desugar
    // This handles let*, letrec, case, do, quasiquote, and any user-defined macros
    scheme_expr = expand(scheme_expr);

    // Handle atomic expressions - just return them
    if (is_atomic(scheme_expr)) {
        if (scheme_expr->get_type() == static_cast<int>(Type::SYMBOL)) {
            return Stmt::makeReturn(Atom::makeVar(get_symbol_name(scheme_expr), TypeHint::UNKNOWN));
        } else {
            TypeHint hint = infer_type(scheme_expr);
            return Stmt::makeReturn(Atom::makeLit(scheme_expr, hint));
        }
    }

    // Handle list expressions
    if (!scheme_expr->pair()) {
        throw std::runtime_error("Expected list expression");
    }

    Object* first = car(scheme_expr);
    Object* rest = cdr(scheme_expr);

    // Check for special forms
    if (first && first->get_type() == static_cast<int>(Type::SYMBOL)) {
        std::string op = get_symbol_name(first);

        // If
        if (op == "if") {
            Object* cond = car(rest);
            Object* then_expr = car(cdr(rest));
            Object* else_expr = car(cdr(cdr(rest)));
            return lower_if(cond, then_expr, else_expr);
        }

        // Let
        if (op == "let") {
            Object* bindings = car(rest);
            Object* body = cdr(rest);
            return lower_let(bindings, body);
        }

        // Let* (sequential bindings)
        if (op == "let*") {
            Object* bindings = car(rest);
            Object* body = cdr(rest);
            return lower_let_star(bindings, body);
        }

        // Letrec (mutually recursive bindings)
        if (op == "letrec") {
            Object* bindings = car(rest);
            Object* body = cdr(rest);
            return lower_letrec(bindings, body);
        }

        // Begin (sequence)
        if (op == "begin") {
            return lower_begin(rest);
        }

        // Cond (multi-way conditional)
        if (op == "cond") {
            return lower_cond(rest);
        }

        // And/Or are desugared by the expander before reaching CoreIR
        // If we see them here, something went wrong
        if (op == "and" || op == "or") {
            throw std::runtime_error("and/or should be desugared by expander before CoreIR lowering");
        }

        // Set! (variable mutation)
        if (op == "set!") {
            Object* var = car(rest);
            Object* value = car(cdr(rest));
            return lower_set(var, value);
        }

        // Values (multiple return values)
        if (op == "values") {
            return lower_values(rest);
        }

        // Call-with-values (consume multiple return values)
        if (op == "call-with-values") {
            Object* producer = car(rest);
            Object* consumer = car(cdr(rest));
            return lower_call_with_values(producer, consumer);
        }

        // Lambda
        if (op == "lambda") {
            Object* params = car(rest);
            Object* body = cdr(rest);
            auto lambda_expr = lower_lambda(params, body);
            std::string temp = gen_temp();
            auto ret = Stmt::makeReturn(Atom::makeVar(temp));
            return Stmt::makeLet(temp, std::move(lambda_expr), std::move(ret));
        }

        // Define
        if (op == "define") {
            // Two forms:
            // 1. (define var value) - simple variable definition
            // 2. (define (name params...) body...) - function definition (syntactic sugar)

            Object* first_arg = car(rest);

            if (first_arg->get_type() == static_cast<int>(Type::SYMBOL)) {
                // Simple variable definition: (define var value)
                std::string var_name = get_symbol_name(first_arg);
                Object* value_expr = car(cdr(rest));

                // Lower the value expression
                auto value_stmts = lower_expr(value_expr);

                // Check if value_stmts is just "Return Lit(val)"
                // If so, we need to wrap it in a Let
                Atom value_atom;
                std::unique_ptr<Stmt> prep_stmt = nullptr;

                if (value_stmts->isReturn()) {
                    auto& ret = value_stmts->asReturn();
                    if (ret.value.isLit()) {
                        // Atomic literal - use directly
                        value_atom = ret.value;
                    } else {
                        // Variable - use it
                        value_atom = ret.value;
                    }
                } else {
                    // Complex expression - need to extract result
                    std::string result_var = extract_result_var(value_stmts.get());
                    value_atom = Atom::makeVar(result_var);
                    prep_stmt = std::move(value_stmts);
                }

                // Create SetGlobal expression
                auto set_global = std::make_unique<Expr>(
                    Expr::makeSetGlobal(var_name, value_atom)
                );

                // Bind to temp and return it
                std::string temp = gen_temp();
                auto ret = Stmt::makeReturn(Atom::makeVar(temp));
                auto set_stmt = Stmt::makeLet(temp, std::move(set_global), std::move(ret));

                // Chain value evaluation before the set (if needed)
                if (prep_stmt) {
                    return inline_stmt_chain(std::move(prep_stmt), std::move(set_stmt));
                } else {
                    return set_stmt;
                }
            }
            else if (first_arg->pair()) {
                // Function definition: (define (name params...) body...)
                // Desugar to: (define name (lambda (params...) body...))

                Object* name_sym = car(first_arg);
                std::string func_name = get_symbol_name(name_sym);
                Object* params = cdr(first_arg);
                Object* body = cdr(rest);

                // Create lambda
                auto lambda_expr = lower_lambda(params, body);

                // Store lambda in temp
                std::string lambda_temp = gen_temp();
                auto lambda_let = Stmt::makeLet(lambda_temp, std::move(lambda_expr), nullptr);

                // Create SetGlobal for the function
                auto set_global = std::make_unique<Expr>(
                    Expr::makeSetGlobal(func_name, Atom::makeVar(lambda_temp))
                );

                std::string result_temp = gen_temp();
                auto ret = Stmt::makeReturn(Atom::makeVar(result_temp));
                auto set_stmt = Stmt::makeLet(result_temp, std::move(set_global), std::move(ret));

                // Chain: lambda creation, then set global
                lambda_let->asLet().next = std::move(set_stmt);
                return lambda_let;
            }
            else {
                throw std::runtime_error("Invalid define form");
            }
        }

        // Primitive or call: evaluate and return
        if (is_primitive_op(op)) {
            // Use statement version which handles complex arguments
            return lower_primitive_call_stmt(op, rest);
        }
    }

    // General function call - need to normalize function and arguments
    // Step 1: Normalize function if needed
    Atom func_atom;
    std::unique_ptr<Stmt> func_stmts = nullptr;

    if (is_atomic(first)) {
        if (first->get_type() == static_cast<int>(Type::SYMBOL)) {
            func_atom = Atom::makeVar(get_symbol_name(first));
        } else {
            func_atom = Atom::makeLit(first);
        }
    } else {
        // Complex function expression (like lambda) - normalize it
        func_stmts = lower_expr(first);
        std::string result_var = extract_result_var(func_stmts.get());
        func_atom = Atom::makeVar(result_var);
    }

    // Step 2: Normalize arguments if needed
    std::vector<Atom> atom_args;
    std::vector<std::unique_ptr<Stmt>> arg_stmt_chains;

    Object* current = rest;
    while (current && current != nil && current != empty_list) {
        Object* arg = car(current);

        if (is_atomic(arg)) {
            if (arg->get_type() == static_cast<int>(Type::SYMBOL)) {
                atom_args.push_back(Atom::makeVar(get_symbol_name(arg)));
            } else {
                atom_args.push_back(Atom::makeLit(arg));
            }
        } else {
            // Complex argument - normalize it
            auto arg_stmt = lower_expr(arg);
            std::string result_var = extract_result_var(arg_stmt.get());
            atom_args.push_back(Atom::makeVar(result_var));
            arg_stmt_chains.push_back(std::move(arg_stmt));
        }

        current = cdr(current);
    }

    // Step 3: Build the call expression
    auto call_expr = std::make_unique<Expr>(Expr::makeCall(std::move(func_atom), std::move(atom_args)));
    std::string temp = gen_temp();
    auto ret = Stmt::makeReturn(Atom::makeVar(temp));
    auto call_stmt = Stmt::makeLet(temp, std::move(call_expr), std::move(ret));

    // Step 4: Chain all argument statements before the call
    std::unique_ptr<Stmt> stmt = std::move(call_stmt);
    for (auto it = arg_stmt_chains.rbegin(); it != arg_stmt_chains.rend(); ++it) {
        stmt = inline_stmt_chain(std::move(*it), std::move(stmt));
    }

    // Step 5: Chain function statement if present
    if (func_stmts) {
        stmt = inline_stmt_chain(std::move(func_stmts), std::move(stmt));
    }

    return stmt;
}

// ============================================================================
// Tail Call Marking
// ============================================================================

void SchemeToCoreirLowering::mark_tail_calls(Stmt* stmt) {
    if (!stmt) return;

    if (stmt->isLet()) {
        auto& let = stmt->asLet();

        // Mark tail calls in the Continuation (recursive)
        mark_tail_calls(let.next.get());

        // Check if the let binds a call, and if the Continuation is a Return of that binding
        if (let.expr && let.expr->isCall() && let.next && let.next->isReturn()) {
            const auto& ret = let.next->asReturn();
            // If we're returning the variable we just bound, this is a tail call
            if (ret.value.isVar() && ret.value.asVar().name == let.name) {
                // Mark the call as a tail call
                auto& call = let.expr->data;
                if (std::holds_alternative<Expr::Call>(call)) {
                    std::get<Expr::Call>(call).is_tail_call = true;
                }
            }
        }

        // Check if the let binds a call-with-values, and if the Continuation is a Return of that binding
        if (let.expr && let.expr->isCallWithValues() && let.next && let.next->isReturn()) {
            const auto& ret = let.next->asReturn();
            // If we're returning the variable we just bound, this is a tail call
            if (ret.value.isVar() && ret.value.asVar().name == let.name) {
                // Mark the call-with-values as a tail call
                auto& cwv = let.expr->data;
                if (std::holds_alternative<Expr::CallWithValues>(cwv)) {
                    std::get<Expr::CallWithValues>(cwv).is_tail_call = true;
                }
            }
        }
    }
    else if (stmt->isIf()) {
        auto& if_stmt = stmt->asIf();

        // Both branches of an if in tail position are themselves in tail position
        mark_tail_calls(if_stmt.then_branch.get());
        mark_tail_calls(if_stmt.else_branch.get());
    }
    else if (stmt->isReturn()) {
        // Return statement - nothing to mark (the call would be in a preceding Let)
    }
}

// ============================================================================
// Lower Define
// ============================================================================

std::unique_ptr<CoreIRFunction> SchemeToCoreirLowering::lower_define(Object* scheme_expr) {
    // STEP 1: Expand macros and desugar
    // This handles let*, letrec, case, do, quasiquote, and any user-defined macros
    scheme_expr = expand(scheme_expr);

    // Parse define expression
    if (!scheme_expr || !scheme_expr->pair()) {
        throw std::runtime_error("Invalid define expression: not a list");
    }

    Object* rest = cdr(scheme_expr);
    if (!rest || rest == nil || rest == empty_list) {
        throw std::runtime_error("Invalid define expression: missing arguments");
    }

    Object* first_arg = car(rest);

    // Two forms:
    // 1. (define var value) - simple variable definition (NOT a function - error)
    // 2. (define (name params...) body...) - function definition (syntactic sugar)

    if (first_arg->get_type() == static_cast<int>(Type::SYMBOL)) {
        // Simple variable definition: (define var value)
        // This is NOT a function definition, so we can't return a CoreIRFunction
        throw std::runtime_error("Define variable form cannot be lowered to CoreIRFunction (use function syntax)");
    }
    else if (first_arg->pair()) {
        // Function definition: (define (name params...) body...)

        Object* name_sym = car(first_arg);
        if (name_sym->get_type() != static_cast<int>(Type::SYMBOL)) {
            throw std::runtime_error("Function name must be a symbol");
        }

        std::string func_name = get_symbol_name(name_sym);

        // Extract parameters
        std::vector<std::string> param_names;
        Object* param_list = cdr(first_arg);

        while (param_list && param_list != nil && param_list != empty_list) {
            Object* param = car(param_list);
            if (param->get_type() != static_cast<int>(Type::SYMBOL)) {
                throw std::runtime_error("Function parameter must be a symbol");
            }
            param_names.push_back(get_symbol_name(param));
            param_list = cdr(param_list);
        }

        // Extract body expressions
        Object* body = cdr(rest);
        if (!body || body == nil || body == empty_list) {
            throw std::runtime_error("Function body cannot be empty");
        }

        // Lower the body to a statement
        auto body_stmt = lower_lambda_body(body);

        // Mark tail calls in the body
        mark_tail_calls(body_stmt.get());

        // Create CoreIRFunction
        return std::make_unique<CoreIRFunction>(
            func_name,
            std::move(param_names),
            std::move(body_stmt)
        );
    }
    else {
        throw std::runtime_error("Invalid define form: first argument must be symbol or list");
    }
}

// ============================================================================
// Lower Expression to Function
// ============================================================================

std::unique_ptr<CoreIRFunction> SchemeToCoreirLowering::lower_expression_to_function(
    Object* scheme_expr,
    const std::string& func_name) {

    // Check if this is a lambda expression: (lambda (params...) body...)
    if (is_pair(scheme_expr) && is_symbol(car(scheme_expr))) {
        std::string head = get_symbol_name(car(scheme_expr));
        if (head == "lambda") {
            // Extract lambda parameters and body
            Object* params_list = car(cdr(scheme_expr));
            Object* body_list = cdr(cdr(scheme_expr));

            // Extract parameter names
            std::vector<std::string> param_names;
            Object* current = params_list;
            while (current && current != nil && current != empty_list) {
                Object* param = car(current);
                if (param->get_type() != static_cast<int>(Type::SYMBOL)) {
                    throw std::runtime_error("Lambda parameter must be symbol");
                }
                param_names.push_back(get_symbol_name(param));
                current = cdr(current);
            }

            // Lower lambda body
            auto body_stmt = lower_lambda_body(body_list);

            return std::make_unique<CoreIRFunction>(
                func_name,
                std::move(param_names),
                std::move(body_stmt)
            );
        }
    }

    // Not a lambda - create a zero-argument function that evaluates the expression
    auto body_stmt = lower_expr(scheme_expr);

    // IMPORTANT: Do NOT mark tail calls for top-level wrapper functions
    // The wrapper function is called directly by the VM and has no caller frame.
    // Tail calls would cause the frame to be replaced, leaving nowhere to return to.
    // Regular CALLs work fine - they create a new frame that returns to this one.
    // mark_tail_calls(body_stmt.get());  ← DISABLED

    // Create a zero-argument function that evaluates the expression
    std::vector<std::string> params;  // Empty parameter list

    return std::make_unique<CoreIRFunction>(
        func_name,
        std::move(params),
        std::move(body_stmt)
    );
}

// ============================================================================
// Helper Functions for Statement Chaining
// ============================================================================

std::string SchemeToCoreirLowering::extract_result_var(Stmt* stmt) {
    // Walk the Stmt chain to find the Return statement
    // The result variable is in the Return's Atom

    if (std::holds_alternative<Stmt::Return>(stmt->data)) {
        auto& ret = std::get<Stmt::Return>(stmt->data);
        if (std::holds_alternative<Atom::Var>(ret.value.data)) {
            return std::get<Atom::Var>(ret.value.data).name;
        } else {
            // Return has a literal, need to bind it to a temp
            std::string temp = gen_temp();
            return temp;
        }
    } else if (std::holds_alternative<Stmt::Let>(stmt->data)) {
        auto& let = std::get<Stmt::Let>(stmt->data);
        return extract_result_var(let.next.get());
    } else if (std::holds_alternative<Stmt::If>(stmt->data)) {
        // For If statements used as values, we need to create a result variable
        // that will be assigned in both branches
        // This is handled by converting the If to use phi-style result binding
        // For now, we generate a temp variable name that the If will write to
        std::string result_var = gen_temp();
        return result_var;
    }

    throw std::runtime_error("Unexpected statement type in extract_result_var");
}

std::unique_ptr<Stmt> SchemeToCoreirLowering::inline_stmt_chain(std::unique_ptr<Stmt> chain,
                                                                  std::unique_ptr<Stmt> Continuation) {
    // Replace the Return statement at the end of chain with Continuation
    // This effectively inlines the chain before the Continuation

    if (std::holds_alternative<Stmt::Return>(chain->data)) {
        // Found the Return - replace with Continuation
        return std::move(Continuation);
    } else if (std::holds_alternative<Stmt::Let>(chain->data)) {
        auto& let_data = std::get<Stmt::Let>(chain->data);

        // Recursively inline the next statement
        std::string name = let_data.name;
        auto expr = std::move(let_data.expr);
        auto inlined_next = inline_stmt_chain(std::move(let_data.next), std::move(Continuation));

        return Stmt::makeLet(name, std::move(expr), std::move(inlined_next));
    } else if (std::holds_alternative<Stmt::If>(chain->data)) {
        // For If, inlining is complex (would need to clone Continuation)
        // For now, throw an error - this shouldn't happen in current usage
        throw std::runtime_error("Cannot inline stmt chain with If statement (clone not implemented)");
    }

    throw std::runtime_error("Unexpected statement type in inline_stmt_chain");
}

} // namespace coreir
