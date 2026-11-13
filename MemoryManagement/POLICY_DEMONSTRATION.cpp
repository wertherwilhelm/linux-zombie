/**
 * @file POLICY_DEMONSTRATION.cpp
 * @brief Comprehensive demonstration of all three memory policies
 *
 * This file demonstrates the memory:: abstraction layer working with:
 * 1. GCPolicy - Garbage collection with generational support
 * 2. SharedPtrPolicy - Reference counting with std::shared_ptr
 * 3. RawPtrPolicy - Manual memory management
 *
 * Each policy implements the same memory:: interfaces but with different
 * allocation strategies and lifetime management.
 *
 * COMPILE:
 *   cl.exe /EHsc /std:c++20 /I. /I../GarbageCollector /I../SceneGraph/include \
 *          POLICY_DEMONSTRATION.cpp \
 *          /link /LIBPATH:../x64/Debug GarbageCollector.lib
 *
 * RUN:
 *   ./POLICY_DEMONSTRATION.exe
 */

#include <iostream>
#include <memory>
#include <vector>
#include <cassert>

// ============================================================================
// Memory Abstraction Layer
// ============================================================================

#include "IMemoryManager.h"
#include "ITracer.h"
#include "IWriteBarrier.h"
#include "IRootManager.h"
#include "concepts.h"
#include "policies/all.h"

// ============================================================================
// Example Object Hierarchy (Policy-Agnostic)
// ============================================================================

/**
 * @brief Simple binary tree node (policy-templatized)
 *
 * This demonstrates how to write policy-agnostic code that works with
 * any memory management strategy.
 */
template<typename Policy>
class Node {
public:
    using NodePtr = typename Policy::template Ptr<Node>;

    int value;
    NodePtr left;
    NodePtr right;

    explicit Node(int v) : value(v), left(nullptr), right(nullptr) {}

    // Trace references if policy supports GC
    void trace_references(memory::ITracer& tracer) {
        if constexpr (memory::SupportsTracing<Policy>) {
            Policy::trace(tracer, left);
            Policy::trace(tracer, right);
        }
    }

    // Debug output
    void print(int depth = 0) const {
        for (int i = 0; i < depth; ++i) std::cout << "  ";
        std::cout << "Node(" << value << ")\n";

        if (left) {
            if constexpr (requires { left.get(); }) {
                // SharedPtr policy
                left->print(depth + 1);
            } else {
                // Raw/GC policy
                left->print(depth + 1);
            }
        }

        if (right) {
            if constexpr (requires { right.get(); }) {
                right->print(depth + 1);
            } else {
                right->print(depth + 1);
            }
        }
    }
};

// ============================================================================
// Demonstration 1: SharedPtrPolicy (No GC Required)
// ============================================================================

void demonstrate_shared_ptr_policy() {
    std::cout << "\n========================================\n";
    std::cout << "Demonstration 1: SharedPtrPolicy\n";
    std::cout << "========================================\n\n";

    using MyNode = Node<memory::SharedPtrPolicy>;
    using NodePtr = memory::SharedPtrPolicy::Ptr<MyNode>;

    std::cout << "Creating binary tree with shared_ptr...\n";

    // Create root
    auto root = std::make_shared<MyNode>(1);

    // Add children
    root->left = std::make_shared<MyNode>(2);
    root->right = std::make_shared<MyNode>(3);

    // Add grandchildren
    root->left->left = std::make_shared<MyNode>(4);
    root->left->right = std::make_shared<MyNode>(5);

    std::cout << "Tree structure:\n";
    root->print();

    std::cout << "\nReference counts are automatic!\n";
    std::cout << "Use count for root: " << root.use_count() << "\n";
    std::cout << "Use count for left child: " << root->left.use_count() << "\n";

    std::cout << "\nNo GC dependency - portable standard C++\n";
    std::cout << "Memory freed automatically on scope exit\n";
}

// ============================================================================
// Demonstration 2: RawPtrPolicy (Manual Management)
// ============================================================================

void demonstrate_raw_ptr_policy() {
    std::cout << "\n========================================\n";
    std::cout << "Demonstration 2: RawPtrPolicy\n";
    std::cout << "========================================\n\n";

    using MyNode = Node<memory::RawPtrPolicy>;

    std::cout << "Creating binary tree with raw pointers...\n";

    // Manual allocation
    auto* root = new MyNode(1);
    root->left = new MyNode(2);
    root->right = new MyNode(3);
    root->left->left = new MyNode(4);
    root->left->right = new MyNode(5);

    std::cout << "Tree structure:\n";
    root->print();

    std::cout << "\nZero overhead - direct pointer access\n";
    std::cout << "Full control over lifetime\n";

    // Manual cleanup (depth-first)
    std::function<void(MyNode*)> cleanup = [&](MyNode* node) {
        if (!node) return;
        cleanup(node->left);
        cleanup(node->right);
        delete node;
    };

    cleanup(root);
    std::cout << "\nManual delete required - easy to leak!\n";
}

// ============================================================================
// Demonstration 3: GCPolicy (Garbage Collection)
// ============================================================================

#ifdef SCENEGRAPH_USE_GC
#include <GarbageCollector/gc_manager_template.h>
#include <GarbageCollector/generational_gc_strategy.h>
#include <GarbageCollector/gc_context.h>
#include <GarbageCollector/gc_new.h>
#include <GarbageCollector/gc_local.h>
#include <GarbageCollector/gc_object.h>

// GC-compatible node (derives from gc_object)
template<typename Policy>
class GCNode : public gc_object {
public:
    int value;
    GCNode* left;
    GCNode* right;

    explicit GCNode(int v) : value(v), left(nullptr), right(nullptr) {}

    void trace_references(Tracer& tracer) override {
        tracer.visit_field(reinterpret_cast<void**>(&left));
        tracer.visit_field(reinterpret_cast<void**>(&right));
    }

    void print(int depth = 0) const {
        for (int i = 0; i < depth; ++i) std::cout << "  ";
        std::cout << "GCNode(" << value << ")\n";
        if (left) left->print(depth + 1);
        if (right) right->print(depth + 1);
    }
};

void demonstrate_gc_policy() {
    std::cout << "\n========================================\n";
    std::cout << "Demonstration 3: GCPolicy\n";
    std::cout << "========================================\n\n";

    // Create generational GC
    gc_manager_t<GenerationalGCStrategy> gc;
    gc_context ctx(&gc);

    std::cout << "GC initialized (generational, moving collector)\n";
    std::cout << "GC implements all memory:: interfaces:\n";
    std::cout << "  - memory::IMemoryManager (allocate/deallocate)\n";
    std::cout << "  - memory::ITracer (visit/visit_field)\n";
    std::cout << "  - memory::IWriteBarrier (write_barrier)\n";
    std::cout << "  - memory::IRootManager (add_root/remove_root)\n\n";

    // Verify concepts
    static_assert(memory::MemoryManager<decltype(gc)>,
                  "GC must satisfy MemoryManager concept");
    static_assert(memory::SupportsTracing<decltype(gc)>,
                  "GC must support tracing");
    static_assert(memory::SupportsWriteBarrier<decltype(gc)>,
                  "Generational GC must support write barriers");
    static_assert(memory::SupportsRooting<decltype(gc)>,
                  "GC must support rooting");
    static_assert(memory::TracingGC<decltype(gc)>,
                  "GC must satisfy TracingGC concept");
    static_assert(memory::GenerationalGC<decltype(gc)>,
                  "This is a generational GC");

    std::cout << "Creating binary tree with GC...\n";

    // Allocate root and protect from collection
    auto* root = gc_new<GCNode<memory::GCPolicy>>(1);
    gc_local root_guard{root};  // Uses memory::IRootManager::add_root()

    std::cout << "Root protected via gc_local (RAII root guard)\n";
    std::cout << "  gc_local uses memory::IRootManager::add_root()\n\n";

    // Build tree (write barriers fire automatically)
    root->left = gc_new<GCNode<memory::GCPolicy>>(2);
    root->right = gc_new<GCNode<memory::GCPolicy>>(3);
    root->left->left = gc_new<GCNode<memory::GCPolicy>>(4);
    root->left->right = gc_new<GCNode<memory::GCPolicy>>(5);

    std::cout << "Tree structure:\n";
    root->print();

    std::cout << "\nAllocating garbage to trigger GC...\n";
    for (int i = 0; i < 1000; ++i) {
        gc_new<GCNode<memory::GCPolicy>>(i);
    }

    size_t before = gc.object_count();
    std::cout << "Objects before GC: " << before << "\n";

    std::cout << "\nTriggering garbage collection...\n";
    gc.collect();

    size_t after = gc.object_count();
    std::cout << "Objects after GC: " << after << "\n";
    std::cout << "Collected: " << (before - after) << " objects\n";

    std::cout << "\nRoot and reachable nodes survived!\n";
    std::cout << "Tree still intact:\n";
    root->print();

    std::cout << "\nDemonstrating moving GC safety...\n";
    void* old_addr = root->left;
    std::cout << "Address before GC: " << old_addr << "\n";

    gc.collect();

    void* new_addr = root->left;
    std::cout << "Address after GC: " << new_addr << "\n";

    if (old_addr != new_addr) {
        std::cout << "Object MOVED! Pointer automatically updated via slot-based tracing\n";
    } else {
        std::cout << "Object stayed in same location (no compaction needed)\n";
    }

    std::cout << "\nNo manual delete needed - GC handles cleanup!\n";
    std::cout << "gc_local automatically unroots on scope exit\n";
}
#endif

// ============================================================================
// Demonstration 4: Policy Comparison
// ============================================================================

void demonstrate_policy_comparison() {
    std::cout << "\n========================================\n";
    std::cout << "Policy Comparison Summary\n";
    std::cout << "========================================\n\n";

    std::cout << "SharedPtrPolicy:\n";
    std::cout << "  ✓ No GC dependency (portable)\n";
    std::cout << "  ✓ Deterministic destruction\n";
    std::cout << "  ✓ Thread-safe reference counting\n";
    std::cout << "  ✗ Reference counting overhead\n";
    std::cout << "  ✗ Cyclic references leak\n";
    std::cout << "  ✗ Memory fragmentation\n\n";

    std::cout << "RawPtrPolicy:\n";
    std::cout << "  ✓ Zero overhead\n";
    std::cout << "  ✓ Full control\n";
    std::cout << "  ✓ Predictable performance\n";
    std::cout << "  ✗ Manual lifetime management\n";
    std::cout << "  ✗ Easy to leak\n";
    std::cout << "  ✗ No safety nets\n\n";

#ifdef SCENEGRAPH_USE_GC
    std::cout << "GCPolicy:\n";
    std::cout << "  ✓ Automatic collection\n";
    std::cout << "  ✓ No manual delete\n";
    std::cout << "  ✓ Compaction (no fragmentation)\n";
    std::cout << "  ✓ Fast allocation (~10ns)\n";
    std::cout << "  ✗ GC dependency required\n";
    std::cout << "  ✗ GC pauses (typically <5ms)\n";
    std::cout << "  ✗ Requires gc_object base class\n\n";
#endif

    std::cout << "All three policies implement memory:: interfaces!\n";
    std::cout << "Choose based on your application's needs:\n";
    std::cout << "  - Portability → SharedPtrPolicy\n";
    std::cout << "  - Performance → RawPtrPolicy\n";
    std::cout << "  - Convenience → GCPolicy\n";
}

// ============================================================================
// Demonstration 5: Concept-Based Dispatch
// ============================================================================

template<typename Policy>
void process_with_policy() {
    std::cout << "\n========================================\n";
    std::cout << "Concept-Based Dispatch\n";
    std::cout << "========================================\n\n";

    std::cout << "Policy: " << typeid(Policy).name() << "\n\n";

    // Compile-time capability detection
    if constexpr (memory::SupportsTracing<Policy>) {
        std::cout << "✓ Policy supports tracing (GC)\n";
    } else {
        std::cout << "✗ Policy does NOT support tracing\n";
    }

    if constexpr (memory::SupportsWriteBarrier<Policy>) {
        std::cout << "✓ Policy supports write barriers (generational GC)\n";
    } else {
        std::cout << "✗ Policy does NOT support write barriers\n";
    }

    if constexpr (memory::SupportsRooting<Policy>) {
        std::cout << "✓ Policy supports rooting (GC roots)\n";
    } else {
        std::cout << "✗ Policy does NOT support rooting\n";
    }

    std::cout << "\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "============================================\n";
    std::cout << "Memory Abstraction Layer Demonstration\n";
    std::cout << "============================================\n";
    std::cout << "\nThis demonstrates Phase 4 of the GC abstraction:\n";
    std::cout << "  - Three memory policies (GC, SharedPtr, Raw)\n";
    std::cout << "  - All implement memory:: interfaces\n";
    std::cout << "  - Policy-agnostic code via templates\n";
    std::cout << "  - Compile-time capability detection\n";

    // Run demonstrations
    demonstrate_shared_ptr_policy();
    demonstrate_raw_ptr_policy();

#ifdef SCENEGRAPH_USE_GC
    demonstrate_gc_policy();
#else
    std::cout << "\n========================================\n";
    std::cout << "Demonstration 3: GCPolicy (SKIPPED)\n";
    std::cout << "========================================\n\n";
    std::cout << "GCPolicy demonstration requires:\n";
    std::cout << "  #define SCENEGRAPH_USE_GC\n";
    std::cout << "  Link with GarbageCollector.lib\n";
    std::cout << "\nRebuild with GC support to see this demo.\n";
#endif

    demonstrate_policy_comparison();

    // Concept-based dispatch
    process_with_policy<memory::SharedPtrPolicy>();
    process_with_policy<memory::RawPtrPolicy>();

    std::cout << "\n============================================\n";
    std::cout << "All demonstrations complete!\n";
    std::cout << "============================================\n";

    return 0;
}
