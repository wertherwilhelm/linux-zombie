#include "pch.h"
/**
 * @file value_demo.cpp
 * @brief Demonstration of the Value Ledger and Multiverse system
 *
 * This file shows off the power of the multiverse:
 * - Values with full provenance tracking
 * - Swappable skins (behaviors)
 * - Cross-language mirrors
 * - Time-travel via snapshots
 *
 * Run this to see the magic!
 */

#include "value.h"
#include "ledger.h"
#include "mirror.h"
#include "scheme_skin.h"
#include "Object.h"
#include "atom.h"
#include "Base.h"
#include <iostream>
#include <iomanip>

// ============================================================================
// Demo Utilities
// ============================================================================

void print_banner(const std::string& title) {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║ " << std::left << std::setw(61) << title << " ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
}

void print_value_info(const std::string& label, const Value& v) {
    std::cout << "\n" << label << ":\n";
    std::cout << "  String:  " << v.to_string() << "\n";
    std::cout << "  Backend: " << backend_tag_name(v.backend()) << "\n";
    std::cout << "  Origin:  " << v.origin() << "\n";
    std::cout << "  Traits:  " << v.traits() << "\n";
    std::cout << "  Truthy:  " << (v.is_truthy() ? "yes" : "no") << "\n";
    if (v.is_callable()) {
        std::cout << "  [This value is callable!]\n";
    }
}

// ============================================================================
// Demo 1: Basic Value Wrapping
// ============================================================================

void demo_basic_values() {
    print_banner("Demo 1: Basic Value Wrapping with Provenance");

    // Wrap different Scheme types
    Value v_int = wrap_int(42, "demo:basic-int");
    Value v_double = wrap_double(3.14159, "demo:basic-double");
    Value v_string = wrap_string("Hello, Multiverse!", "demo:basic-string");
    Value v_nil = wrap_nil("demo:basic-nil");
    Value v_true = wrap_bool(true, "demo:basic-bool");
    Value v_false = wrap_bool(false, "demo:basic-bool");

    print_value_info("Integer 42", v_int);
    print_value_info("Pi", v_double);
    print_value_info("String", v_string);
    print_value_info("Nil", v_nil);
    print_value_info("True (#t)", v_true);
    print_value_info("False (#f)", v_false);

    std::cout << "\n✨ Notice how each value knows exactly where it came from!\n";
}

// ============================================================================
// Demo 2: Skin System
// ============================================================================

void demo_skins() {
    print_banner("Demo 2: Swappable Skins (Behavior Transformation)");

    // Create ledger and register skins
    Ledger ledger;
    ledger.register_skin(get_scheme_skin());

    // Create a custom "debug" skin (same behavior, different traits)
    Skin debug_skin = get_scheme_skin();
    debug_skin.name = "debug";
    debug_skin.default_traits = "verbose logging tracing";
    ledger.register_skin(debug_skin);

    // Create a value
    Value v = wrap_int(100, "demo:skin-test");

    print_value_info("Original (scheme skin)", v);

    // Apply debug skin
    ledger.apply_skin(v, "debug");
    print_value_info("After applying 'debug' skin", v);

    std::cout << "\n✨ Notice how traits accumulated: 'safe debuggable' + 'verbose logging tracing'\n";
    std::cout << "   Backend and origin preserved - history matters!\n";
}

// ============================================================================
// Demo 3: Provenance Tracking
// ============================================================================

void demo_provenance() {
    print_banner("Demo 3: Full Provenance Tracking");

    Ledger ledger;

    // Create values from different sources
    Value v1 = wrap_int(10, "repl:line-5");
    Value v2 = wrap_int(20, "file:test.scm:42");
    Value v3 = wrap_int(30, "native:init");
    v3.ledger.backend = BackendTag::Native;  // Mark as native
    v3.ledger.traits = "optimized inline";

    // Record in ledger
    ledger.record_value(v1.ptr, v1.ledger);
    ledger.record_value(v2.ptr, v2.ledger);
    ledger.record_value(v3.ptr, v3.ledger);

    print_value_info("Value from REPL", v1);
    print_value_info("Value from file", v2);
    print_value_info("Value from native code", v3);

    // Backend statistics
    std::cout << "\n📊 Backend Statistics:\n";
    auto counts = ledger.count_by_backend();
    std::cout << "  Scheme:   " << counts[BackendTag::Scheme] << " values\n";
    std::cout << "  Native:   " << counts[BackendTag::Native] << " values\n";
    std::cout << "  Bytecode: " << counts[BackendTag::Bytecode] << " values\n";
    std::cout << "  Dream:    " << counts[BackendTag::Dream] << " values\n";

    std::cout << "\n✨ Perfect for profiling: see which backend is used most!\n";
}

// ============================================================================
// Demo 4: Time Travel (Snapshots)
// ============================================================================

void demo_time_travel() {
    print_banner("Demo 4: Time Travel via Ledger Snapshots");

    Ledger ledger;

    // Create initial values
    Value v1 = wrap_int(100, "timeline:alpha");
    Value v2 = wrap_int(200, "timeline:alpha");

    ledger.record_value(v1.ptr, v1.ledger);
    ledger.record_value(v2.ptr, v2.ledger);

    std::cout << "\n📸 Capturing snapshot 'before-optimization'...\n";
    size_t snapshot_id = ledger.capture_snapshot("before-optimization");

    // Modify values (simulate optimization)
    v1.ledger.backend = BackendTag::Native;
    v1.ledger.traits = "optimized jit-compiled";
    v2.ledger.backend = BackendTag::Bytecode;
    v2.ledger.traits = "bytecode cached";

    ledger.record_value(v1.ptr, v1.ledger);
    ledger.record_value(v2.ptr, v2.ledger);

    std::cout << "\n⏭️  After optimization:\n";
    print_value_info("V1 (now native)", v1);
    print_value_info("V2 (now bytecode)", v2);

    std::cout << "\n⏮️  Restoring snapshot...\n";
    ledger.restore_snapshot(snapshot_id);

    // Look up restored provenance
    const LedgerEntry* restored1 = ledger.lookup_provenance(v1.ptr);
    const LedgerEntry* restored2 = ledger.lookup_provenance(v2.ptr);

    if (restored1 && restored2) {
        std::cout << "  V1 backend: " << backend_tag_name(restored1->backend) << " (was Native!)\n";
        std::cout << "  V2 backend: " << backend_tag_name(restored2->backend) << " (was Bytecode!)\n";
    }

    std::cout << "\n✨ We just traveled back in time! Original state restored!\n";
}

// ============================================================================
// Demo 5: Multiverse Branching
// ============================================================================

void demo_multiverse() {
    print_banner("Demo 5: Multiverse Branching (Parallel Realities)");

    std::cout << "\n🌌 Creating three parallel realities for the same value...\n";

    // Create Base value
    Value v_scheme = wrap_int(42, "multiverse:Base");

    // Reality A: Keep as Scheme (safe, slow)
    Value reality_a = v_scheme;
    reality_a.ledger.traits = "safe interpreted baseline";

    // Reality B: Pretend it's Bytecode (medium speed)
    Value reality_b = v_scheme;
    reality_b.ledger.backend = BackendTag::Bytecode;
    reality_b.ledger.traits = "bytecode balanced";

    // Reality C: Pretend it's Native (fast, risky)
    Value reality_c = v_scheme;
    reality_c.ledger.backend = BackendTag::Native;
    reality_c.ledger.traits = "native optimized jit";

    // Reality D: Dream variant (genetic mutation!)
    Value reality_d = v_scheme;
    reality_d.ledger.backend = BackendTag::Dream;
    reality_d.ledger.origin = "dream:mutant-137";
    reality_d.ledger.traits = "experimental genetic-variant unrolled";

    print_value_info("Reality A (Scheme baseline)", reality_a);
    print_value_info("Reality B (Bytecode)", reality_b);
    print_value_info("Reality C (Native JIT)", reality_c);
    print_value_info("Reality D (Dream variant)", reality_d);

    std::cout << "\n✨ Same value, four realities! Workbench would show these side-by-side!\n";
    std::cout << "   - Compare performance\n";
    std::cout << "   - Check semantic equivalence\n";
    std::cout << "   - Pick the best reality\n";
    std::cout << "   - Or blend traits from multiple!\n";
}

// ============================================================================
// Main Demo Runner
// ============================================================================

void run_all_demos() {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                                                               ║\n";
    std::cout << "║            🌌  VALUE LEDGER & MULTIVERSE DEMO  🌌              ║\n";
    std::cout << "║                                                               ║\n";
    std::cout << "║  Watch as we demonstrate:                                    ║\n";
    std::cout << "║  • Values with full provenance tracking                      ║\n";
    std::cout << "║  • Swappable behaviors (skins)                               ║\n";
    std::cout << "║  • Time-travel debugging (snapshots)                         ║\n";
    std::cout << "║  • Parallel realities (multiverse branching)                 ║\n";
    std::cout << "║                                                               ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";

    try {
        demo_basic_values();
        demo_skins();
        demo_provenance();
        demo_time_travel();
        demo_multiverse();

        std::cout << "\n";
        std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                                                               ║\n";
        std::cout << "║                    🎉  ALL DEMOS COMPLETE!  🎉                 ║\n";
        std::cout << "║                                                               ║\n";
        std::cout << "║  The multiverse foundation is ready!                         ║\n";
        std::cout << "║  Next steps:                                                 ║\n";
        std::cout << "║  • Add Scheme primitives (value-info, use-skin)              ║\n";
        std::cout << "║  • Build C++ → Scheme mirrors                                ║\n";
        std::cout << "║  • Integrate with Continuation snapshots                     ║\n";
        std::cout << "║  • Build the visual Multiverse Workbench!                    ║\n";
        std::cout << "║                                                               ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";

    } catch (const std::exception& e) {
        std::cerr << "\n❌ Demo failed: " << e.what() << "\n";
    }
}
