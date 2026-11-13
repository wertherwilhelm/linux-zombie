// REPL-test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <string>
#include <memory>

#include "repl_core.hpp"
#include "repl_smart_edit.hpp"
#include "scheme_engine.h"
#include "SimpleDebugger.h"
#include "bytecode_vm.h"

static void emit_events(const std::vector<ReplEvent>& ev) {
    for (const auto& e : ev) {
        switch (e.kind) {
        case ReplEvent::Kind::Prompt:
            if (!e.text.empty()) std::cout << e.text << std::flush;
            break;
        case ReplEvent::Kind::Stdout:
        case ReplEvent::Kind::Result:
        case ReplEvent::Kind::Info:
            std::cout << e.text << "\n";
            break;
        case ReplEvent::Kind::Stderr:
            std::cerr << e.text << "\n";
            break;
        }
    }
}

static std::string format_meta_prefixes(const ReplConfig& cfg) {
    std::string out;
    for (size_t i = 0; i < cfg.meta_prefixes.size(); ++i) {
        if (i > 0) out.append(", ");
        out.append(cfg.meta_prefixes[i]);
    }
    return out;
}

static std::string primary_meta_prefix(const ReplConfig& cfg) {
    if (cfg.meta_prefixes.empty()) return {};
    return cfg.meta_prefixes.front();
}

int main(int argc, char** argv) {
    // Parse command-line arguments
    EvaluatorType eval_type = EvaluatorType::LISP;  // Default to Lisp
    bool show_help = false;
    bool enable_debugger = false;
    ReplConfig cfg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--vm" || arg == "-v") {
            eval_type = EvaluatorType::TOY_VM;
        } else if (arg == "--Lisp" || arg == "-l") {
            eval_type = EvaluatorType::LISP;
        } else if (arg == "--bytecode" || arg == "-b") {
            eval_type = EvaluatorType::BYTECODE;
        } else if (arg == "--debug" || arg == "-d") {
            enable_debugger = true;
        } else if (arg == "--help" || arg == "-h") {
            show_help = true;
        }
    }

    if (show_help) {
        auto meta_prefix = primary_meta_prefix(cfg);
        auto meta_prefix_list = format_meta_prefixes(cfg);
        std::cout << "Zombie Scheme REPL v0.0.1\n";
        std::cout << "Usage: REPL-test [options]\n";
        std::cout << "\nOptions:\n";
        std::cout << "  -l, --Lisp       Use traditional Lisp evaluator (default)\n";
        std::cout << "  -v, --vm         Use toy VM evaluator (experimental)\n";
        std::cout << "  -b, --bytecode   Use bytecode compiler/VM evaluator\n";
        std::cout << "  -d, --debug      Enable debugger (requires --bytecode)\n";
        std::cout << "  -h, --help       Show this help message\n";
        std::cout << "\nExit commands:\n";
        std::cout << "  quit             Exit the REPL\n";
        std::cout << "  exit             Exit the REPL\n";
        std::cout << "  (quit)           Exit the REPL (Scheme form)\n";
        std::cout << "  (exit)           Exit the REPL (Scheme form)\n";
        if (!meta_prefix_list.empty()) {
            std::cout << "\nMeta commands (prefixes: " << meta_prefix_list << "):\n";
        } else {
            std::cout << "\nMeta commands:\n";
        }
        std::cout << "  " << meta_prefix << "help [cmd]      List meta commands or details for one command\n";
        std::cout << "  " << meta_prefix << "quit, " << meta_prefix << "q        Exit the REPL\n";
        std::cout << "  " << meta_prefix << "reset           Reset interpreter state\n";
        std::cout << "  " << meta_prefix << "clear           Clear input buffer\n";
        std::cout << "  " << meta_prefix << "format, " << meta_prefix << "fmt    Reformat current input buffer\n";
        std::cout << "  " << meta_prefix << "close           Auto-close open parens in buffer\n";
        std::cout << "\nDebugger commands (when enabled):\n";
        std::cout << "  (debug-break)           Trigger a breakpoint\n";
        std::cout << "  (debug-step)            Enable step mode\n";
        std::cout << "  (debug-continue)        Resume execution\n";
        std::cout << "  (debug-backtrace)       Print call stack\n";
        std::cout << "  (debug-breakpoint loc)  Add breakpoint at location\n";
        return 0;
    }

    // Display evaluator type
    const char* eval_name = "Unknown";
    switch (eval_type) {
        case EvaluatorType::LISP:     eval_name = "Lisp"; break;
        case EvaluatorType::TOY_VM:   eval_name = "Toy VM"; break;
        case EvaluatorType::BYTECODE: eval_name = "Bytecode VM"; break;
    }
    std::cout << "Zombie Scheme REPL v0.0.1 - Using " << eval_name << " evaluator";
    if (enable_debugger) {
        std::cout << " with debugger";
    }
    std::cout << "\n";

    // scheme_engine now directly implements IEvalEngine
    auto engine = std::make_unique<scheme_engine>(eval_type);
    // Enable debugger if requested (only works with bytecode VM)
    std::shared_ptr<SimpleDebugger> debugger;
    if (enable_debugger) {
        if (eval_type == EvaluatorType::BYTECODE) {
            debugger = std::make_shared<SimpleDebugger>();
            zombie::bytecode::BytecodeVM* bc_vm = dynamic_cast<zombie::bytecode::BytecodeVM*>(engine->get_evaluator());
            if (bc_vm) {
                bc_vm->set_debugger(debugger.get());
                std::cout << "Debugger enabled - use (debug-break) to trigger breakpoints\n";
            } else {
                std::cerr << "Warning: Could not enable debugger (not a BytecodeVM)\n";
            }
        } else {
            std::cerr << "Warning: Debugger only works with --bytecode evaluator\n";
        }
    }

    ReplSession repl(std::move(engine), cfg);
    repl.add_input_plugin(std::make_unique<LispSmartEditPlugin>());

    emit_events(repl.start());

    std::string line;
    while (std::getline(std::cin, line)) {
        auto ev = repl.submit_line(line);

        // crude exit: if prompt event is empty after :quit
        bool exiting = false;
        for (auto& e : ev) if (e.kind == ReplEvent::Kind::Prompt && e.text.empty()) exiting = true;

        emit_events(ev);
        if (exiting) break;
    }
    return 0;
}
