#include "compilation.h"
#include "compilation_roots.h"
#include "scheme_to_coreir.h"
#include "coreir_to_bytecode.h"
#include "bytecode_vm.h"
#include "gc_ptr.h"
#include <bit>
#include <cstdint>
#include <functional>

namespace zombie::compilation {
namespace {
using namespace zombie::runtime;
using namespace coreir;

void visit_stmt(Stmt& stmt, const std::function<void(Atom&)>& visit);
void visit_expr(Expr& expr, const std::function<void(Atom&)>& visit) {
    if (expr.isPrim()) for (auto& a : std::get<Expr::Prim>(expr.data).args) visit(a);
    else if (expr.isCall()) {
        auto& c = std::get<Expr::Call>(expr.data);
        visit(c.func); for (auto& a : c.args) visit(a);
    } else if (expr.isSetGlobal()) visit(std::get<Expr::SetGlobal>(expr.data).value);
    else if (expr.isSetLocal()) visit(std::get<Expr::SetLocal>(expr.data).value);
    else if (expr.isLambda()) {
        auto& l = std::get<Expr::Lambda>(expr.data);
        if (l.body) visit_stmt(*l.body, visit);
    } else if (expr.isValues()) for (auto& a : std::get<Expr::Values>(expr.data).values) visit(a);
    else if (expr.isCallWithValues()) {
        auto& c = std::get<Expr::CallWithValues>(expr.data);
        visit(c.producer); visit(c.consumer);
    }
}
void visit_stmt(Stmt& stmt, const std::function<void(Atom&)>& visit) {
    if (stmt.isLet()) {
        auto& l = stmt.asLet();
        if (l.expr) visit_expr(*l.expr, visit);
        if (l.next) visit_stmt(*l.next, visit);
    } else if (stmt.isIf()) {
        auto& i = stmt.asIf(); visit(i.cond);
        if (i.then_branch) visit_stmt(*i.then_branch, visit);
        if (i.else_branch) visit_stmt(*i.else_branch, visit);
    } else visit(stmt.asReturn().value);
}

// Exact structural encoding (not the lossy IR pretty printer). Cache only scalar
// immutable literals: sharing quoted strings, lists, or closures across distinct
// parsed programs could change identity/mutation semantics. Those units bypass it.
class Fingerprint {
    std::string key_;
    bool cacheable_ = true;
    void word(const std::string& w) { key_ += std::to_string(w.size()) + ":" + w; }
    void atom(const Atom& a) {
        word(std::to_string(static_cast<int>(a.type_hint)));
        if (a.isVar()) { word("var"); word(a.asVar().name); return; }
        auto* o = a.asLit().value;
        word("lit");
        if (!o) { word("null"); return; }
        if (o == nil) { word("nil"); return; }
        if (o == empty_list) { word("empty"); return; }
        word(std::to_string(o->get_type()));
        switch (static_cast<Type>(o->get_type())) {
        case Type::INT: word(std::to_string(static_cast<a_int*>(o)->value())); break;
        case Type::LONG: word(std::to_string(static_cast<a_long*>(o)->value())); break;
        case Type::DOUBLE: word(std::to_string(std::bit_cast<uint64_t>(static_cast<a_double*>(o)->value()))); break;
        case Type::BOOL: word(static_cast<a_bool*>(o)->value() ? "true" : "false"); break;
        default: cacheable_ = false;
        }
    }
    void atoms(const std::vector<Atom>& args) {
        word(std::to_string(args.size())); for (const auto& a : args) atom(a);
    }
    void expr(const Expr& e) {
        word(std::to_string(e.data.index()));
        word(std::to_string(static_cast<int>(e.inferred_type)));
        if (e.isPrim()) { word(e.asPrim().op); atoms(e.asPrim().args); }
        else if (e.isCall()) {
            auto& c = e.asCall(); atom(c.func); atoms(c.args); word(c.is_tail_call ? "tail" : "call");
        } else if (e.isGetGlobal()) word(e.asGetGlobal().name);
        else if (e.isSetGlobal()) { word(e.asSetGlobal().name); atom(e.asSetGlobal().value); }
        else if (e.isSetLocal()) { word(e.asSetLocal().name); atom(e.asSetLocal().value); }
        else if (e.isLambda()) {
            auto& l = e.asLambda(); names(l.params); word(l.rest); stmt(l.body.get());
        } else if (e.isValues()) atoms(e.asValues().values);
        else if (e.isCallWithValues()) {
            auto& c = e.asCallWithValues(); atom(c.producer); atom(c.consumer);
            word(c.is_tail_call ? "tail" : "call");
        }
    }
    void stmt(const Stmt* s) {
        if (!s) { word("null-stmt"); return; }
        word(std::to_string(s->data.index()));
        if (s->isLet()) {
            auto& l = s->asLet(); word(l.name);
            if (l.expr) expr(*l.expr); else word("null-expr");
            stmt(l.next.get());
        } else if (s->isIf()) {
            auto& i = s->asIf(); atom(i.cond); stmt(i.then_branch.get()); stmt(i.else_branch.get());
        } else atom(s->asReturn().value);
    }
    void names(const std::vector<std::string>& names) {
        word(std::to_string(names.size())); for (const auto& n : names) word(n);
    }
public:
    std::optional<std::string> encode(const CoreIRFunction& f) {
        word(f.name); names(f.params); stmt(f.body.get());
        return cacheable_ ? std::optional<std::string>(std::move(key_)) : std::nullopt;
    }
};

class BytecodeArtifact final : public ExecutableCode {
    coreir::CompiledFunction function_;
    bool debug_;
    ScopedRootTracer roots_;
public:
    BytecodeArtifact(coreir::CompiledFunction function, bool debug)
        : function_(std::move(function)), debug_(debug), roots_([this](Tracer& t) {
            for (auto*& c : function_.constants) t.visit_field(c);
        }) {}
    OutputKind output_kind() const noexcept override { return OutputKind::Bytecode; }
    size_t code_size() const noexcept override { return function_.bytecode.size(); }
    Object* execute(Environment* env) override {
        if (!env) throw std::invalid_argument("Bytecode execution needs an environment");
        zombie::bytecode::BytecodeVM vm(env, debug_);
        return vm.execute(function_.bytecode, function_.constants, env);
    }
};
} // namespace

std::shared_ptr<CompiledArtifact> CompilationCache::find(const std::string& key, std::string* fallback_reason) {
    auto found = index_.find(key);
    if (found == index_.end()) return {};
    entries_.splice(entries_.begin(), entries_, found->second);
    if (fallback_reason) *fallback_reason = found->second->fallback_reason;
    return found->second->artifact;
}
bool CompilationCache::insert(std::string key, std::shared_ptr<CompiledArtifact> artifact,
                              std::string fallback_reason) {
    if (!capacity_) return false;
    if (!artifact) throw std::invalid_argument("Cannot cache a null artifact");
    if (auto found = index_.find(key); found != index_.end()) {
        found->second->artifact = std::move(artifact);
        found->second->fallback_reason = std::move(fallback_reason);
        entries_.splice(entries_.begin(), entries_, found->second);
        return false;
    }
    entries_.push_front({std::move(key), std::move(artifact), std::move(fallback_reason)});
    try { index_.emplace(entries_.front().key, entries_.begin()); }
    catch (...) { entries_.pop_front(); throw; }
    if (entries_.size() <= capacity_) return false;
    index_.erase(entries_.back().key);
    entries_.pop_back();
    return true;
}
void CompilationCache::clear() { index_.clear(); entries_.clear(); }

CompilationPipeline::CompilationPipeline(std::unique_ptr<ICompilerBackend> backend,
        size_t capacity, std::unique_ptr<ICompilerBackend> fallback)
    : backend_(std::move(backend)), fallback_(std::move(fallback)), cache_(capacity) {
    if (!backend_) throw std::invalid_argument("Compilation requires a backend");
}
std::shared_ptr<CompiledArtifact> CompilationPipeline::compile(Object* expression,
        Environment* env, OutputKind output) {
    if (!expression || !env) throw std::invalid_argument("Compilation requires expression and environment");
    if (!backend_->supports(output))
        throw UnsupportedCompilation(backend_->name() + ": requested output is not implemented");
    gc_root<Object> source(expression);
    gc_root<Environment> environment(env);
    coreir::SchemeToCoreirLowering lowering(env);
    auto function = lowering.lower_expression_to_function(source.get(), "__eval__");
    ScopedRootTracer roots([&](Tracer& t) {
        if (function->body) visit_stmt(*function->body, [&](Atom& a) {
            if (a.isLit()) t.visit_field(std::get<Atom::Lit>(a.data).value);
        });
    });
    auto key = Fingerprint().encode(*function);
    if (key) {
        *key = backend_->name() + ":" + std::to_string(static_cast<int>(output)) + ":" + *key;
        if (auto artifact = cache_.find(*key, &last_fallback_)) { ++stats_.cache_hits; return artifact; }
    }
    ++stats_.cache_misses;
    std::shared_ptr<CompiledArtifact> artifact;
    last_fallback_.clear();
    try { artifact = backend_->compile(*function, output); }
    catch (const UnsupportedCompilation& e) {
        if (output != OutputKind::NativeJit || !fallback_ || !fallback_->supports(OutputKind::Bytecode)) throw;
        last_fallback_ = e.what();
        artifact = fallback_->compile(*function, OutputKind::Bytecode);
        ++stats_.fallback_compilations;
    }
    if (!artifact) throw std::runtime_error("Compiler returned a null artifact");
    ++stats_.compilations;
    if (key && cache_.insert(std::move(*key), artifact, last_fallback_)) ++stats_.evictions;
    return artifact;
}
Object* CompilationPipeline::evaluate(Object* expression, Environment* env, OutputKind output) {
    auto artifact = compile(expression, env, output);
    auto executable = std::dynamic_pointer_cast<ExecutableCode>(artifact);
    if (!executable) throw std::runtime_error("Compilation output is not executable");
    return executable->execute(env);
}
std::shared_ptr<CompiledArtifact> BytecodeCompilerBackend::compile(const coreir::CoreIRFunction& function,
                                                                 OutputKind output) {
    if (!supports(output)) throw UnsupportedCompilation("Bytecode backend only emits bytecode");
    if (!function.body) throw std::invalid_argument("CoreIR function has no body");
    coreir::CoreIRToBytecode translator;
    return std::make_shared<BytecodeArtifact>(translator.compile_function(function), debug_);
}
} // namespace zombie::compilation
