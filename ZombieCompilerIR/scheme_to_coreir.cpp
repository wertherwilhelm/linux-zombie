#include "zombie_runtime_types.h"
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
        expr->get_type() == static_cast<int>(Type::DOUBLE) || expr->get_type() == static_cast<int>(Type::RATIONAL) ||
        expr->get_type() == static_cast<int>(Type::BIGNUM)) {
        return true;
    }

    // Strings and characters are atomic
    if (expr->get_type() == static_cast<int>(Type::STRING) || expr->get_type() == static_cast<int>(Type::CHAR)) {
        return true;
    }

    // Vector and bytevector literals are self-evaluating constants (R7RS),
    // so they are atomic from the lowering's point of view.
    if (expr->get_type() == static_cast<int>(Type::VECTOR) ||
        expr->get_type() == static_cast<int>(Type::BYTEVECTOR)) {
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

// Convert an atomic Scheme expr to a CoreIR Atom. Single source of truth so that
// quote-stripping ((quote X) -> datum X) is consistent across every lowering path;
// scattered copies of this logic previously disagreed (some returned a literal of the
// whole (quote X) form, yielding "Unbound variable:" / wrong results).
Atom SchemeToCoreirLowering::atomic_to_atom(Object* expr) {
    if (expr->get_type() == static_cast<int>(Type::SYMBOL)) {
        return Atom::makeVar(get_symbol_name(expr), TypeHint::UNKNOWN);
    }
    if (expr->pair()) {
        Object* head = car(expr);
        if (head && head->get_type() == static_cast<int>(Type::SYMBOL) &&
            get_symbol_name(head) == "quote") {
            Object* quoted = car(cdr(expr));
            return Atom::makeLit(quoted, infer_type(quoted));
        }
    }
    return Atom::makeLit(expr, infer_type(expr));
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
    if (obj->get_type() == static_cast<int>(Type::BIGNUM)) {
        return TypeHint::INT;  // an exact integer, just wider than a fixnum
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
            scheme_expr->get_type() == static_cast<int>(Type::CHAR) || scheme_expr->get_type() == static_cast<int>(Type::RATIONAL) ||
            scheme_expr->get_type() == static_cast<int>(Type::BIGNUM)) {
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
        // Wrap in a no-op Primitive (+ x 0) to carry the atom through as an Expr.
        std::vector<Atom> args;
        args.push_back(atomic_to_atom(scheme_expr));
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

        // Convert argument to atom (atomic_to_atom strips quote: (quote X) -> datum X;
        // previously this inlined makeLit(arg) and passed e.g. (quote ()) unstripped, so
        // (null? '()) saw a pair and (car '(1 2)) returned the symbol `quote`).
        if (is_atomic(arg)) {
            atom_args.push_back(atomic_to_atom(arg));
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

std::unique_ptr<Stmt> SchemeToCoreirLowering::splice_tail(
    std::unique_ptr<Stmt> chain,
    const std::function<std::unique_ptr<Stmt>(Atom)>& k) {
    if (std::holds_alternative<Stmt::Return>(chain->data)) {
        return k(std::get<Stmt::Return>(chain->data).value);
    }
    if (std::holds_alternative<Stmt::Let>(chain->data)) {
        auto& let = std::get<Stmt::Let>(chain->data);
        std::string name = let.name;
        auto expr = std::move(let.expr);
        auto next = splice_tail(std::move(let.next), k);
        return Stmt::makeLet(name, std::move(expr), std::move(next));
    }
    auto& iff = std::get<Stmt::If>(chain->data);
    Atom cond = iff.cond;
    auto then_b = splice_tail(std::move(iff.then_branch), k);
    auto else_b = splice_tail(std::move(iff.else_branch), k);
    return Stmt::makeIf(cond, std::move(then_b), std::move(else_b));
}

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_operands(
    Object* args, std::vector<Atom> acc,
    const std::function<std::unique_ptr<Stmt>(std::vector<Atom>)>& build) {
    if (!args || args == nil || args == empty_list) {
        return build(std::move(acc));
    }
    Object* arg = car(args);
    Object* rest_args = cdr(args);
    if (is_atomic(arg)) {
        acc.push_back(atomic_to_atom(arg));
        return lower_operands(rest_args, std::move(acc), build);
    }
    auto chain = lower_expr(arg);
    std::vector<Atom> acc_copy = acc;
    return splice_tail(std::move(chain), [this, rest_args, acc_copy, &build](Atom v) {
        std::vector<Atom> acc2 = acc_copy;
        acc2.push_back(v);
        return lower_operands(rest_args, std::move(acc2), build);
    });
}

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_primitive_call_stmt(const std::string& op,
                                                                          Object* args) {
    return lower_operands(args, {}, [this, op](std::vector<Atom> atoms) {
        std::string result_temp = gen_temp();
        auto prim_call = std::make_unique<Expr>(Expr::makePrim(op, std::move(atoms)));
        return Stmt::makeLet(result_temp, std::move(prim_call),
                             Stmt::makeReturn(Atom::makeVar(result_temp)));
    });
}

// ============================================================================
// Lower Lambda
// ============================================================================

std::unique_ptr<Expr> SchemeToCoreirLowering::lower_lambda(Object* params, Object* body) {
    // Extract parameter names, supporting variadic forms:
    //   (lambda (a b) ...)      fixed
    //   (lambda (a . rest) ...) fixed + rest
    //   (lambda args ...)       all args collected into `rest`
    std::vector<std::string> param_names;
    std::string rest_name;
    Object* current = params;

    while (current && current != nil && current != empty_list) {
        if (current->get_type() == static_cast<int>(Type::SYMBOL)) {
            // A bare symbol here is the rest parameter: either the whole param
            // list (lambda args ...) or the dotted tail (a . rest).
            rest_name = get_symbol_name(current);
            break;
        }
        if (!current->pair()) {
            break;
        }
        Object* param = car(current);
        if (param->get_type() != static_cast<int>(Type::SYMBOL)) {
            throw std::runtime_error("Lambda parameter must be symbol");
        }
        param_names.push_back(get_symbol_name(param));
        current = cdr(current);
    }

    // Lower body
    auto body_stmt = lower_lambda_body(body);

    // Mark tail calls in the body. A nested lambda is always entered through a
    // fresh call frame (OP_CALL -> Closure::apply), so a tail call there reuses
    // that frame and recurses in constant space. (The top-level __eval__ wrapper
    // in lower_expression_to_function deliberately does NOT get this: it runs as
    // the entry frame with no caller to return to.) Without this, pipeline-
    // compiled code — coreir and native — never emitted OP_TAIL_CALL and grew a
    // frame per iteration, so deep tail loops ran in O(n^2) time / space.
    mark_tail_calls(body_stmt.get());

    return std::make_unique<Expr>(
        Expr::makeLambda(std::move(param_names), std::move(body_stmt), std::move(rest_name))
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

    // Multiple expressions: evaluate the first for side effects, then continue.
    // Lower each non-final expression to a Stmt chain and splice it before the
    // rest, exactly as lower_begin does. (An earlier atomic special-case wrapped
    // the value in a no-op (+ x 0) to carry it as an Expr, but that is a type
    // error for any non-numeric atom — e.g. a non-final 'sym or "str" produced
    // "ADD: operands must be numbers". lower_expr + inline_stmt_chain handles all
    // atom kinds uniformly.)
    auto first_stmt = lower_expr(first_expr);
    auto rest_stmt = lower_lambda_body(rest);
    return inline_stmt_chain(std::move(first_stmt), std::move(rest_stmt));
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
            atom_args.push_back(atomic_to_atom(arg));  // strips quote consistently
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
        cond_atom = atomic_to_atom(cond);
    } else {
        // Complex condition: lower to statement chain and extract result
        cond_stmts = lower_expr(cond);
        std::string result_var = extract_result_var(cond_stmts.get());
        cond_atom = Atom::makeVar(result_var);
    }

    // Lower then and else branches
    auto then_stmt = lower_expr(then_expr);
    auto else_stmt = else_expr ? lower_expr(else_expr)
                               : Stmt::makeReturn(Atom::makeLit(nil, TypeHint::VOID));

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

    // Regular clause: (test expr ...)
    // Atomic tests (#t, symbols, (quote X)) become the cond atom directly; complex
    // tests lower to a statement chain whose result var is the atom. (Previously an
    // atomic test went through extract_result_var, which returned "" -> empty var.)
    Atom cond_atom;
    std::unique_ptr<Stmt> test_stmt = nullptr;
    if (is_atomic(test)) {
        cond_atom = atomic_to_atom(test);
    } else {
        test_stmt = lower_expr(test);
        cond_atom = Atom::makeVar(extract_result_var(test_stmt.get()));
    }

    // Lower actions (multiple expressions = begin)
    std::unique_ptr<Stmt> then_branch;
    if (is_nil(actions)) {
        // No actions - return the test's own value
        then_branch = Stmt::makeReturn(cond_atom);
    } else if (is_nil(cdr(actions))) {
        // Single action
        then_branch = lower_expr(car(actions));
    } else {
        // Multiple actions - wrap in begin
        then_branch = lower_begin(actions);
    }

    // Rest of clauses form the else branch
    auto else_branch = lower_cond(rest_clauses);

    auto if_stmt = Stmt::makeIf(cond_atom, std::move(then_branch), std::move(else_branch));
    if (test_stmt) {
        return inline_stmt_chain(std::move(test_stmt), std::move(if_stmt));
    }
    return if_stmt;
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
// Lower Expression to Statement
// ============================================================================

std::unique_ptr<Stmt> SchemeToCoreirLowering::lower_expr(Object* scheme_expr) {
    // STEP 1: Expand macros and desugar
    // This handles let*, letrec, case, do, quasiquote, and any user-defined macros
    scheme_expr = expand(scheme_expr);

    // Handle atomic expressions (symbol, literal, or (quote X)) - just return them
    if (is_atomic(scheme_expr)) {
        return Stmt::makeReturn(atomic_to_atom(scheme_expr));
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
            // else is optional: (if test then) has no else branch. Pass nullptr so
            // lower_if yields an unspecified (nil) value rather than dereferencing nil.
            Object* else_tail = cdr(cdr(rest));
            Object* else_expr = (else_tail && else_tail->pair()) ? car(else_tail) : nullptr;
            return lower_if(cond, then_expr, else_expr);
        }

        // Let. Named let (and every other derived form) is already desugared by
        // the Expander in expand() above — named let -> letrec, let -> lambda
        // application, cond/case/do/when/unless/let*/let-values/... -> core forms
        // — so the dedicated handling that used to live here is redundant.
        if (op == "let") {
            return lower_let(car(rest), cdr(rest));
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

        // values / call-with-values are ordinary runtime primitives; lower them
        // through the normal primitive-call path (OP_CALL) so multiple-value
        // semantics match the other evaluators. (The dedicated Values /
        // CallWithValues lowering passed the producer's values to the consumer
        // as a single packed object — "too few arguments" / wrong types — so it
        // is intentionally bypassed here.)

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

                // Execute the SetGlobal (bind to a temp to force evaluation),
                // then return the unspecified value. R7RS: the value of a define
                // is unspecified (nil/VOID here, matching the tree-walk).
                std::string temp = gen_temp();
                auto ret = Stmt::makeReturn(Atom::makeLit(nil, TypeHint::VOID));
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

                // Execute the SetGlobal side effect, then return the unspecified
                // value (R7RS; consistent with simple-define and the tree-walk).
                std::string result_temp = gen_temp();
                auto ret = Stmt::makeReturn(Atom::makeLit(nil, TypeHint::VOID));
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

    // Step 2: ANF-lower the arguments (tolerating `if` in operand position).
    std::unique_ptr<Stmt> stmt = lower_operands(rest, {},
        [this, func_atom](std::vector<Atom> atoms) {
            auto call_expr = std::make_unique<Expr>(
                Expr::makeCall(func_atom, std::move(atoms)));
            std::string temp = gen_temp();
            return Stmt::makeLet(temp, std::move(call_expr),
                                 Stmt::makeReturn(Atom::makeVar(temp)));
        });

    // Step 3: Chain the function-evaluation statements before the call.
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

Atom SchemeToCoreirLowering::terminal_atom(Stmt* stmt) {
    // Walk Lets to the terminal Return and return its atom directly. For a
    // Return(Var) the var is bound by the preceding Lets; for a Return(Lit) the
    // literal is used as-is. (The old pattern — extract_result_var + makeVar —
    // invented a fresh, UNBOUND temp for a Return(Lit), so e.g. (begin 1 999) in
    // operand position produced "Unbound variable: $tmpN".)
    if (std::holds_alternative<Stmt::Return>(stmt->data)) {
        return std::get<Stmt::Return>(stmt->data).value;
    }
    if (std::holds_alternative<Stmt::Let>(stmt->data)) {
        return terminal_atom(std::get<Stmt::Let>(stmt->data).next.get());
    }
    // A terminal If (value-position if) is not supported here — inline_stmt_chain
    // cannot splice an If either, so this is a pre-existing limitation.
    throw std::runtime_error("terminal_atom: if in operand position not supported");
}

// Deep-clone a lowered IR subtree (Atoms are value types; Expr/Stmt own unique_ptrs).
// Needed to splice a continuation into both branches of an If.
static std::unique_ptr<Stmt> clone_stmt(const Stmt& s);

static std::unique_ptr<Expr> clone_expr(const Expr& e) {
    Expr out;
    if (e.isPrim()) { auto& p = e.asPrim(); out = Expr::makePrim(p.op, p.args); }
    else if (e.isCall()) { auto& c = e.asCall(); out = Expr::makeCall(c.func, c.args, TypeHint::UNKNOWN, c.is_tail_call); }
    else if (e.isGetGlobal()) { out = Expr::makeGetGlobal(e.asGetGlobal().name); }
    else if (e.isSetGlobal()) { auto& s = e.asSetGlobal(); out = Expr::makeSetGlobal(s.name, s.value); }
    else if (e.isSetLocal()) { auto& s = e.asSetLocal(); out = Expr::makeSetLocal(s.name, s.value); }
    else if (e.isLambda()) { auto& l = e.asLambda(); out = Expr::makeLambda(l.params, clone_stmt(*l.body), l.rest); }
    else if (e.isValues()) { out = Expr::makeValues(e.asValues().values); }
    else if (e.isCallWithValues()) { auto& c = e.asCallWithValues(); out = Expr::makeCallWithValues(c.producer, c.consumer, c.is_tail_call); }
    else throw std::runtime_error("clone_expr: unknown expression kind");
    out.inferred_type = e.inferred_type;
    return std::make_unique<Expr>(std::move(out));
}

static std::unique_ptr<Stmt> clone_stmt(const Stmt& s) {
    if (std::holds_alternative<Stmt::Let>(s.data)) {
        auto& l = std::get<Stmt::Let>(s.data);
        return Stmt::makeLet(l.name, clone_expr(*l.expr), clone_stmt(*l.next));
    }
    if (std::holds_alternative<Stmt::If>(s.data)) {
        auto& i = std::get<Stmt::If>(s.data);
        return Stmt::makeIf(i.cond, clone_stmt(*i.then_branch), clone_stmt(*i.else_branch));
    }
    return Stmt::makeReturn(std::get<Stmt::Return>(s.data).value);
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
        // Both branches of the If terminate in a Return; the continuation must run
        // after whichever branch executed, so splice a CLONE into one branch and the
        // original into the other (an `if` used for its value / effect in non-tail
        // position, e.g. a non-final begin expression that is an if).
        auto& if_data = std::get<Stmt::If>(chain->data);
        Atom cond = if_data.cond;
        auto then_b = inline_stmt_chain(std::move(if_data.then_branch), clone_stmt(*Continuation));
        auto else_b = inline_stmt_chain(std::move(if_data.else_branch), std::move(Continuation));
        return Stmt::makeIf(cond, std::move(then_b), std::move(else_b));
    }

    throw std::runtime_error("Unexpected statement type in inline_stmt_chain");
}

} // namespace coreir
