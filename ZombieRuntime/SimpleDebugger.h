#pragma once

#include "i_debugger.h"
#include "scheme_reader.h"
#include "Lisp.h"
#include "zombie_runtime_types.h"  // For namespace using declarations
#include <iostream>
#include <sstream>
#include <algorithm>

// ============================================================================
// SimpleDebugger - Basic interactive debugger
// ============================================================================

class SimpleDebugger : public IDebugger {
public:
    enum DebugMode {
        RUNNING,        // Running normally
        STEPPING,       // Step mode (break at every instruction)
        STEP_OVER,      // Step over (don't enter calls)
        STEP_OUT,       // Step out (return from function)
        PAUSED          // Paused at breakpoint
    };

    SimpleDebugger()
        : mode_(RUNNING)
        , step_depth_(0)
        , target_depth_(-1)
        , saved_ctx_()
    {}

    // ========================================================================
    // Debugger Hooks
    // ========================================================================

    bool on_step(const DebugContext& ctx) override {
        // Check if we should break
        if (mode_ == STEPPING) {
            enter_debug_prompt(ctx);
            return false;  // Pause
        }

        if (mode_ == STEP_OVER) {
            // Only break if we're at same depth or shallower
            if (ctx.call_stack.size() <= static_cast<size_t>(target_depth_)) {
                mode_ = STEPPING;
                enter_debug_prompt(ctx);
                return false;
            }
        }

        if (mode_ == STEP_OUT) {
            // Only break if we're shallower than target
            if (ctx.call_stack.size() < static_cast<size_t>(target_depth_)) {
                mode_ = STEPPING;
                enter_debug_prompt(ctx);
                return false;
            }
        }

        // Check breakpoints
        if (should_break_here(ctx)) {
            // Find which breakpoint was hit
            for (auto& bp : breakpoints_) {
                if (bp.enabled && matches_breakpoint(bp, ctx)) {
                    bp.hit_count++;
                    if (bp.hit_count > bp.ignore_count) {
                        on_breakpoint(bp, ctx);
                        return false;  // Pause
                    }
                }
            }
        }

        return true;  // Continue
    }

    void on_breakpoint(const Breakpoint& bp, const DebugContext& ctx) override {
        std::cout << "\n🛑 Breakpoint hit: " << bp.location << "\n";
        std::cout << "   Hit count: " << bp.hit_count << "\n";
        mode_ = PAUSED;
        enter_debug_prompt(ctx);
    }

    void on_error(const std::string& error, const DebugContext& ctx) override {
        std::cout << "\n❌ Error: " << error << "\n";
        mode_ = PAUSED;
        print_context(ctx);
        enter_debug_prompt(ctx);
    }

    void on_return(Object* result, const DebugContext& ctx) override {
        // Optional: log returns
        if (mode_ == STEPPING) {
            std::cout << "← Return: ";
            if (result) {
                std::cout << result << "\n";
            } else {
                std::cout << "<void>\n";
            }
        }
    }

    void on_function_entry(const std::string& name, const DebugContext& ctx) override {
        // Optional: log function entries
        if (mode_ == STEPPING) {
            std::cout << "→ Enter: " << name << "\n";
        }
    }

    // ========================================================================
    // Query Interface
    // ========================================================================

    bool should_break_here(const DebugContext& ctx) const override {
        for (const auto& bp : breakpoints_) {
            if (bp.enabled && matches_breakpoint(bp, ctx)) {
                return true;
            }
        }
        return false;
    }

    bool is_stepping() const override {
        return mode_ == STEPPING;
    }

    bool is_paused() const override {
        return mode_ == PAUSED;
    }

    // ========================================================================
    // Control Interface
    // ========================================================================

    void resume() override {
        mode_ = RUNNING;
    }

    void step() override {
        mode_ = STEPPING;
    }

    void step_over() override {
        mode_ = STEP_OVER;
        target_depth_ = static_cast<int>(saved_ctx_.call_stack.size());
    }

    void step_out() override {
        mode_ = STEP_OUT;
        target_depth_ = static_cast<int>(saved_ctx_.call_stack.size());
    }

    // ========================================================================
    // Breakpoint Management
    // ========================================================================

    void add_breakpoint(const Breakpoint& bp) override {
        breakpoints_.push_back(bp);
    }

    void remove_breakpoint(const std::string& location) override {
        breakpoints_.erase(
            std::remove_if(breakpoints_.begin(), breakpoints_.end(),
                [&](const Breakpoint& bp) { return bp.location == location; }),
            breakpoints_.end()
        );
    }

    void enable_breakpoint(const std::string& location) override {
        for (auto& bp : breakpoints_) {
            if (bp.location == location) {
                bp.enabled = true;
            }
        }
    }

    void disable_breakpoint(const std::string& location) override {
        for (auto& bp : breakpoints_) {
            if (bp.location == location) {
                bp.enabled = false;
            }
        }
    }

    std::vector<Breakpoint> get_breakpoints() const override {
        return breakpoints_;
    }

    // ========================================================================
    // Inspection Interface
    // ========================================================================

    std::vector<DebugFrame> get_call_stack() const override {
        return saved_ctx_.call_stack;
    }

    std::vector<std::pair<std::string, Object*>> get_locals(int frame_index) const override {
        if (frame_index < 0 || frame_index >= static_cast<int>(saved_ctx_.call_stack.size())) {
            return {};
        }

        const DebugFrame& frame = saved_ctx_.call_stack[frame_index];
        std::vector<std::pair<std::string, Object*>> locals;

        if (frame.env) {
            // Walk the environment and collect bindings
            // This is simplified - real implementation would need to walk frames
            // For now, just return empty (would need Environment API extension)
        }

        return locals;
    }

    Object* eval_in_frame(const std::string& expr, int frame_index) override {
        if (frame_index < 0 || frame_index >= static_cast<int>(saved_ctx_.call_stack.size())) {
            return nil;
        }

        const DebugFrame& frame = saved_ctx_.call_stack[frame_index];

        try {
            // Parse the expression
            scheme_reader reader;
            Object* parsed = reader.read_from_string(expr);

            // Evaluate in the frame's environment
            if (saved_ctx_.vm) {
                // Use bytecode VM
                Lisp evaluator;
                return evaluator.new_eval(parsed, frame.env);
            }
        } catch (...) {
            return nil;
        }

        return nil;
    }

private:
    DebugMode mode_;
    int step_depth_;
    int target_depth_;
    std::vector<Breakpoint> breakpoints_;
    DebugContext saved_ctx_;

    bool matches_breakpoint(const Breakpoint& bp, const DebugContext& ctx) const {
        switch (bp.type) {
            case Breakpoint::SOURCE_LOCATION:
                return ctx.source_file + ":" + std::to_string(ctx.source_line) == bp.location;

            case Breakpoint::FUNCTION_ENTRY:
                if (!ctx.call_stack.empty()) {
                    return ctx.call_stack.back().function_name == bp.location;
                }
                return false;

            case Breakpoint::INSTRUCTION:
                return std::to_string(ctx.instruction_pointer) == bp.location;

            case Breakpoint::CONDITIONAL:
                if (bp.condition) {
                    return bp.condition(ctx);
                }
                return false;

            default:
                return false;
        }
    }

    void enter_debug_prompt(const DebugContext& ctx) {
        saved_ctx_ = ctx;
        print_context(ctx);

        while (true) {
            std::cout << "\n(debug) ";
            std::string cmd;
            std::getline(std::cin, cmd);

            // Trim whitespace
            cmd.erase(0, cmd.find_first_not_of(" \t"));
            cmd.erase(cmd.find_last_not_of(" \t") + 1);

            if (cmd.empty()) continue;

            // Parse command
            if (cmd == "c" || cmd == "continue") {
                resume();
                break;
            } else if (cmd == "s" || cmd == "step") {
                step();
                break;
            } else if (cmd == "n" || cmd == "next") {
                step_over();
                break;
            } else if (cmd == "o" || cmd == "out") {
                step_out();
                break;
            } else if (cmd == "bt" || cmd == "backtrace") {
                print_backtrace(ctx);
            } else if (cmd == "l" || cmd == "list") {
                print_context(ctx);
            } else if (cmd == "b" || cmd == "break") {
                list_breakpoints();
            } else if (cmd.starts_with("b ")) {
                // Add breakpoint
                std::string location = cmd.substr(2);
                add_breakpoint(Breakpoint(Breakpoint::SOURCE_LOCATION, location));
                std::cout << "Breakpoint added: " << location << "\n";
            } else if (cmd.starts_with("p ")) {
                // Print expression
                std::string expr = cmd.substr(2);
                Object* result = eval_in_frame(expr, 0);
                std::cout << result << "\n";
            } else if (cmd == "h" || cmd == "help") {
                print_help();
            } else if (cmd == "q" || cmd == "quit") {
                std::cout << "Exiting debugger...\n";
                mode_ = RUNNING;
                break;
            } else {
                std::cout << "Unknown command: " << cmd << " (type 'h' for help)\n";
            }
        }
    }

    void print_context(const DebugContext& ctx) const {
        std::cout << "\n📍 Debug Context:\n";
        std::cout << "   IP: " << ctx.instruction_pointer << "\n";
        std::cout << "   Depth: " << ctx.call_stack.size() << "\n";

        if (!ctx.call_stack.empty()) {
            const auto& frame = ctx.call_stack.back();
            std::cout << "   Function: " << frame.function_name << "\n";
            if (!frame.source_location.empty() && frame.source_location != "<unknown>") {
                std::cout << "   Location: " << frame.source_location << "\n";
            }
        }

        if (ctx.value_stack && !ctx.value_stack->empty()) {
            std::cout << "   Stack top: " << ctx.value_stack->back() << "\n";
        }
    }

    void print_backtrace(const DebugContext& ctx) const {
        std::cout << "\n📚 Call Stack:\n";
        for (size_t i = 0; i < ctx.call_stack.size(); i++) {
            const auto& frame = ctx.call_stack[i];
            std::cout << "  #" << i << " " << frame.function_name;
            if (!frame.source_location.empty() && frame.source_location != "<unknown>") {
                std::cout << " at " << frame.source_location;
            }
            std::cout << "\n";
        }
    }

    void list_breakpoints() const {
        if (breakpoints_.empty()) {
            std::cout << "No breakpoints set.\n";
            return;
        }

        std::cout << "\n🎯 Breakpoints:\n";
        for (size_t i = 0; i < breakpoints_.size(); i++) {
            const auto& bp = breakpoints_[i];
            std::cout << "  #" << i << " " << bp.location;
            if (!bp.enabled) {
                std::cout << " (disabled)";
            }
            if (bp.hit_count > 0) {
                std::cout << " [hit " << bp.hit_count << " times]";
            }
            std::cout << "\n";
        }
    }

    void print_help() const {
        std::cout << "\n📖 Debugger Commands:\n";
        std::cout << "  c, continue     - Continue execution\n";
        std::cout << "  s, step         - Step to next instruction\n";
        std::cout << "  n, next         - Step over (don't enter calls)\n";
        std::cout << "  o, out          - Step out (return from function)\n";
        std::cout << "  bt, backtrace   - Show call stack\n";
        std::cout << "  l, list         - Show current context\n";
        std::cout << "  b, break        - List breakpoints\n";
        std::cout << "  b <location>    - Add breakpoint\n";
        std::cout << "  p <expr>        - Evaluate expression\n";
        std::cout << "  h, help         - Show this help\n";
        std::cout << "  q, quit         - Exit debugger\n";
    }
};
