#pragma once
#include "zombie_runtime_types.h"
#include "coreir.h"
#include <list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace zombie::compilation {
enum class OutputKind { Bytecode, NativeJit, NativeObject };

class UnsupportedCompilation : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Artifacts own their code and constant roots. Object-file artifacts need not
// implement ExecutableCode; an AOT writer/linker can consume them separately.
class CompiledArtifact {
public:
    virtual ~CompiledArtifact() = default;
    virtual OutputKind output_kind() const noexcept = 0;
    virtual size_t code_size() const noexcept = 0;
};
class ExecutableCode : public CompiledArtifact {
public:
    virtual zombie::runtime::Object* execute(zombie::runtime::Environment* env) = 0;
};
class ICompilerBackend {
public:
    virtual ~ICompilerBackend() = default;
    virtual std::string name() const = 0;
    virtual bool supports(OutputKind output) const noexcept = 0;
    virtual std::shared_ptr<CompiledArtifact> compile(const coreir::CoreIRFunction& function,
                                                     OutputKind output) = 0;
};

struct CompilationStats {
    size_t cache_hits = 0, cache_misses = 0, compilations = 0;
    size_t fallback_compilations = 0, evictions = 0;
};

// Bounded LRU of compiled artifacts, never of evaluation results. Shared ownership
// pins executing artifacts even if reentrant evaluation evicts their cache entry.
class CompilationCache {
    struct Entry {
        std::string key;
        std::shared_ptr<CompiledArtifact> artifact;
        std::string fallback_reason;
    };
    size_t capacity_;
    std::list<Entry> entries_;
    std::unordered_map<std::string, std::list<Entry>::iterator> index_;
public:
    explicit CompilationCache(size_t capacity = 128) : capacity_(capacity) {}
    CompilationCache(const CompilationCache&) = delete;
    CompilationCache& operator=(const CompilationCache&) = delete;
    std::shared_ptr<CompiledArtifact> find(const std::string& key, std::string* fallback_reason = nullptr);
    bool insert(std::string key, std::shared_ptr<CompiledArtifact> artifact,
                std::string fallback_reason = {});
    void clear();
    size_t size() const noexcept { return entries_.size(); }
};

// Frontend stages run before cache lookup, so macro/environment changes are
// reflected in the lowered IR. A fallback is opt-in and only handles compile-time
// unsupported constructs, never runtime errors after execution has started.
class CompilationPipeline {
    std::unique_ptr<ICompilerBackend> backend_;
    std::unique_ptr<ICompilerBackend> fallback_;
    CompilationCache cache_;
    CompilationStats stats_;
    std::string last_fallback_;
public:
    explicit CompilationPipeline(std::unique_ptr<ICompilerBackend> backend,
                                 size_t cache_capacity = 128,
                                 std::unique_ptr<ICompilerBackend> fallback = {});
    std::shared_ptr<CompiledArtifact> compile(zombie::runtime::Object* expression,
                                             zombie::runtime::Environment* env,
                                             OutputKind output);
    zombie::runtime::Object* evaluate(zombie::runtime::Object* expression,
                                      zombie::runtime::Environment* env, OutputKind output);
    void clear_cache() { cache_.clear(); }
    const CompilationStats& stats() const noexcept { return stats_; }
    size_t cache_size() const noexcept { return cache_.size(); }
    const std::string& last_fallback_reason() const noexcept { return last_fallback_; }
};

class BytecodeCompilerBackend : public ICompilerBackend {
    bool debug_;
public:
    explicit BytecodeCompilerBackend(bool debug = false) : debug_(debug) {}
    std::string name() const override { return "coreir-bytecode-v1"; }
    bool supports(OutputKind output) const noexcept override { return output == OutputKind::Bytecode; }
    std::shared_ptr<CompiledArtifact> compile(const coreir::CoreIRFunction&, OutputKind) override;
};
} // namespace zombie::compilation
