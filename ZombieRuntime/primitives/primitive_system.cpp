#include "zombie_runtime_types.h"
#include "primitive_system.h"
#include "../native_primitive.h"
#include "../Base.h"
#include "../atom.h"
#include "../cons_cell.h"
#include "../../GarbageCollector/gc_ptr.h"
#include <cstdlib>
#include <vector>
#include <string>

/**
 * @file primitive_system.cpp
 * @brief System interface primitives (R7RS 6.14), table-driven.
 */

// Process argv, for (command-line).
static std::vector<std::string> g_command_line_args;

void set_command_line_args(int argc, char* argv[]) {
    g_command_line_args.clear();
    for (int i = 0; i < argc; i++) {
        g_command_line_args.push_back(argv[i]);
    }
}

namespace {

using namespace zombie::runtime;

static int exit_code_from(Object* args) {
    if (args && args != nil) {
        Object* x = first(args);
        if (x && x != nil && is_int(x)) return (int)num(x);
    }
    return 0;
}

// (exit [obj]) — normal exit with status (default 0).
Object* prim_exit(IEvaluator*, Object* args, Environment*) {
    std::exit(exit_code_from(args));
    return nil;  // never reached
}

// (emergency-exit [obj]) — immediate termination, bypassing cleanup.
Object* prim_emergency_exit(IEvaluator*, Object* args, Environment*) {
    std::_Exit(exit_code_from(args));
    return nil;  // never reached
}

// (command-line) — list of argv strings (program name first).
Object* prim_command_line(IEvaluator*, Object*, Environment*) {
    Object* result = nil;
    for (auto it = g_command_line_args.rbegin(); it != g_command_line_args.rend(); ++it) {
        result = gc_new<ConsCell>(gc_new<a_string>(*it), result);
    }
    return result;
}

// (features) — implementation feature identifiers (R7RS 6.14).
Object* prim_features(IEvaluator*, Object*, Environment*) {
    const char* feature_list[] = {
#ifdef _WIN32
        "windows",
#endif
#ifdef __linux__
        "posix",
        "unix",
#endif
        "complex", "ratios", "exact-closed", "ieee-float", "exact-complex",
        "r7rs", "zombie-scheme",
        nullptr
    };
    int count = 0;
    while (feature_list[count] != nullptr) count++;
    Object* result = nil;
    for (int i = count - 1; i >= 0; i--) {
        result = gc_new<ConsCell>(gc_new<a_string>(feature_list[i]), result);
    }
    return result;
}

// (void) — an unspecified value.
Object* prim_void(IEvaluator*, Object*, Environment*) {
    return nil;
}

}  // namespace

void PrimitiveSystem::install(zombie::runtime::Environment* env) {
    using namespace zombie::runtime;
    static const NativeEntry entries[] = {
        { "exit",           0, 1, prim_exit           },
        { "emergency-exit", 0, 1, prim_emergency_exit },
        { "command-line",   0, 0, prim_command_line   },
        { "features",       0, 0, prim_features       },
        { "void",           0, 0, prim_void           },
    };
    install_natives(env, entries, sizeof(entries) / sizeof(entries[0]));
}
