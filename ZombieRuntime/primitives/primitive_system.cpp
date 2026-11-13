#include "pch.h"
#include "../pch.h"
#include "primitive_system.h"
#include "../Base.h"
#include "../atom.h"
#include "../cons_cell.h"
#include "../../GarbageCollector/gc_ptr.h"
#include <cstdlib>

// Global to store command line arguments
static std::vector<std::string> g_command_line_args;

void set_command_line_args(int argc, char* argv[]) {
    g_command_line_args.clear();
    for (int i = 0; i < argc; i++) {
        g_command_line_args.push_back(argv[i]);
    }
}

// ============================================================================
// Construction
// ============================================================================

PrimitiveSystem::PrimitiveSystem(ID id, int min_args, int max_args)
    : PrimitiveBase(min_args, max_args)
    , m_id(id)
{
    // Set Primitive name based on ID
    switch (id) {
        case EXIT:          m_name = "exit"; break;
        case EMERGENCYEXIT: m_name = "emergency-exit"; break;
        case COMMANDLINE:   m_name = "command-line"; break;
        case FEATURES:      m_name = "features"; break;
        case VOID:          m_name = "void"; break;
    }
}

// ============================================================================
// Apply
// ============================================================================

Object* PrimitiveSystem::apply(IEvaluator* eval, Object* args, Environment* env) {
    // Extract first argument if present
    Object* x = nullptr;
    if (args && args != nil) {
        x = first(args);
    }

    switch (m_id) {
        case EXIT: {
            // (exit) or (exit obj)
            // Exit the program with status code
            // If obj is provided and is an integer, use it as exit code
            // Otherwise exit with 0
            int exit_code = 0;
            if (x != nullptr && x != nil) {
                if (is_int(x)) {
                    exit_code = (int)num(x);
                }
            }
            std::exit(exit_code);
            // Never returns
            return nil;
        }

        case EMERGENCYEXIT: {
            // (emergency-exit) or (emergency-exit obj)
            // Emergency exit - similar to exit but bypasses cleanup
            int exit_code = 0;
            if (x != nullptr && x != nil) {
                if (is_int(x)) {
                    exit_code = (int)num(x);
                }
            }
            // Use _Exit for immediate termination without cleanup
            std::_Exit(exit_code);
            // Never returns
            return nil;
        }

        case COMMANDLINE: {
            // (command-line)
            // Returns a list of strings representing the command line arguments
            // First element is typically the program name
            if (g_command_line_args.empty()) {
                // Return empty list if no args set
                return nil;
            }

            // Build list of command line arguments
            Object* result = nil;

            // Build in reverse order so we can cons from front
            for (auto it = g_command_line_args.rbegin(); it != g_command_line_args.rend(); ++it) {
                result = gc_new<ConsCell>(
                    gc_new<a_string>(*it),
                    result
                );
            }

            return result;
        }

        case FEATURES: {
            // (features)
            // Returns a list of symbols identifying features supported by this implementation
            // R7RS Section 6.14

            // Build list of feature identifiers
            Object* result = nil;

            // Platform features (build in reverse order for cons)
            const char* feature_list[] = {
                // Platform
#ifdef _WIN32
                "windows",
#endif
#ifdef __linux__
                "posix",
                "unix",
#endif
                // Numeric features
                "complex",           // Complex numbers supported
                "ratios",            // Exact rational numbers (a/b)
                "exact-closed",      // Exact arithmetic is closed
                "ieee-float",        // IEEE 754 floating point
                "exact-complex",     // Exact complex numbers
                // Core features
                "r7rs",              // R7RS-small compliant
                "zombie-scheme",     // Implementation name
                nullptr
            };

            // Build in reverse to get proper order
            int count = 0;
            while (feature_list[count] != nullptr) count++;

            for (int i = count - 1; i >= 0; i--) {
                result = gc_new<ConsCell>(
                    gc_new<a_string>(feature_list[i]),
                    result
                );
            }

            return result;
        }

        case VOID:
            // (void) - returns an unspecified value
            // Used for procedures that don't return a meaningful value
            return nil;

        default:
            return error("Unknown system Primitive");
    }
}

// ============================================================================
// Registration
// ============================================================================

void PrimitiveSystem::install(Environment* env) {
    // GC Safety: Use macro to protect name string before allocating primitive
    #define DEF(name, ...) do { \
        gc_root<a_string> _n(gc_new<a_string>(name)); \
        env->define(_n.get(), gc_new<PrimitiveSystem>(__VA_ARGS__)); \
    } while(0)

    // System interface operations
    DEF("exit",           EXIT, 0, 1);
    DEF("emergency-exit", EMERGENCYEXIT, 0, 1);
    DEF("command-line",   COMMANDLINE, 0, 0);
    DEF("features",       FEATURES, 0, 0);
    DEF("void",           VOID, 0, 0);

    #undef DEF
}
