#include "native_compiler.h"
#include "compilation_roots.h"
#include "native_primitive.h"
#include "gc_ptr.h"
#include <exception>
#include <set>
#include <unordered_map>

#if ZOMBIE_ENABLE_ASMJIT && (defined(__x86_64__) || defined(_M_X64))
#define ZOMBIE_NATIVE_X64 1
#include <asmjit/x86.h>
#else
#define ZOMBIE_NATIVE_X64 0
#endif

namespace zombie::native {
using namespace zombie::compilation;
using namespace zombie::runtime;
using namespace coreir;

#if ZOMBIE_NATIVE_X64
struct NativeCompilerBackend::Runtime { asmjit::JitRuntime jit; };
namespace {
enum class StepKind { Literal, Global, StoreGlobal, Copy, Primitive };
struct Step {
    StepKind kind;
    size_t destination;
    std::string name;
    std::vector<size_t> arguments;
    size_t constant = 0;
};
struct Program {
    std::vector<Step> steps;
    std::vector<Object*> constants;
    size_t slots = 0;
};
struct Frame {
    const Program& program;
    Environment* env;
    std::vector<Object*> slots;
    std::exception_ptr error;
    ScopedRootTracer roots;
    Frame(const Program& p, Environment* e)
        : program(p), env(e), slots(p.slots, nullptr), roots([this](Tracer& t) {
            t.visit_field(env);
            for (auto*& value : slots) t.visit_field(value);
        }) {}
};

// No exception crosses a generated frame. The caller rethrows after the native
// function returns; all temporary values and arguments stay in traced slots.
int run_step(Frame* f, size_t index) noexcept {
    try {
        const Step& step = f->program.steps[index];
        Object* value = nullptr;
        switch (step.kind) {
        case StepKind::Literal: value = f->program.constants[step.constant]; break;
        case StepKind::Global: value = f->env->lookup(step.name); break;
        case StepKind::StoreGlobal:
            f->env->define(step.name, f->slots[step.arguments[0]]);
            value = nil;
            break;
        case StepKind::Copy: value = f->slots[step.arguments[0]]; break;
        case StepKind::Primitive: {
            gc_root<Object> procedure(f->env->lookup(step.name));
            auto* primitive = dynamic_cast<NativePrimitive*>(procedure.get());
            if (!primitive)
                throw std::runtime_error("Native primitive binding is no longer a primitive: " + step.name);
            gc_root<Object> args(nil);
            for (auto it = step.arguments.rbegin(); it != step.arguments.rend(); ++it)
                args.reset(cons(f->slots[*it], args.get()));
            value = primitive->apply(nullptr, args.get(), f->env);
            break;
        }
        }
        if (!value) throw std::runtime_error("Native helper returned null");
        f->slots[step.destination] = value;
        return 1;
    } catch (...) {
        f->error = std::current_exception();
        return 0;
    }
}
int slot_truth(Frame* f, size_t slot) noexcept { return truth(f->slots[slot]) ? 1 : 0; }
Object* slot_value(Frame* f, size_t slot) noexcept { return f->slots[slot]; }

class AsmErrors final : public asmjit::ErrorHandler {
public:
    void handle_error(asmjit::Error, const char* message, asmjit::BaseEmitter*) override {
        throw std::runtime_error(std::string("asmjit: ") + message);
    }
};
void check(asmjit::Error error) {
    if (error != asmjit::Error::kOk)
        throw std::runtime_error(std::string("asmjit: ") + asmjit::stringify_error(error));
}

// The compiler emits CoreIR sequencing and branches directly as machine code.
// Dynamic primitive operations use the existing boxed runtime ABI. There is no
// bytecode interpreter or runtime IR dispatch loop behind the native entry point.
class Emitter {
    asmjit::x86::Compiler& cc_;
    asmjit::x86::Gp frame_;
    asmjit::Label failure_;
    Program& program_;
    using Locals = std::unordered_map<std::string, size_t>;

    asmjit::x86::Gp call(uint64_t address, size_t index, bool pointer_result = false) {
        asmjit::InvokeNode* invoke = nullptr;
        auto result = pointer_result ? cc_.new_gp_ptr() : cc_.new_gp32();
        check(cc_.invoke(asmjit::Out(invoke), asmjit::imm(address),
            pointer_result ? asmjit::FuncSignature::build<Object*, Frame*, size_t>()
                           : asmjit::FuncSignature::build<int, Frame*, size_t>()));
        invoke->set_arg(0, frame_);
        invoke->set_arg(1, asmjit::imm(index));
        invoke->set_ret(0, result);
        return result;
    }
    size_t step(Step operation) {
        size_t index = program_.steps.size();
        size_t slot = operation.destination;
        program_.steps.push_back(std::move(operation));
        auto ok = call(reinterpret_cast<uint64_t>(&run_step), index);
        cc_.test(ok, ok);
        cc_.jz(failure_);
        return slot;
    }
    size_t atom(const Atom& a, const Locals& locals) {
        if (a.isVar()) {
            if (auto found = locals.find(a.asVar().name); found != locals.end()) return found->second;
            return step({StepKind::Global, program_.slots++, a.asVar().name, {}});
        }
        size_t constant = program_.constants.size();
        program_.constants.push_back(a.asLit().value ? a.asLit().value : nil);
        return step({StepKind::Literal, program_.slots++, {}, {}, constant});
    }
    size_t expr(const Expr& e, const Locals& locals) {
        if (e.isPrim()) {
            auto& p = e.asPrim();
            // These primitives consume already evaluated arguments and do not
            // need an evaluator callback. Extend deliberately as the ABI grows.
            static const std::set<std::string> supported = {
                "+", "-", "*", "/", "<", ">", "<=", ">=", "=", "not",
                "cons", "car", "cdr", "list", "null?", "pair?", "eq?", "eqv?", "equal?",
                "display", "newline", "write", "number?", "integer?", "string?", "symbol?",
                "boolean?", "vector?", "vector", "vector-ref", "vector-length", "string-length"
            };
            if (!supported.count(p.op)) throw UnsupportedCompilation("Native primitive not implemented: " + p.op);
            std::vector<size_t> arguments;
            for (const auto& a : p.args) arguments.push_back(atom(a, locals));
            return step({StepKind::Primitive, program_.slots++, p.op, std::move(arguments)});
        }
        if (e.isGetGlobal()) return step({StepKind::Global, program_.slots++, e.asGetGlobal().name, {}});
        if (e.isSetGlobal()) {
            size_t value = atom(e.asSetGlobal().value, locals);
            return step({StepKind::StoreGlobal, program_.slots++, e.asSetGlobal().name, {value}});
        }
        if (e.isSetLocal()) {
            auto& s = e.asSetLocal();
            auto found = locals.find(s.name);
            if (found == locals.end()) throw UnsupportedCompilation("Native set-local has no binding: " + s.name);
            size_t value = atom(s.value, locals);
            step({StepKind::Copy, found->second, {}, {value}});
            return atom(Atom::makeLit(nil), locals);
        }
        const char* construct = e.isLambda() ? "lambda/closure creation"
            : e.isCall() ? "general procedure calls"
            : e.isValues() ? "multiple values" : "call-with-values";
        throw UnsupportedCompilation(std::string("Native CoreIR construct not implemented: ") + construct);
    }
public:
    Emitter(asmjit::x86::Compiler& cc, asmjit::x86::Gp frame, Program& p)
        : cc_(cc), frame_(frame), failure_(cc.new_label()), program_(p) {}
    void stmt(const Stmt* s, Locals locals = {}) {
        if (!s) throw std::invalid_argument("Native CoreIR has a missing statement");
        if (s->isLet()) {
            auto& l = s->asLet();
            if (!l.expr) throw std::invalid_argument("Native CoreIR let has no expression");
            size_t value = expr(*l.expr, locals);
            // A binding has its own slot even when its initializer aliases another
            // local; set-local must not mutate the initializer's binding.
            size_t binding = program_.slots++;
            step({StepKind::Copy, binding, {}, {value}});
            locals[l.name] = binding;
            stmt(l.next.get(), std::move(locals));
        } else if (s->isIf()) {
            auto& i = s->asIf();
            size_t condition = atom(i.cond, locals);
            auto result = call(reinterpret_cast<uint64_t>(&slot_truth), condition);
            auto otherwise = cc_.new_label();
            cc_.test(result, result);
            cc_.jz(otherwise);
            stmt(i.then_branch.get(), locals); // Both branches terminate in Return.
            cc_.bind(otherwise);
            stmt(i.else_branch.get(), std::move(locals));
        } else {
            size_t slot = atom(s->asReturn().value, locals);
            cc_.ret(call(reinterpret_cast<uint64_t>(&slot_value), slot, true));
        }
    }
    void finish() {
        cc_.bind(failure_);
        auto zero = cc_.new_gp_ptr();
        cc_.xor_(zero, zero);
        cc_.ret(zero);
    }
};
using EntryPoint = Object* (*)(Frame*);

class NativeArtifact final : public ExecutableCode {
    // Owner type is erased so Runtime can remain an implementation detail.
    std::shared_ptr<void> runtime_owner_;
    asmjit::JitRuntime& runtime_;
    Program program_;
    EntryPoint entry_ = nullptr;
    size_t code_size_ = 0;
    ScopedRootTracer roots_;
public:
    NativeArtifact(std::shared_ptr<void> owner, asmjit::JitRuntime& runtime, Program program)
        : runtime_owner_(std::move(owner)), runtime_(runtime), program_(std::move(program)),
          roots_([this](Tracer& t) { for (auto*& c : program_.constants) t.visit_field(c); }) {}
    ~NativeArtifact() override { if (entry_) runtime_.release(entry_); }
    void install(asmjit::CodeHolder& code) {
        code_size_ = code.code_size();
        check(runtime_.add(&entry_, &code));
    }
    OutputKind output_kind() const noexcept override { return OutputKind::NativeJit; }
    size_t code_size() const noexcept override { return code_size_; }
    Object* execute(Environment* env) override {
        if (!env) throw std::invalid_argument("Native execution needs an environment");
        Frame frame(program_, env);
        Object* result = entry_(&frame);
        if (frame.error) std::rethrow_exception(frame.error);
        return result;
    }
};
} // namespace
#else
struct NativeCompilerBackend::Runtime {};
#endif

NativeCompilerBackend::NativeCompilerBackend() : runtime_(std::make_shared<Runtime>()) {}
NativeCompilerBackend::~NativeCompilerBackend() = default;
bool NativeCompilerBackend::available() noexcept { return ZOMBIE_NATIVE_X64 != 0; }
bool NativeCompilerBackend::supports(OutputKind output) const noexcept {
    return available() && output == OutputKind::NativeJit;
}
std::shared_ptr<CompiledArtifact> NativeCompilerBackend::compile(const coreir::CoreIRFunction& function,
                                                               OutputKind output) {
    if (!supports(output)) throw UnsupportedCompilation("asmjit backend only supports host x86-64 JIT output");
#if ZOMBIE_NATIVE_X64
    if (!function.params.empty()) throw UnsupportedCompilation("Native function arguments/closures are not implemented yet");
    AsmErrors errors;
    asmjit::CodeHolder code;
    check(code.init(runtime_->jit.environment(), runtime_->jit.cpu_features()));
    code.set_error_handler(&errors);
    asmjit::x86::Compiler cc(&code);
    auto* fn = cc.add_func(asmjit::FuncSignature::build<Object*, Frame*>());
    auto frame = cc.new_gp_ptr("frame");
    fn->set_arg(0, frame);
    Program program;
    Emitter emitter(cc, frame, program);
    emitter.stmt(function.body.get());
    emitter.finish();
    cc.end_func();
    check(cc.finalize());
    auto artifact = std::make_shared<NativeArtifact>(runtime_, runtime_->jit, std::move(program));
    artifact->install(code);
    return artifact;
#else
    throw UnsupportedCompilation("asmjit native compilation is unavailable in this build");
#endif
}
} // namespace zombie::native
