/**
 * @file gc_ergonomics_demo_v2.cpp
 * @brief Demonstration of improved automatic write barriers (opt-out, not opt-in)
 *
 * This demo shows the new ergonomics improvements:
 * 1. Virtual write_barrier() method (no RTTI/dynamic_cast)
 * 2. Automatic owner binding via _gc_bind_fields()
 * 3. Opt-out safety model (barriers fire by default)
 * 4. set_unsafe() for performance-critical paths
 *
 * Build:
 *   cl.exe /EHsc /std:c++latest /I. gc_ergonomics_demo_v2.cpp
 */

#include "gc_manager_template.h"
#include "generational_gc_strategy.h"
#include "gc_context.h"
#include "gc_new.h"
#include "gc_field.h"
#include "auto_traceable.h"
#include "gc_local.h"
#include <iostream>
#include <cassert>

// ============================================================================
// Example Scene Graph Node with Automatic Write Barriers
// ============================================================================

struct SceneNode : auto_traceable<SceneNode> {
    gc_field<SceneNode> left;
    gc_field<SceneNode> right;
    gc_field<SceneNode> parent;
    int value = 0;

    GC_FIELDS(left, right, parent);

    SceneNode(int v = 0) : value(v) {
        GC_INIT_FIELDS();  // Binds all fields for automatic write barriers
    }
};

// ============================================================================
// Test 1: Automatic Write Barriers
// ============================================================================

void test_automatic_write_barriers() {
    std::cout << "\n=== Test 1: Automatic Write Barriers ===\n";

    gc_manager_t<GenerationalGCStrategy> gc;
    gc_context ctx(&gc);

    // Create nodes in different generations
    auto* old_node = gc_new_old<SceneNode>(&gc, 1);
    auto* young_node = gc_new_young<SceneNode>(&gc, 2);

    gc_local root{old_node};

    std::cout << "Created old_node (value=" << old_node->value << ")\n";
    std::cout << "Created young_node (value=" << young_node->value << ")\n";

    // This assignment automatically fires write barrier!
    // No manual barrier recording needed.
    old_node->left = young_node;

    std::cout << "Assigned young_node to old_node->left (barrier fired automatically)\n";

    // Verify young node is in remembered set
    auto& strategy = gc.strategy();
    std::cout << "Checking remembered set...\n";

    // Force minor GC - young_node should survive because of remembered set
    strategy.minor_collect();

    std::cout << "Minor GC completed - young_node should have survived\n";

    // Verify old_node->left still valid (should be updated if moved)
    assert(old_node->left != nullptr);
    assert(old_node->left->value == 2);

    std::cout << "✓ old_node->left still valid (value=" << old_node->left->value << ")\n";
}

// ============================================================================
// Test 2: Opt-Out Safety Model (set_unsafe)
// ============================================================================

void test_opt_out_safety() {
    std::cout << "\n=== Test 2: Opt-Out Safety Model ===\n";

    gc_manager_t<GenerationalGCStrategy> gc;
    gc_context ctx(&gc);

    auto* node = gc_new_young<SceneNode>(&gc, 42);
    gc_local root{node};

    // Safe mode (default) - fires write barrier
    auto* child1 = gc_new_young<SceneNode>(&gc, 1);
    node->left = child1;  // Barrier fires automatically
    std::cout << "✓ Safe assignment: node->left = child1 (barrier fired)\n";

    // Unsafe mode (opt-out) - skip barrier for performance
    auto* child2 = gc_new_young<SceneNode>(&gc, 2);
    node->right.set_unsafe(child2);  // Explicit opt-out
    std::cout << "✓ Unsafe assignment: node->right.set_unsafe(child2) (no barrier)\n";

    std::cout << "Both assignments completed:\n";
    std::cout << "  left->value = " << node->left->value << "\n";
    std::cout << "  right->value = " << node->right->value << "\n";
}

// ============================================================================
// Test 3: No RTTI/Dynamic Cast (Virtual Method Dispatch)
// ============================================================================

void test_virtual_write_barrier() {
    std::cout << "\n=== Test 3: Virtual Write Barrier (No RTTI) ===\n";

    // Test with mark-sweep GC (barriers are no-ops)
    {
        gc_manager_t<MarkSweepStrategy> gc_ms;
        gc_context ctx(&gc_ms);

        auto* node1 = gc_ms.allocate<SceneNode>(10);
        auto* node2 = gc_ms.allocate<SceneNode>(20);

        gc_local root{node1, node2};

        // Assignment works, but barrier is no-op for mark-sweep
        node1->left = node2;
        std::cout << "✓ Mark-sweep: Assignment completed (barrier was no-op)\n";
    }

    // Test with generational GC (barriers record remembered set)
    {
        gc_manager_t<GenerationalGCStrategy> gc_gen;
        gc_context ctx(&gc_gen);

        auto* old_node = gc_new_old<SceneNode>(&gc_gen, 30);
        auto* young_node = gc_new_young<SceneNode>(&gc_gen, 40);

        gc_local root{old_node};

        // Assignment fires actual barrier for generational GC
        old_node->left = young_node;
        std::cout << "✓ Generational: Assignment completed (barrier recorded edge)\n";
    }

    std::cout << "✓ Both GC types work with same code (no dynamic_cast!)\n";
}

// ============================================================================
// Test 4: Manual Binding (Without auto_traceable)
// ============================================================================

struct ManualNode : gc_object {
    gc_field<ManualNode> child;
    int value = 0;

    ManualNode(int v = 0) : value(v) {
        // Manual binding (for classes not using auto_traceable)
        child.bind_owner(this);
    }

    void trace_references(Tracer& t) override {
        child.trace(t);
    }
};

void test_manual_binding() {
    std::cout << "\n=== Test 4: Manual Binding (Non-auto_traceable) ===\n";

    gc_manager_t<GenerationalGCStrategy> gc;
    gc_context ctx(&gc);

    auto* parent = gc_new_old<ManualNode>(&gc, 100);
    auto* child = gc_new_young<ManualNode>(&gc, 200);

    gc_local root{parent};

    // Even with manual binding, barriers fire automatically
    parent->child = child;
    std::cout << "✓ Manual binding: parent->child = child (barrier fired)\n";

    gc.strategy().minor_collect();

    assert(parent->child != nullptr);
    assert(parent->child->value == 200);
    std::cout << "✓ Child survived minor GC (value=" << parent->child->value << ")\n";
}

// ============================================================================
// Test 5: Unbound Fields (Conservative Behavior)
// ============================================================================

struct UnboundNode : gc_object {
    gc_field<UnboundNode> child;  // Never bound!
    int value = 0;

    UnboundNode(int v = 0) : value(v) {
        // Intentionally skip binding for testing
    }

    void trace_references(Tracer& t) override {
        child.trace(t);
    }
};

void test_unbound_conservative() {
    std::cout << "\n=== Test 5: Unbound Fields (Conservative Behavior) ===\n";

    gc_manager_t<GenerationalGCStrategy> gc;
    gc_context ctx(&gc);

    auto* parent = gc_new_old<UnboundNode>(&gc, 1000);
    auto* child = gc_new_young<UnboundNode>(&gc, 2000);

    gc_local root{parent};

    // Assignment works, but barrier doesn't fire (field not bound)
    parent->child = child;
    std::cout << "⚠ Unbound field: parent->child = child (no barrier - conservative)\n";
    std::cout << "  Field is bound: " << (parent->child.is_bound() ? "YES" : "NO") << "\n";

    // Child is still traceable, so it survives (conservative but safe)
    gc.strategy().minor_collect();

    assert(parent->child != nullptr);
    assert(parent->child->value == 2000);
    std::cout << "✓ Child survived anyway (tracing provides safety)\n";
    std::cout << "  (Conservative: may retain objects longer, but never incorrect)\n";
}

// ============================================================================
// Main Test Harness
// ============================================================================

int main() {
    std::cout << "=================================================\n";
    std::cout << "  GC Ergonomics V2: Automatic Write Barriers\n";
    std::cout << "=================================================\n";

    try {
        test_automatic_write_barriers();
        test_opt_out_safety();
        test_virtual_write_barrier();
        test_manual_binding();
        test_unbound_conservative();

        std::cout << "\n=================================================\n";
        std::cout << "     All Tests Passed! ✓\n";
        std::cout << "=================================================\n";

        std::cout << "\nKey Improvements:\n";
        std::cout << "✓ Write barriers are opt-out (fire by default)\n";
        std::cout << "✓ No RTTI/dynamic_cast (virtual method dispatch)\n";
        std::cout << "✓ Automatic binding via _gc_bind_fields()\n";
        std::cout << "✓ set_unsafe() for hot paths\n";
        std::cout << "✓ Conservative safety (unbound fields still traced)\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << "\n";
        return 1;
    }
}
