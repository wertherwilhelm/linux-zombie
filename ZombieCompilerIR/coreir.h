#pragma once

#include <string>
#include <vector>
#include <variant>
#include <memory>

// Forward declarations
class Object;
class Environment;

namespace coreir {

// ============================================================================
// Type Hints - Optional type information for optimization and verification
// ============================================================================

enum class TypeHint {
    UNKNOWN,   // No type information available
    INT,       // Exact integer (can be unboxed in JIT)
    NUM,       // Any number (int or float)
    BOOL,      // Boolean (#t or #f)
    STRING,    // String
    SYMBOL,    // Symbol/identifier
    CLOSURE,   // Lambda/Procedure
    LIST,      // Cons cell or nil
    VECTOR,    // Vector
    VOID       // No return value (for side-effect operations like set!, display)
};

// Convert TypeHint to string for debugging
const char* type_hint_name(TypeHint hint);

// ============================================================================
// Value Type - Reuse existing Object* model
// ============================================================================

using Value = Object*;

// ============================================================================
// Atom - Already-evaluated values (variables or literals)
// ============================================================================

struct Atom {
    struct Var {
        std::string name;

        Var(const std::string& n) : name(n) {}
    };

    struct Lit {
        Value value;  // Object*

        Lit(Value v) : value(v) {}
    };

    std::variant<Var, Lit> data;
    TypeHint type_hint = TypeHint::UNKNOWN;  // Optional type information

    // Default constructor (needed for factory methods)
    Atom() : data(Lit(nullptr)), type_hint(TypeHint::UNKNOWN) {}

    // Constructors
    static Atom makeVar(const std::string& name, TypeHint hint = TypeHint::UNKNOWN) {
        Atom a;
        a.data = Var(name);
        a.type_hint = hint;
        return a;
    }

    static Atom makeLit(Value v, TypeHint hint = TypeHint::UNKNOWN) {
        Atom a;
        a.data = Lit(v);
        a.type_hint = hint;
        return a;
    }

    // Type checks
    bool isVar() const { return std::holds_alternative<Var>(data); }
    bool isLit() const { return std::holds_alternative<Lit>(data); }

    // Accessors
    const Var& asVar() const { return std::get<Var>(data); }
    const Lit& asLit() const { return std::get<Lit>(data); }
};

// ============================================================================
// Forward declarations for Expr/Stmt
// ============================================================================

struct Expr;
struct Stmt;

// ============================================================================
// Expr - Value-producing expressions (operands must be Atoms)
// ============================================================================

struct Expr {
    // Primitive operation: op(args...)
    struct Prim {
        std::string op;           // Operation name ("+", "-", "*", etc.)
        std::vector<Atom> args;   // All operands are atoms

        Prim(const std::string& o, std::vector<Atom> a)
            : op(o), args(std::move(a)) {}
    };

    // Function call: func(args...)
    struct Call {
        Atom func;                // Function to call (Var or Lit)
        std::vector<Atom> args;   // All arguments are atoms
        bool is_tail_call;        // True if this call is in tail position

        Call(Atom f, std::vector<Atom> a, bool tail = false)
            : func(std::move(f)), args(std::move(a)), is_tail_call(tail) {}
    };

    // Get global variable
    struct GetGlobal {
        std::string name;

        GetGlobal(const std::string& n) : name(n) {}
    };

    // Set global variable
    struct SetGlobal {
        std::string name;
        Atom value;  // Value to set (atom)

        SetGlobal(const std::string& n, Atom v)
            : name(n), value(std::move(v)) {}
    };

    // Set local variable (for letrec support)
    struct SetLocal {
        std::string name;
        Atom value;  // Value to set (atom)

        SetLocal(const std::string& n, Atom v)
            : name(n), value(std::move(v)) {}
    };

    // Lambda (Closure creation)
    // NOTE: Lambda execution is TODO for Phase 1
    struct Lambda {
        std::vector<std::string> params;
        std::unique_ptr<Stmt> body;

        Lambda(std::vector<std::string> p, std::unique_ptr<Stmt> b)
            : params(std::move(p)), body(std::move(b)) {}
    };

    // Multiple values: values(val1, val2, ...)
    // Returns multiple values (for call-with-values)
    struct Values {
        std::vector<Atom> values;  // All values are atoms

        Values(std::vector<Atom> v) : values(std::move(v)) {}
    };

    // Call-with-values: call_with_values(producer, consumer)
    // Special call that receives multiple values from producer and passes them to consumer
    struct CallWithValues {
        Atom producer;   // Function that produces values (must be 0-arity)
        Atom consumer;   // Function that consumes values
        bool is_tail_call;  // True if in tail position

        CallWithValues(Atom p, Atom c, bool tail = false)
            : producer(std::move(p)), consumer(std::move(c)), is_tail_call(tail) {}
    };

    std::variant<Prim, Call, GetGlobal, SetGlobal, SetLocal, Lambda, Values, CallWithValues> data;
    TypeHint inferred_type = TypeHint::UNKNOWN;  // Inferred result type

    // Default constructor (needed for factory methods)
    Expr() : data(Prim("", {})), inferred_type(TypeHint::UNKNOWN) {}

    // Constructors
    static Expr makePrim(const std::string& op, std::vector<Atom> args, TypeHint hint = TypeHint::UNKNOWN) {
        Expr e;
        e.data = Prim(op, std::move(args));
        e.inferred_type = hint;
        return e;
    }

    static Expr makeCall(Atom func, std::vector<Atom> args, TypeHint hint = TypeHint::UNKNOWN, bool is_tail_call = false) {
        Expr e;
        e.data = Call(std::move(func), std::move(args), is_tail_call);
        e.inferred_type = hint;
        return e;
    }

    static Expr makeGetGlobal(const std::string& name, TypeHint hint = TypeHint::UNKNOWN) {
        Expr e;
        e.data = GetGlobal(name);
        e.inferred_type = hint;
        return e;
    }

    static Expr makeSetGlobal(const std::string& name, Atom value) {
        Expr e;
        e.data = SetGlobal(name, std::move(value));
        e.inferred_type = TypeHint::VOID;  // set! returns void
        return e;
    }

    static Expr makeSetLocal(const std::string& name, Atom value) {
        Expr e;
        e.data = SetLocal(name, std::move(value));
        e.inferred_type = TypeHint::VOID;  // set! returns void
        return e;
    }

    static Expr makeLambda(std::vector<std::string> params, std::unique_ptr<Stmt> body) {
        Expr e;
        e.data = Lambda(std::move(params), std::move(body));
        e.inferred_type = TypeHint::CLOSURE;  // lambda creates Closure
        return e;
    }

    static Expr makeValues(std::vector<Atom> values) {
        Expr e;
        e.data = Values(std::move(values));
        e.inferred_type = TypeHint::UNKNOWN;  // multiple values - type is dynamic
        return e;
    }

    static Expr makeCallWithValues(Atom producer, Atom consumer, bool is_tail_call = false) {
        Expr e;
        e.data = CallWithValues(std::move(producer), std::move(consumer), is_tail_call);
        e.inferred_type = TypeHint::UNKNOWN;  // return type depends on consumer
        return e;
    }

    // Type checks
    bool isPrim() const { return std::holds_alternative<Prim>(data); }
    bool isCall() const { return std::holds_alternative<Call>(data); }
    bool isGetGlobal() const { return std::holds_alternative<GetGlobal>(data); }
    bool isSetGlobal() const { return std::holds_alternative<SetGlobal>(data); }
    bool isSetLocal() const { return std::holds_alternative<SetLocal>(data); }
    bool isLambda() const { return std::holds_alternative<Lambda>(data); }
    bool isValues() const { return std::holds_alternative<Values>(data); }
    bool isCallWithValues() const { return std::holds_alternative<CallWithValues>(data); }

    // Accessors
    const Prim& asPrim() const { return std::get<Prim>(data); }
    const Call& asCall() const { return std::get<Call>(data); }
    const GetGlobal& asGetGlobal() const { return std::get<GetGlobal>(data); }
    const SetGlobal& asSetGlobal() const { return std::get<SetGlobal>(data); }
    const SetLocal& asSetLocal() const { return std::get<SetLocal>(data); }
    const Lambda& asLambda() const { return std::get<Lambda>(data); }
    const Values& asValues() const { return std::get<Values>(data); }
    const CallWithValues& asCallWithValues() const { return std::get<CallWithValues>(data); }
};

// ============================================================================
// Stmt - ANF statements (control flow)
// ============================================================================

struct Stmt {
    // Let binding: let name = expr; next
    struct Let {
        std::string name;
        std::unique_ptr<Expr> expr;
        std::unique_ptr<Stmt> next;

        Let(const std::string& n, std::unique_ptr<Expr> e, std::unique_ptr<Stmt> nxt)
            : name(n), expr(std::move(e)), next(std::move(nxt)) {}
    };

    // Conditional: if cond then else
    struct If {
        Atom cond;                      // Condition (must be atom)
        std::unique_ptr<Stmt> then_branch;
        std::unique_ptr<Stmt> else_branch;

        If(Atom c, std::unique_ptr<Stmt> t, std::unique_ptr<Stmt> e)
            : cond(std::move(c)), then_branch(std::move(t)), else_branch(std::move(e)) {}
    };

    // Return value
    struct Return {
        Atom value;  // Value to return (must be atom)

        Return(Atom v) : value(std::move(v)) {}
    };

    std::variant<Let, If, Return> data;

    // Default constructor (needed for factory methods)
    Stmt() : data(Return(Atom::makeLit(nullptr))) {}

    // Constructors
    static std::unique_ptr<Stmt> makeLet(const std::string& name,
                                          std::unique_ptr<Expr> expr,
                                          std::unique_ptr<Stmt> next) {
        auto s = std::make_unique<Stmt>();
        s->data = Let(name, std::move(expr), std::move(next));
        return s;
    }

    static std::unique_ptr<Stmt> makeIf(Atom cond,
                                         std::unique_ptr<Stmt> then_branch,
                                         std::unique_ptr<Stmt> else_branch) {
        auto s = std::make_unique<Stmt>();
        s->data = If(std::move(cond), std::move(then_branch), std::move(else_branch));
        return s;
    }

    static std::unique_ptr<Stmt> makeReturn(Atom value) {
        auto s = std::make_unique<Stmt>();
        s->data = Return(std::move(value));
        return s;
    }

    // Type checks
    bool isLet() const { return std::holds_alternative<Let>(data); }
    bool isIf() const { return std::holds_alternative<If>(data); }
    bool isReturn() const { return std::holds_alternative<Return>(data); }

    // Accessors
    const Let& asLet() const { return std::get<Let>(data); }
    const If& asIf() const { return std::get<If>(data); }
    const Return& asReturn() const { return std::get<Return>(data); }

    Let& asLet() { return std::get<Let>(data); }
    If& asIf() { return std::get<If>(data); }
    Return& asReturn() { return std::get<Return>(data); }
};

// ============================================================================
// CoreIRFunction - Top-level callable
// ============================================================================

struct CoreIRFunction {
    std::string name;
    std::vector<std::string> params;
    std::unique_ptr<Stmt> body;

    CoreIRFunction(const std::string& n,
                   std::vector<std::string> p,
                   std::unique_ptr<Stmt> b)
        : name(n), params(std::move(p)), body(std::move(b)) {}
};

// ============================================================================
// Pretty-printing for debugging
// ============================================================================

std::string print_atom(const Atom& atom);
std::string print_expr(const Expr& expr);
std::string print_stmt(const Stmt& stmt, int indent = 0);
std::string print_function(const CoreIRFunction& func);

} // namespace coreir
