// REPL-test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <vector>
#include <string>
#include <memory>

#include "repl_core.hpp"
#include "repl_terminal.hpp"
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
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
    bool plain = false;
    bool enable_debugger = false;
    ReplConfig cfg;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--plain") {
            plain = true;
        } else if (arg == "--vm" || arg == "-v") {
            eval_type = EvaluatorType::TOY_VM;
        } else if (arg == "--Lisp" || arg == "-l") {
            eval_type = EvaluatorType::LISP;
        } else if (arg == "--bytecode" || arg == "-b") {
            eval_type = EvaluatorType::BYTECODE;
        } else if (arg == "--coreir" || arg == "-c") {
            eval_type = EvaluatorType::COREIR;
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
        std::cout << "Usage: repl [options]\n";
        std::cout << "\nOptions:\n";
        std::cout << "  -l, --Lisp       Use traditional Lisp evaluator (default)\n";
        std::cout << "  -v, --vm         Use toy VM evaluator (experimental)\n";
        std::cout << "  -b, --bytecode   Use bytecode compiler/VM evaluator\n";
        std::cout << "  -c, --coreir     Use CoreIR -> bytecode pipeline evaluator\n";
        std::cout << "  -d, --debug      Enable debugger (requires --bytecode)\n";
        std::cout << "      --plain      Disable terminal editing (also automatic for pipes)\n";
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
        case EvaluatorType::COREIR:   eval_name = "CoreIR"; break;
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

    auto* live_engine = engine.get();
    ReplSession repl(std::move(engine), cfg);
    repl.add_input_plugin(std::make_unique<LispSmartEditPlugin>());

#ifdef _WIN32
    bool interactive = !plain && _isatty(_fileno(stdin)) && _isatty(_fileno(stdout));
#else
    bool interactive = !plain && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#endif
    std::unique_ptr<ReplTerminal> terminal;
    if (interactive) {
        terminal = std::make_unique<ReplTerminal>(*live_engine);
        std::cout << "Tab: complete | Ctrl-R: history | Ctrl-C: cancel | Ctrl-D: exit | :help\n";
    }
    std::string prompt = cfg.prompt;
    std::string line;
    for (;;) {
        if (terminal) {
            bool cancelled = false;
            if (!terminal->read(prompt, line, cancelled)) break;
            if (cancelled) {
                repl.clear_buffer();
                prompt = cfg.prompt;
                continue;
            }
        } else {
            std::cout << prompt << std::flush;
            if (!std::getline(std::cin, line)) break;
        }
        auto events = repl.submit_line(line);
        bool exiting = false;
        for (const auto& event : events) {
            if (event.kind == ReplEvent::Kind::Prompt) {
                prompt = event.text;
                if (prompt.empty()) exiting = true;
            } else {
                emit_events({event});
            }
        }
        if (exiting) break;
    }
    return 0;
}
