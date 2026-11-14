#include "pch.h"
#include "compiler.h"
#include "Base.h"
#include "atom.h"
#include "closure.h"
#include "numeric_runtime.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include <sstream>

namespace zombie { namespace bytecode {

// ============================================================================
// Constructor
// ============================================================================

Compiler::Compiler()
    : env_(nullptr), expander_(nullptr), tail_context_(false) {
}

// ============================================================================
// Main compilation entry points
// ============================================================================

CompiledCode Compiler::compile(Object* expr, Environment* env) {
    // Reset state
    bytecode_.clear();
    constants_.clear();
    labels_.clear();
    env_ = env;
    tail_context_ = true;  // Top-level expression is in tail position

    // Create expander for Macro expansion and desugaring
    // This is created fresh for each compilation to ensure clean state
    expander_ = env ? new Expander(env) : nullptr;

    // STEP 1: Expand macros and desugar
    // This handles let*, letrec, case, do, quasiquote, and any user-defined macros
    if (expander_) {
        expr = expander_->expand(expr);
    }

    // STEP 2: Compile the expanded expression
    compile_expr(expr);

    // Clean up expander
    delete expander_;
    expander_ = nullptr;

    // Emit final HALT
    emit(OP_HALT);

    // Build result
    CompiledCode result;
    result.bytecode = bytecode_;
    result.constants = constants_;
    return result;
}

CompiledCode Compiler::compile_sequence(Object* exprs, Environment* env) {
    // Reset state
    bytecode_.clear();
    constants_.clear();
    labels_.clear();
    env_ = env;

    // Create expander for Macro expansion and desugaring
    expander_ = env ? new Expander(env) : nullptr;

    // Compile each expression, keeping only last result on stack
    Object* current = exprs;
    while (!is_nil(current)) {
        // Expand each expression before compiling
        Object* expr = first(current);
        if (expander_) {
            expr = expander_->expand(expr);
        }
        compile_expr(expr);

        // Pop result if not the last expression
        Object* next = rest(current);
        if (!is_nil(next)) {
            emit(OP_POP);
        }

        current = next;
    }

    // Clean up expander
    delete expander_;
    expander_ = nullptr;

    // Emit final HALT
    emit(OP_HALT);

    // Build result
    CompiledCode result;
    result.bytecode = bytecode_;
    result.constants = constants_;
    return result;
}

// ============================================================================
// Core compilation
// ============================================================================

void Compiler::compile_expr(Object* expr) {
    if (!expr) {
        // nullptr → nil
        uint16_t idx = add_constant(nil);
        emit(OP_LOAD_CONST);
        emit_u16(idx);
        return;
    }

    // Atom or list?
    if (!expr->pair()) {
        compile_atom(expr);
    } else {
        compile_list(expr);
    }
}

void Compiler::compile_atom(Object* atom) {
    // Handle nil and t
    if (is_nil(atom)) {
        uint16_t idx = add_constant(nil);
        emit(OP_LOAD_CONST);
        emit_u16(idx);
        return;
    }

    if (is_t(atom)) {
        uint16_t idx = add_constant(t);
        emit(OP_LOAD_CONST);
        emit_u16(idx);
        return;
    }

    // Check for symbols FIRST (they are variable lookups)
    a_symbol* sym = dynamic_cast<a_symbol*>(atom);
    if (sym) {
        uint16_t idx = add_constant(sym);
        emit(OP_LOAD_VAR);
        emit_u16(idx);
        return;
    }

    // String literals are constants, not variable names
    a_string* str = dynamic_cast<a_string*>(atom);
    if (str) {
        uint16_t idx = add_constant(str);
        emit(OP_LOAD_CONST);  // Fixed: String literals should load as constants
        emit_u16(idx);
        return;
    }

    // Numbers and other atoms are constants
    if (atom->is_atom()) {
        uint16_t idx = add_constant(atom);
        emit(OP_LOAD_CONST);
        emit_u16(idx);
        return;
    }

    // Everything else is a constant
    uint16_t idx = add_constant(atom);
    emit(OP_LOAD_CONST);
    emit_u16(idx);
}

void Compiler::compile_list(Object* list) {
    if (is_nil(list)) {
        // Empty list is nil
        uint16_t idx = add_constant(nil);
        emit(OP_LOAD_CONST);
        emit_u16(idx);
        return;
    }

    Object* first_elem = first(list);

    // Check for special forms
    std::string name = get_symbol_name(first_elem);
    if (!name.empty()) {
        if (name == "quote") {
            compile_quote(rest(list));
            return;
        }
        if (name == "if") {
            compile_if(rest(list));
            return;
        }
        if (name == "lambda") {
            compile_lambda(rest(list));
            return;
        }
        if (name == "define") {
            compile_define(rest(list));
            return;
        }
        if (name == "set!") {
            compile_set(rest(list));
            return;
        }
        if (name == "begin") {
            compile_begin(rest(list));
            return;
        }
        if (name == "and") {
            compile_and(rest(list));
            return;
        }
        if (name == "or") {
            compile_or(rest(list));
            return;
        }
        if (name == "call/cc" || name == "call-with-current-continuation") {
            compile_callcc(rest(list));
            return;
        }
        if (name == "dynamic-wind") {
            compile_dynamic_wind(rest(list));
            return;
        }
        if (name == "cond") {
            compile_cond(rest(list));
            return;
        }
        if (name == "guard") {
            compile_guard(rest(list));
            return;
        }
        if (name == "parameterize") {
            compile_parameterize(rest(list));
            return;
        }
    }

    // Not a special form - function application
    compile_application(first_elem, rest(list));
}

// ============================================================================
// Special forms
// ============================================================================

void Compiler::compile_quote(Object* args) {
    Object* value = first(args);
    uint16_t idx = add_constant(value);
    emit(OP_LOAD_CONST);
    emit_u16(idx);
}

void Compiler::compile_if(Object* args) {
    // (if test consequent alternate)
    // Test is NOT in tail position, but both branches ARE
    Object* test = first(args);
    Object* consequent = first(rest(args));
    Object* alternate = rest(rest(args));

    bool saved_tail = tail_context_;

    // Compile test (not in tail position)
    tail_context_ = false;
    compile_expr(test);

    // Create labels
    size_t else_label = make_label();
    size_t end_label = make_label();

    // Jump to else if test is false
    emit_jump(OP_JUMP_IF_FALSE, else_label);

    // Compile consequent (in tail position)
    tail_context_ = saved_tail;
    compile_expr(consequent);
    emit_jump(OP_JUMP, end_label);

    // Compile alternate (in tail position)
    bind_label(else_label);
    tail_context_ = saved_tail;
    if (!is_nil(alternate)) {
        compile_expr(first(alternate));
    } else {
        // No alternate - push nil
        uint16_t idx = add_constant(nil);
        emit(OP_LOAD_CONST);
        emit_u16(idx);
    }

    bind_label(end_label);
    tail_context_ = saved_tail;  // Restore
}

void Compiler::compile_lambda(Object* args) {
    // (lambda (params...) body...)
    Object* params = first(args);
    Object* body = rest(args);

    // Create a Closure template
    // NOTE: The Closure constructor wraps body in (begin ...), but we compile
    // the original body directly to avoid treating "begin" as a variable
    Closure* tmpl = gc_new<Closure>(params, body, env_);

    // IMPORTANT: Compile the lambda body to bytecode and store it in the Closure
    // This allows the bytecode VM to execute closures without re-compiling

    // Save current compiler state
    auto saved_bytecode = bytecode_;
    auto saved_constants = constants_;
    auto saved_labels = labels_;
    bool saved_tail = tail_context_;

    // Reset for compiling lambda body
    bytecode_.clear();
    constants_.clear();
    labels_.clear();
    tail_context_ = true;  // Lambda body is ALWAYS in tail position

    // Compile the body expressions
    // IMPORTANT: Use the ACTUAL body from the Closure (_body might be wrapped in begin)
    // For bytecode, we need the unwrapped version
    Object* body_to_compile = body;

    // If the Closure wrapped a single expression in begin, unwrap it for compilation
    if (!is_nil(body) && is_nil(rest(body))) {
        // Single expression case - the Closure sets _body = first(body)
        // We should compile that first expression directly
        body_to_compile = first(body);
        compile_expr(body_to_compile);
    } else if (!is_nil(body)) {
        // Multiple expressions - compile as sequence (last one is in tail position)
        Object* current = body;
        while (!is_nil(current)) {
            Object* next = rest(current);
            bool is_last = is_nil(next);

            // Only last expression is in tail position
            tail_context_ = is_last;

            compile_expr(first(current));

            // Pop result if not the last expression
            if (!is_last) {
                emit(OP_POP);
            }

            current = next;
        }
    } else {
        // Empty body - return nil
        uint16_t idx = add_constant(nil);
        emit(OP_LOAD_CONST);
        emit_u16(idx);
    }

    // Emit RETURN instead of HALT (we're in a function)
    emit(OP_RETURN);

    // Store compiled bytecode in the Closure
    tmpl->set_bytecode(bytecode_, constants_);

    // Restore compiler state
    bytecode_ = saved_bytecode;
    constants_ = saved_constants;
    labels_ = saved_labels;
    tail_context_ = saved_tail;

    // Add Closure to constant pool
    uint16_t idx = add_constant(tmpl);

    // Emit MAKE_CLOSURE instruction
    emit(OP_MAKE_CLOSURE);
    emit_u16(idx);
}

void Compiler::compile_define(Object* args) {
    // (define name value) or (define (name params...) body...)
    // NOTE: The value expression is NOT in tail position
    Object* name = first(args);
    Object* rest_args = rest(args);

    bool saved_tail = tail_context_;
    tail_context_ = false;  // Value is not in tail position

    // Check if this is function definition syntax: (define (name params) body)
    if (!is_nil(name) && name->pair()) {
        // Syntactic sugar: (define (f x) body) => (define f (lambda (x) body))
        Object* func_name = first(name);
        Object* params = rest(name);
        Object* body = rest_args;

        // Create lambda: (lambda params body...)
        Object* lambda_expr = cons(gc_new<a_string>("lambda"), cons(params, body));

        // Compile the lambda
        compile_expr(lambda_expr);

        // Store in environment with the function name
        uint16_t name_idx = add_constant(func_name);
        emit(OP_STORE_VAR);
        emit_u16(name_idx);
    } else {
        // Simple define: (define name value)
        Object* value = first(rest_args);

        // Compile value
        compile_expr(value);

        // Store in environment
        uint16_t name_idx = add_constant(name);
        emit(OP_STORE_VAR);
        emit_u16(name_idx);
    }

    tail_context_ = saved_tail;  // Restore
}

void Compiler::compile_set(Object* args) {
    // (set! name value)
    Object* name = first(args);
    Object* value = first(rest(args));

    // Compile value
    compile_expr(value);

    // Store in environment
    uint16_t name_idx = add_constant(name);
    emit(OP_STORE_VAR);
    emit_u16(name_idx);
}

void Compiler::compile_begin(Object* args) {
    // (begin expr1 expr2 ...)
    // Only the last expression is in tail position
    Object* current = args;
    bool saved_tail = tail_context_;

    while (!is_nil(current)) {
        Object* next = rest(current);
        bool is_last = is_nil(next);

        // Only last expression inherits tail context
        tail_context_ = is_last ? saved_tail : false;

        compile_expr(first(current));

        // Pop result if not the last expression
        if (!is_last) {
            emit(OP_POP);
        }

        current = next;
    }

    tail_context_ = saved_tail;  // Restore tail context

    // If begin is empty, return nil
    if (is_nil(args)) {
        uint16_t idx = add_constant(nil);
        emit(OP_LOAD_CONST);
        emit_u16(idx);
    }
}

void Compiler::compile_and(Object* args) {
    // (and expr1 expr2 ...)
    // Short-circuit evaluation: return first false, or last true

    if (is_nil(args)) {
        // (and) → #t
        uint16_t idx = add_constant(t);
        emit(OP_LOAD_CONST);
        emit_u16(idx);
        return;
    }

    size_t end_label = make_label();
    Object* current = args;

    while (!is_nil(current)) {
        compile_expr(first(current));

        Object* next = rest(current);
        if (!is_nil(next)) {
            // Not the last expression - check if false
            emit(OP_DUP);  // Keep value on stack
            emit_jump(OP_JUMP_IF_FALSE, end_label);
            emit(OP_POP);  // Pop the true value, continue
        }

        current = next;
    }

    bind_label(end_label);
}

void Compiler::compile_or(Object* args) {
    // (or expr1 expr2 ...)
    // Short-circuit evaluation: return first true, or last false

    if (is_nil(args)) {
        // (or) → #f
        uint16_t idx = add_constant(false_obj);
        emit(OP_LOAD_CONST);
        emit_u16(idx);
        return;
    }

    size_t end_label = make_label();
    Object* current = args;

    while (!is_nil(current)) {
        compile_expr(first(current));

        Object* next = rest(current);
        if (!is_nil(next)) {
            // Not the last expression - check if true
            emit(OP_DUP);  // Keep value on stack
            // In Scheme, only #f is false
            // If not #f, jump to end
            size_t check_label = make_label();
            emit_jump(OP_JUMP_IF_FALSE, check_label);
            emit_jump(OP_JUMP, end_label);  // Value is true, keep it
            bind_label(check_label);
            emit(OP_POP);  // Pop the false value, continue
        }

        current = next;
    }

    bind_label(end_label);
}

void Compiler::compile_callcc(Object* args) {
    // (call/cc proc)
    // (call-with-current-continuation proc)

    if (is_nil(args)) {
        throw std::runtime_error("call/cc: missing Procedure argument");
    }

    Object* proc_expr = first(args);

    // Compile the Procedure expression
    compile_expr(proc_expr);

    // Emit CALLCC instruction
    emit(OP_CALLCC);
}

void Compiler::compile_dynamic_wind(Object* args) {
    // (dynamic-wind before thunk after)
    // Stack layout after compilation: [before, thunk, after]

    if (is_nil(args)) {
        throw std::runtime_error("dynamic-wind: missing 'before' argument");
    }

    Object* before_expr = first(args);
    Object* rest_args = rest(args);

    if (is_nil(rest_args)) {
        throw std::runtime_error("dynamic-wind: missing 'thunk' argument");
    }

    Object* thunk_expr = first(rest_args);
    Object* after_args = rest(rest_args);

    if (is_nil(after_args)) {
        throw std::runtime_error("dynamic-wind: missing 'after' argument");
    }

    Object* after_expr = first(after_args);

    // Compile the three thunks (pushed onto stack in order: before, thunk, after)
    compile_expr(before_expr);
    compile_expr(thunk_expr);
    compile_expr(after_expr);

    // Emit DYNAMIC_WIND instruction
    // The instruction will pop them in reverse order: after, thunk, before
    emit(OP_DYNAMIC_WIND);
}

void Compiler::compile_cond(Object* clauses) {
    // (cond (test1 expr1...) (test2 expr2...) ... (else exprN...))
    // Desugar to nested ifs: (if test1 (begin expr1...) (if test2 (begin expr2...) ...))

    if (is_nil(clauses)) {
        // No clauses - push nil (unspecified behavior)
        uint16_t idx = add_constant(nil);
        emit(OP_LOAD_CONST);
        emit_u16(idx);
        return;
    }

    Object* first_clause = first(clauses);
    Object* rest_clauses = rest(clauses);

    if (!first_clause->pair()) {
        throw std::runtime_error("cond clause must be a list");
    }

    Object* test = first(first_clause);
    Object* actions = rest(first_clause);

    // Check for else clause
    std::string test_name = get_symbol_name(test);
    if (test_name == "else") {
        // Else clause - compile actions as begin
        if (is_nil(actions)) {
            // No actions - push nil
            uint16_t idx = add_constant(nil);
            emit(OP_LOAD_CONST);
            emit_u16(idx);
        } else if (is_nil(rest(actions))) {
            // Single action
            compile_expr(first(actions));
        } else {
            // Multiple actions - compile as begin
            compile_begin(actions);
        }
        return;
    }

    // Regular clause: (test => action...)
    // Compile as: (if test (begin action...) (cond rest...))

    bool saved_tail = tail_context_;

    // Compile test (not in tail position)
    tail_context_ = false;
    compile_expr(test);

    // Create labels for branching
    size_t else_label = make_label();
    size_t end_label = make_label();

    // Jump to else if test is false
    emit_jump(OP_JUMP_IF_FALSE, else_label);

    // Compile then branch (actions) in tail position
    tail_context_ = saved_tail;
    if (is_nil(actions)) {
        // No actions - push the test result (Scheme cond semantics)
        // But we already consumed it in JUMP_IF_FALSE, so push #t
        uint16_t idx = add_constant(t);
        emit(OP_LOAD_CONST);
        emit_u16(idx);
    } else if (is_nil(rest(actions))) {
        // Single action
        compile_expr(first(actions));
    } else {
        // Multiple actions
        compile_begin(actions);
    }
    emit_jump(OP_JUMP, end_label);

    // Compile else branch (remaining clauses) in tail position
    bind_label(else_label);
    tail_context_ = saved_tail;
    compile_cond(rest_clauses);

    bind_label(end_label);
    tail_context_ = saved_tail;
}

void Compiler::compile_guard(Object* args) {
    // (guard (var clause ...) body ...)   [R7RS 4.2.7]
    //   =>  (%guard (lambda (var) (cond clause ... [else (raise var)]))
    //               (lambda () body ...))
    //
    // The %guard primitive owns the C++ RAII work (handler install, payload
    // recovery); the rewrite reuses the existing lambda/cond compilation.
    // The else clause re-raising var is appended only when the user wrote
    // no else, giving R7RS "re-raise when no clause matches" semantics.
    if (is_nil(args) || !first(args)->pair()) {
        throw std::runtime_error("guard: malformed syntax - expected (guard (var clause ...) body ...)");
    }
    Object* spec = first(args);
    Object* var = first(spec);
    Object* clauses = rest(spec);
    Object* body = rest(args);

    // Root every node we build: cons/gc_new can trigger a collection that
    // would otherwise reclaim earlier nodes before they are linked in
    std::vector<gc_ptr<Object>> keep;
    auto K = [&keep](Object* o) { keep.emplace_back(o); return o; };
    auto S = [&K](const char* n) { return K(gc_new<a_symbol>(n)); };
    auto C = [&K](Object* a, Object* d) { return K(cons(a, d)); };

    bool has_else = false;
    for (Object* c = clauses; c && !is_nil(c); c = rest(c)) {
        Object* cl = first(c);
        if (cl && cl->pair() && get_symbol_name(first(cl)) == "else") {
            has_else = true;
        }
    }

    Object* cond_clauses = nil;
    if (!has_else) {
        // ((else (raise var)))
        Object* raise_call = C(S("raise"), C(var, nil));
        cond_clauses = C(C(S("else"), C(raise_call, nil)), nil);
    }
    std::vector<Object*> clause_vec;
    for (Object* c = clauses; c && !is_nil(c); c = rest(c)) {
        clause_vec.push_back(first(c));
    }
    for (auto it = clause_vec.rbegin(); it != clause_vec.rend(); ++it) {
        cond_clauses = C(*it, cond_clauses);
    }

    Object* cond_expr = C(S("cond"), cond_clauses);
    Object* handler_lambda = C(S("lambda"), C(C(var, nil), C(cond_expr, nil)));
    Object* thunk_lambda = C(S("lambda"), C(nil, body));
    Object* rewritten = C(S("%guard"), C(handler_lambda, C(thunk_lambda, nil)));

    compile_list(rewritten);
}

void Compiler::compile_parameterize(Object* args) {
    // (parameterize ((p v) ...) body ...)   [R7RS 4.2.6]
    //   =>  (%parameterize (list p ...) (list v ...) (lambda () body ...))
    //
    // All params and values are evaluated (by the list calls) before
    // %parameterize assigns any of them, as R7RS requires.
    if (is_nil(args)) {
        throw std::runtime_error("parameterize: malformed syntax - expected (parameterize ((param value) ...) body ...)");
    }
    Object* bindings = first(args);
    Object* body = rest(args);

    std::vector<gc_ptr<Object>> keep;
    auto K = [&keep](Object* o) { keep.emplace_back(o); return o; };
    auto S = [&K](const char* n) { return K(gc_new<a_symbol>(n)); };
    auto C = [&K](Object* a, Object* d) { return K(cons(a, d)); };

    std::vector<Object*> param_exprs;
    std::vector<Object*> value_exprs;
    for (Object* b = bindings; b && !is_nil(b); b = rest(b)) {
        Object* binding = first(b);
        if (!binding || !binding->pair() || is_nil(rest(binding))) {
            throw std::runtime_error("parameterize: each binding must be (param value)");
        }
        param_exprs.push_back(first(binding));
        value_exprs.push_back(second(binding));
    }

    Object* plist = nil;
    Object* vlist = nil;
    for (size_t i = param_exprs.size(); i-- > 0;) {
        plist = C(param_exprs[i], plist);
        vlist = C(value_exprs[i], vlist);
    }
    plist = C(S("list"), plist);
    vlist = C(S("list"), vlist);

    Object* thunk_lambda = C(S("lambda"), C(nil, body));
    Object* rewritten = C(S("%parameterize"), C(plist, C(vlist, C(thunk_lambda, nil))));

    compile_list(rewritten);
}

// ============================================================================
// Function application
// ============================================================================

void Compiler::compile_application(Object* fn, Object* args) {
    // Save tail context - function and arguments are NOT in tail position
    bool saved_tail = tail_context_;
    tail_context_ = false;

    // Check for optimizable primitives
    std::string fn_name = get_symbol_name(fn);
    if (!fn_name.empty()) {

        // Optimize built-in operations
        if (fn_name == "+" && list_length(args) == 2) {
            compile_expr(first(args));
            compile_expr(first(rest(args)));
            emit(OP_ADD);
            tail_context_ = saved_tail;
            return;
        }
        if (fn_name == "-" && list_length(args) == 2) {
            compile_expr(first(args));
            compile_expr(first(rest(args)));
            emit(OP_SUB);
            tail_context_ = saved_tail;
            return;
        }
        // Special case: unary minus (- x) compiles to (0 - x)
        if (fn_name == "-" && list_length(args) == 1) {
            // Push 0
            uint16_t zero_idx = add_constant(numeric::make_integer(0));
            emit(OP_LOAD_CONST);
            emit_u16(zero_idx);
            // Push argument
            compile_expr(first(args));
            // Emit SUB instruction
            emit(OP_SUB);
            tail_context_ = saved_tail;
            return;
        }
        if (fn_name == "*" && list_length(args) == 2) {
            compile_expr(first(args));
            compile_expr(first(rest(args)));
            emit(OP_MUL);
            tail_context_ = saved_tail;
            return;
        }
        if (fn_name == "/" && list_length(args) == 2) {
            compile_expr(first(args));
            compile_expr(first(rest(args)));
            emit(OP_DIV);
            tail_context_ = saved_tail;
            return;
        }
        if (fn_name == "=" && list_length(args) == 2) {
            compile_expr(first(args));
            compile_expr(first(rest(args)));
            emit(OP_EQ);
            tail_context_ = saved_tail;
            return;
        }
        if (fn_name == "<" && list_length(args) == 2) {
            compile_expr(first(args));
            compile_expr(first(rest(args)));
            emit(OP_LT);
            tail_context_ = saved_tail;
            return;
        }
        if (fn_name == ">" && list_length(args) == 2) {
            compile_expr(first(args));
            compile_expr(first(rest(args)));
            emit(OP_GT);
            tail_context_ = saved_tail;
            return;
        }
        if (fn_name == "cons" && list_length(args) == 2) {
            compile_expr(first(args));
            compile_expr(first(rest(args)));
            emit(OP_CONS);
            tail_context_ = saved_tail;
            return;
        }
        if (fn_name == "car" && list_length(args) == 1) {
            compile_expr(first(args));
            emit(OP_CAR);
            tail_context_ = saved_tail;
            return;
        }
        if (fn_name == "cdr" && list_length(args) == 1) {
            compile_expr(first(args));
            emit(OP_CDR);
            tail_context_ = saved_tail;  // Restore before return
            return;
        }

        // All other primitives also need to restore
        // (add more as needed)
    }

    // General function call
    // Compile function
    compile_expr(fn);

    // Compile arguments (left to right)
    size_t nargs = 0;
    Object* current = args;
    while (!is_nil(current)) {
        compile_expr(first(current));
        nargs++;
        current = rest(current);
    }

    if (nargs > 255) {
        throw std::runtime_error("Too many arguments to function (max 255)");
    }

    // Emit CALL or TAIL_CALL instruction depending on context
    if (saved_tail) {
        emit(OP_TAIL_CALL);
    } else {
        emit(OP_CALL);
    }
    emit(static_cast<uint8_t>(nargs));

    tail_context_ = saved_tail;  // Restore
}

// ============================================================================
// Utility methods
// ============================================================================

std::string Compiler::get_symbol_name(Object* sym) {
    if (!sym) return "";

    a_symbol* a_sym = dynamic_cast<a_symbol*>(sym);
    if (a_sym) return a_sym->value();

    a_string* a_str = dynamic_cast<a_string*>(sym);
    if (a_str) return a_str->value();

    return "";
}

size_t Compiler::list_length(Object* list) {
    size_t len = 0;
    while (!is_nil(list)) {
        len++;
        list = rest(list);
    }
    return len;
}

} } // namespace zombie::bytecode
