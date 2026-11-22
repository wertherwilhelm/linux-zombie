// Batch entry point: program source comes from a file; stdin remains Scheme data.
#include "scheme_engine.h"
#include "native_primitive.h"
#include "numeric_runtime.h"
#include "library_registry.h"
#include "primitives/primitive_system.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {
// Benchmark adapter only: monotonic microseconds for elapsed time. The secondary
// current-second clock is POSIX wall time, not a leap-second-aware TAI clock.
Object* current_jiffy(IEvaluator*, Object*, Environment*) {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return numeric::make_integer(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}
Object* jiffies_per_second(IEvaluator*, Object*, Environment*) { return numeric::make_integer(1000000); }
Object* current_second(IEvaluator*, Object*, Environment*) {
    return numeric::make_inexact(std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}
void install_benchmark_time(Environment* env) {
    const zombie::runtime::NativeEntry entries[] = {
        {"current-jiffy", 0, 0, current_jiffy},
        {"jiffies-per-second", 0, 0, jiffies_per_second},
        {"current-second", 0, 0, current_second}
    };
    zombie::runtime::install_natives(env, entries, std::size(entries));
    auto library = make_gc_ptr<r7rs::Library>(r7rs::LibraryName{"scheme", "time"});
    for (const auto& entry : entries) library->add_export(entry.name);
    std::string error;
    if (!library->initialize(env, error)) throw std::runtime_error(error);
    library->set_initialized(true);
    r7rs::LibraryRegistry::instance().register_library(std::move(library));
}
}

int main(int argc, char** argv) {
    EvaluatorType evaluator = EvaluatorType::LISP;
    bool benchmark_time = false;
    std::string file;
    int program_index = argc;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: zombie-scheme [--evaluator lisp|bytecode|coreir|toyvm] "
                         "[--benchmark-time] FILE [program arguments...]\n"
                         "Execute FILE without a REPL; stdin remains available to (read).\n"
                         "--benchmark-time installs the benchmark-only (scheme time) clock adapter.\n";
            return 0;
        }
        if (arg == "--benchmark-time") { benchmark_time = true; continue; }
        if (arg == "--evaluator") {
            if (++i == argc) { std::cerr << "error: --evaluator needs a name\n"; return 2; }
            std::string name = argv[i];
            if (name == "lisp") evaluator = EvaluatorType::LISP;
            else if (name == "bytecode") evaluator = EvaluatorType::BYTECODE;
            else if (name == "coreir") evaluator = EvaluatorType::COREIR;
            else if (name == "toyvm") evaluator = EvaluatorType::TOY_VM;
            else { std::cerr << "error: unknown evaluator: " << name << '\n'; return 2; }
            continue;
        }
        if (arg == "--") {
            if (++i == argc) { std::cerr << "error: missing program file\n"; return 2; }
            file = argv[i]; program_index = i; break;
        }
        if (!arg.empty() && arg.front() == '-') {
            std::cerr << "error: unknown option: " << arg << '\n'; return 2;
        }
        file = arg; program_index = i; break;
    }
    if (file.empty()) { std::cerr << "error: missing program file (see --help)\n"; return 2; }
    std::ifstream input(file, std::ios::binary);
    if (!input) { std::cerr << "error: cannot open program: " << file << '\n'; return 2; }
    std::string source{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (input.bad()) { std::cerr << "error: cannot read program: " << file << '\n'; return 2; }
    try {
        set_command_line_args(argc - program_index, argv + program_index);
        scheme_engine engine(evaluator);
        if (benchmark_time) install_benchmark_time(engine.get_environment());
        auto* result = engine.eval_string(source);
        if (!result && !engine.get_last_error().empty()) {
            std::cerr << file << ": " << engine.get_last_error() << '\n';
            return 1;
        }
        std::cout.flush();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << file << ": " << error.what() << '\n';
        return 1;
    }
}
