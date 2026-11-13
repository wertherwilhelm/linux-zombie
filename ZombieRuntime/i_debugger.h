#pragma once

#include "Object.h"
#include "Environment.h"
#include <vector>
#include <string>
#include <functional>
#include <memory>

// Forward declarations
namespace zombie { namespace bytecode {
    class BytecodeVM;
} } // namespace zombie::bytecode

namespace zombie { namespace runtime {

// ============================================================================
// Debug Context - Snapshot of execution state
// ============================================================================

/**
 * @brief Single stack frame snapshot for debugging.
 */
struct DebugFrame {
    std::string function_name;      ///< Name of function being executed
    Environment* env;               ///< Lexical environment
    int instruction_pointer;        ///< Current instruction (for bytecode)
    Object* current_expr;           ///< Current expression (for tree-walking)
    std::string source_location;    ///< File:line (if available)

    DebugFrame()
        : function_name("<unknown>")
        , env(nullptr)
        , instruction_pointer(-1)
        , current_expr(nullptr)
        , source_location("<unknown>")
    {}
};

/**
 * @brief Snapshot of execution state for debugger callbacks.
 */
struct DebugContext {
    // Current execution state
    Object* current_expr;               ///< Expression being evaluated
    Environment* current_env;           ///< Current environment
    std::vector<DebugFrame> call_stack;

    // Bytecode VM specific (nullptr for other evaluators)
    int instruction_pointer;
    std::vector<Object*>* value_stack;
    zombie::bytecode::BytecodeVM* vm;

    // Source location (if available)
    std::string source_file;
    int source_line;

    DebugContext()
        : current_expr(nullptr)
        , current_env(nullptr)
        , instruction_pointer(-1)
        , value_stack(nullptr)
        , vm(nullptr)
        , source_line(-1)
    {}
};

// ============================================================================
// Breakpoint - Condition for pausing execution
// ============================================================================

/**
 * @brief Breakpoint definition and hit tracking.
 */
struct Breakpoint {
    enum Type {
        SOURCE_LOCATION,    ///< Break at source file:line
        FUNCTION_ENTRY,     ///< Break on function entry
        INSTRUCTION,        ///< Break at instruction address
        CONDITIONAL         ///< Break when condition is true
    };

    Type type;
    std::string location;          ///< "file.scm:42" or function name
    bool enabled;
    int hit_count;                 ///< Number of times hit
    int ignore_count;              ///< Hit N times before breaking

    // Conditional breakpoint
    std::function<bool(const DebugContext&)> condition;

    Breakpoint()
        : type(SOURCE_LOCATION)
        , enabled(true)
        , hit_count(0)
        , ignore_count(0)
    {}

    Breakpoint(Type t, const std::string& loc)
        : type(t)
        , location(loc)
        , enabled(true)
        , hit_count(0)
        , ignore_count(0)
    {}
};

// ============================================================================
// IDebugger - Abstract debugger interface
// ============================================================================

/**
 * @brief Abstract debugger interface for evaluator instrumentation.
 */
class IDebugger {
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~IDebugger() = default;

    // ========================================================================
    // Debugger Hooks (called by evaluator)
    // ========================================================================

    /**
     * @brief Called before executing each instruction/expression.
     * @param ctx Current execution context.
     * @return true to continue, false to pause execution.
     */
    virtual bool on_step(const DebugContext& ctx) = 0;

    /**
     * @brief Called when a breakpoint is hit.
     * @param bp Breakpoint that triggered.
     * @param ctx Current execution context.
     */
    virtual void on_breakpoint(const Breakpoint& bp, const DebugContext& ctx) = 0;

    /**
     * @brief Called when an error occurs.
     * @param error Error message.
     * @param ctx Current execution context.
     */
    virtual void on_error(const std::string& error, const DebugContext& ctx) = 0;

    /**
     * @brief Called when a function returns.
     * @param result Returned Object.
     * @param ctx Current execution context.
     */
    virtual void on_return(Object* result, const DebugContext& ctx) = 0;

    /**
     * @brief Called when entering a function.
     * @param name Function name.
     * @param ctx Current execution context.
     */
    virtual void on_function_entry(const std::string& name, const DebugContext& ctx) = 0;

    // ========================================================================
    // Query Interface
    // ========================================================================

    /**
     * @brief Check if debugger wants to break at this location.
     * @param ctx Current execution context.
     * @return true if should break.
     */
    virtual bool should_break_here(const DebugContext& ctx) const = 0;

    /**
     * @brief Check if currently in stepping mode.
     */
    virtual bool is_stepping() const = 0;

    /**
     * @brief Check if currently paused.
     */
    virtual bool is_paused() const = 0;

    // ========================================================================
    // Control Interface
    // ========================================================================

    /**
     * @brief Continue execution.
     */
    virtual void resume() = 0;

    /**
     * @brief Step to next instruction/expression.
     */
    virtual void step() = 0;

    /**
     * @brief Step over (don't enter function calls).
     */
    virtual void step_over() = 0;

    /**
     * @brief Step out (return from current function).
     */
    virtual void step_out() = 0;

    // ========================================================================
    // Breakpoint Management
    // ========================================================================

    /**
     * @brief Add a breakpoint.
     */
    virtual void add_breakpoint(const Breakpoint& bp) = 0;
    /**
     * @brief Remove a breakpoint by location key.
     */
    virtual void remove_breakpoint(const std::string& location) = 0;
    /**
     * @brief Enable a breakpoint by location key.
     */
    virtual void enable_breakpoint(const std::string& location) = 0;
    /**
     * @brief Disable a breakpoint by location key.
     */
    virtual void disable_breakpoint(const std::string& location) = 0;
    /**
     * @brief Get all configured breakpoints.
     */
    virtual std::vector<Breakpoint> get_breakpoints() const = 0;

    // ========================================================================
    // Inspection Interface
    // ========================================================================

    /**
     * @brief Get current call stack.
     */
    virtual std::vector<DebugFrame> get_call_stack() const = 0;

    /**
     * @brief Get local variables in a given frame.
     * @param frame_index Frame index (0 = current frame).
     */
    virtual std::vector<std::pair<std::string, Object*>> get_locals(int frame_index = 0) const = 0;

    /**
     * @brief Evaluate expression in context of a frame.
     * @param expr Expression to evaluate.
     * @param frame_index Frame index (0 = current frame).
     */
    virtual Object* eval_in_frame(const std::string& expr, int frame_index = 0) = 0;
};

// ============================================================================
// NullDebugger - No-op debugger (default)
// ============================================================================

/**
 * @brief No-op debugger implementation used as default.
 */
class NullDebugger : public IDebugger {
public:
    bool on_step(const DebugContext&) override { return true; }
    void on_breakpoint(const Breakpoint&, const DebugContext&) override {}
    void on_error(const std::string&, const DebugContext&) override {}
    void on_return(Object*, const DebugContext&) override {}
    void on_function_entry(const std::string&, const DebugContext&) override {}

    bool should_break_here(const DebugContext&) const override { return false; }
    bool is_stepping() const override { return false; }
    bool is_paused() const override { return false; }

    void resume() override {}
    void step() override {}
    void step_over() override {}
    void step_out() override {}

    void add_breakpoint(const Breakpoint&) override {}
    void remove_breakpoint(const std::string&) override {}
    void enable_breakpoint(const std::string&) override {}
    void disable_breakpoint(const std::string&) override {}
    std::vector<Breakpoint> get_breakpoints() const override { return {}; }

    std::vector<DebugFrame> get_call_stack() const override { return {}; }
    std::vector<std::pair<std::string, Object*>> get_locals(int) const override { return {}; }
    Object* eval_in_frame(const std::string&, int) override { return nil; }
};

// Global default debugger (no-op)
extern std::shared_ptr<IDebugger> g_default_debugger;

} } // namespace zombie::runtime
