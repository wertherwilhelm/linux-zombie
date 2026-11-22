#pragma once

/**
 * @file runtime_context.h
 * @brief Unified runtime context for Zombie Scheme
 *
 * RuntimeContext consolidates all runtime state that was previously scattered
 * across globals: GC, symbol table, ports, ledger, mirrors, library registry.
 *
 * Design principles:
 * - Single Object owns all runtime state
 * - Thread-local current context for implicit access
 * - Explicit context passing where performance matters
 * - Incremental migration from globals (delegates initially)
 *
 * Usage:
 *   // Get current context (thread-local)
 *   RuntimeContext* ctx = RuntimeContext::current();
 *
 *   // Access services
 *   ctx->gc()->collect();
 *   ctx->output_port()->write("hello");
 *   a_symbol* sym = ctx->intern("my-symbol");
 *
 *   // Create isolated context
 *   auto ctx = RuntimeContext::create();
 *   RuntimeContext::Scope scope(ctx.get());  // Sets as current
 *   // ... use ctx ...
 *   // Scope destructor restores previous context
 */

#include <memory>
#include <string>
#include <functional>

// Forward declarations
class IGarbageCollector;

// Forward declarations in zombie::runtime namespace
namespace zombie { namespace runtime {
    class InputPort;
    class OutputPort;
    class a_symbol;
    class Object;
    class Environment;
} }

// Use the namespaced types
using zombie::runtime::InputPort;
using zombie::runtime::OutputPort;
using zombie::runtime::a_symbol;
using zombie::runtime::Object;
using zombie::runtime::Environment;

namespace r7rs {
    class LibraryRegistry;
}

/**
 * @brief Unified runtime context containing all Scheme runtime state
 *
 * This class serves as the single point of access for all runtime services.
 * It replaces scattered globals with a cohesive, configurable Object.
 */
class RuntimeContext {
public:
    // ========================================================================
    // Construction / Destruction
    // ========================================================================

    /**
     * @brief Create a new runtime context
     * @param initialize_gc If true, initialize GC subsystem
     *
     * Note: For most use cases, use RuntimeContext::create() factory.
     */
    explicit RuntimeContext(bool initialize_gc = true);

    /**
     * @brief Destructor - cleans up owned resources
     */
    ~RuntimeContext();

    // Non-copyable, movable
    RuntimeContext(const RuntimeContext&) = delete;
    RuntimeContext& operator=(const RuntimeContext&) = delete;
    RuntimeContext(RuntimeContext&&) = default;
    RuntimeContext& operator=(RuntimeContext&&) = default;

    /**
     * @brief Factory to create a fully initialized context
     * @return Unique pointer to new context
     */
    static std::unique_ptr<RuntimeContext> create();

    // ========================================================================
    // Thread-Local Current Context
    // ========================================================================

    /**
     * @brief Get the current thread's runtime context
     * @return Current context, or nullptr if none set
     *
     * Most code should use this to access runtime services.
     */
    static RuntimeContext* current();

    /**
     * @brief Set the current thread's runtime context
     * @param ctx Context to set (can be nullptr)
     * @return Previous context
     */
    static RuntimeContext* set_current(RuntimeContext* ctx);

    /**
     * @brief RAII scope guard for setting current context
     *
     * Usage:
     *   RuntimeContext::Scope scope(my_context);
     *   // my_context is now current
     *   // ... do work ...
     *   // Previous context restored on scope exit
     */
    class Scope {
    public:
        explicit Scope(RuntimeContext* ctx) : previous_(set_current(ctx)) {}
        ~Scope() { set_current(previous_); }

        // Non-copyable
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        RuntimeContext* previous_;
    };

    // ========================================================================
    // Garbage Collection
    // ========================================================================

    /**
     * @brief Get the garbage collector
     * @return GC instance for this context
     */
    IGarbageCollector* gc() const;

    /**
     * @brief Trigger garbage collection
     */
    void collect_garbage();

    // ========================================================================
    // Symbol Table
    // ========================================================================

    /**
     * @brief Intern a symbol (get or create unique symbol for name)
     * @param name Symbol name
     * @return Interned symbol (pointer equality guaranteed for same name)
     */
    a_symbol* intern(const std::string& name);

    /**
     * @brief Check if a symbol is interned
     * @param name Symbol name
     * @return true if symbol exists in table
     */
    bool is_interned(const std::string& name) const;

    // Note: I/O ports are NOT held here. The global stdin_port / stdout_port /
    // stderr_port are the single source of truth the primitives read; the engine
    // owns the port lifetimes. (A former per-context port shadow was write-only
    // dead state and was removed.)

    // ========================================================================
    // Library System
    // ========================================================================

    /**
     * @brief Get the R7RS library registry
     * @return Library registry instance
     */
    r7rs::LibraryRegistry& libraries() const;

    // ========================================================================
    // Singleton Values (nil, #t, #f)
    // ========================================================================

    /**
     * @brief Get the empty list singleton
     * @return '() / nil
     */
    Object* empty_list() const;

    /**
     * @brief Get the true singleton
     * @return #t
     */
    Object* true_value() const;

    /**
     * @brief Get the false singleton
     * @return #f
     */
    Object* false_value() const;

    // ========================================================================
    // Initialization State
    // ========================================================================

    /**
     * @brief Check if context is fully initialized
     * @return true if all subsystems ready
     */
    bool is_initialized() const { return initialized_; }

    /**
     * @brief Ensure constants (nil, #t, #f) are initialized
     *
     * Called automatically during construction, but can be called
     * explicitly if needed.
     */
    void ensure_constants();

private:
    // ========================================================================
    // Private Implementation
    // ========================================================================

    // Initialization flag
    bool initialized_ = false;
};

// ============================================================================
// Convenience Functions (use current context)
// ============================================================================

namespace rt {

/**
 * @brief Intern a symbol using current context
 * @param name Symbol name
 * @return Interned symbol
 */
inline a_symbol* intern(const std::string& name) {
    return RuntimeContext::current()->intern(name);
}

} // namespace rt
