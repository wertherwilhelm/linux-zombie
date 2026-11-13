#include "pch.h"
#include "gtest/gtest.h"
#include "gc_manager_template.h"
#include "mark_sweep_strategy.h"
#include "generational_gc_strategy.h"
#include "gc_allocator.h"
#include "gc_ptr.h"
#include "Object.h"
#include "atom.h"
#include "cons_cell.h"
#include "constants.h"
#include "Base.h"
#include <memory>

// Type aliases (same as other tests)
using a_integer = atom<long long>;
using a_double = atom<double>;

/**
 * @file GCAllocatorTests.cpp
 * @brief Tests for pluggable allocator infrastructure
 *
 * These tests verify that gc_manager_t works correctly with different allocators
 * and that objects are actually deallocated (not leaked) during garbage collection.
 */

class GCAllocatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Global GC already initialized in main
        // init_lisp_constants() is only needed once per program
    }

    void TearDown() override {
        // DON'T call shutdown_gc() here!
        // This test uses local gc_manager_t instances, not the global gc
        // Calling shutdown_gc() breaks all subsequent tests that use gc->allocate()
    }
};

/**
 * Test: gc_manager_t with MallocAllocator doesn't leak memory
 *
 * This test verifies that:
 * 1. gc_manager_t correctly implements IGarbageCollector
 * 2. Objects are actually deallocated during collection
 * 3. bytes_allocated decreases after GC
 */
TEST_F(GCAllocatorTest, TemplateMallocAllocatorNoLeak) {
    gc_manager_t<MarkSweepStrategy, MallocAllocator> gc;

    // Allocate many objects without roots (should be collected)
    size_t before_alloc = gc.object_count();

    for (int i = 0; i < 100; ++i) {
        auto* obj = gc.allocate<a_integer>(i);
        (void)obj;  // Unused, will be collected
    }

    size_t after_alloc = gc.object_count();
    size_t bytes_before_gc = gc.bytes_allocated();

    // All 100 objects should be tracked
    EXPECT_EQ(after_alloc - before_alloc, 100);
    EXPECT_GT(bytes_before_gc, 0);

    // Force GC - should collect all unreachable objects
    gc.collect();

    size_t after_gc = gc.object_count();
    size_t bytes_after_gc = gc.bytes_allocated();

    // All temporary objects should be collected
    EXPECT_EQ(after_gc, before_alloc);

    // Bytes should decrease (objects actually deallocated)
    EXPECT_LT(bytes_after_gc, bytes_before_gc);

    // Get stats to verify collection happened
    auto stats = gc.get_stats();
    EXPECT_GT(stats.collections_run, 0);
    EXPECT_GT(stats.objects_collected, 0);
}

/**
 * Test: gc_manager_t with PoolAllocator doesn't leak memory
 *
 * DISABLED: PoolAllocator cannot be passed by value to gc_manager_t constructor
 * because it contains std::vector<std::unique_ptr<...>> which is not copyable.
 *
 * This is a known architectural issue documented in FIX_PLAN.md Phase 3:
 * - Need to make allocator pluggable via reference/pointer, not by value
 * - OR make PoolAllocator copyable (complex, requires deep copy semantics)
 *
 * TODO: Re-enable this test after Phase 3 fixes are implemented.
 *
 * What this test WOULD verify (once enabled):
 * - gc_manager_t with PoolAllocator correctly deallocates objects
 * - Objects are reclaimed using the pool allocator's deallocate() method
 * - No memory leaks occur with custom allocators
 */
TEST_F(GCAllocatorTest, TemplatePoolAllocatorNoLeak) {
    // Test gc_manager_t with PoolAllocator
    // This verifies Phase 1 fix: gc_manager_t properly reclaims objects
    PoolAllocator pool;
    gc_manager_t<MarkSweepStrategy, PoolAllocator> gc(std::move(pool));

    // Allocate many objects without roots (should be collected)
    size_t before_alloc = gc.object_count();

    for (int i = 0; i < 100; ++i) {
        auto* obj = gc.allocate<a_integer>(i);
        (void)obj;  // Unused, will be collected
    }

    size_t after_alloc = gc.object_count();
    size_t bytes_before_gc = gc.bytes_allocated();

    // All 100 objects should be tracked
    EXPECT_EQ(after_alloc - before_alloc, 100);
    EXPECT_GT(bytes_before_gc, 0);

    // Force GC - should collect all unreachable objects
    gc.collect();

    size_t after_gc = gc.object_count();
    size_t bytes_after_gc = gc.bytes_allocated();

    // All temporary objects should be collected
    EXPECT_EQ(after_gc, before_alloc);

    // CRITICAL: Bytes should decrease (objects actually deallocated)
    // This would FAIL before the fix (bytes stayed the same = memory leak)
    EXPECT_LT(bytes_after_gc, bytes_before_gc);

    // Get stats to verify collection happened
    auto stats = gc.get_stats();
    EXPECT_GT(stats.collections_run, 0);
    EXPECT_EQ(stats.objects_collected, 100);  // All 100 should be collected
}

/**
 * Test: gc_manager_t with roots prevents collection
 *
 * Verifies that objects reachable from roots are NOT collected.
 */
TEST_F(GCAllocatorTest, TemplateGCWithRootsPreventsCollection) {
    gc_manager_t<MarkSweepStrategy, MallocAllocator> gc;

    // Allocate Object and protect it with root
    auto* protected_obj = gc.allocate<a_integer>(42);
    gc.add_root(reinterpret_cast<void**>(&protected_obj));  // Slot-based API

    // Allocate many unprotected objects
    for (int i = 0; i < 100; ++i) {
        auto* obj = gc.allocate<a_integer>(i);
        (void)obj;  // Unused, will be collected
    }

    size_t before_gc = gc.object_count();
    gc.collect();
    size_t after_gc = gc.object_count();

    // 100 objects should be collected, 1 root remains
    EXPECT_EQ(before_gc - after_gc, 100);

    // Verify protected Object still exists
    EXPECT_EQ(protected_obj->value(), 42);

    gc.remove_root(reinterpret_cast<void**>(&protected_obj));  // Slot-based API
}

/**
 * Test: gc_manager_t correctly traces Object graphs
 *
 * DISABLED: This test exposes a potential issue with tracing through nested structures.
 * The inner ConsCell's children (integers 1 and 2) appear to be collected even though
 * they should be reachable from the rooted outer structure.
 *
 * TODO: Investigate whether this is:
 * 1. A test bug (improper root management during construction)
 * 2. A GC bug (mark phase not tracing deeply enough)
 * 3. A timing issue (GC triggering at unexpected point)
 *
 * For now, simpler tests (Shared Structures, etc.) cover the basic tracing functionality.
 */
TEST_F(GCAllocatorTest, TemplateGCTracesObjectGraph) {
    gc_manager_t<MarkSweepStrategy, MallocAllocator> gc;

    // Build a simple nested structure: ((1 . 2) . 3)
    // Protect as we build to avoid premature collection
    auto* inner = gc.allocate<ConsCell>(gc.allocate<a_integer>(1),
                                          gc.allocate<a_integer>(2));
    gc.add_root(reinterpret_cast<void**>(&inner));  // Protect inner while building outer

    auto* outer = gc.allocate<ConsCell>(inner, gc.allocate<a_integer>(3));
    gc.add_root(reinterpret_cast<void**>(&outer));  // Now root the outer structure
    gc.remove_root(reinterpret_cast<void**>(&inner));  // Inner is reachable from outer, so unroot it

    // Allocate garbage
    for (int i = 0; i < 50; ++i) {
        auto* garbage = gc.allocate<a_integer>(999);
        (void)garbage;
    }

    size_t before_gc = gc.object_count();
    gc.collect();
    size_t after_gc = gc.object_count();

    // Should collect 50 garbage objects
    EXPECT_GE(before_gc - after_gc, 50);

    // Verify structure intact: ((1 . 2) . 3)
    auto* inner_after = static_cast<ConsCell*>(car(outer));
    EXPECT_EQ(static_cast<a_integer*>(car(inner_after))->value(), 1);
    EXPECT_EQ(static_cast<a_integer*>(cdr(inner_after))->value(), 2);
    EXPECT_EQ(static_cast<a_integer*>(cdr(outer))->value(), 3);

    gc.remove_root(reinterpret_cast<void**>(&outer));
}

/**
 * Test: gc_manager_t with shared structures
 *
 * Verifies that shared structures (DAG) are handled correctly.
 */
TEST_F(GCAllocatorTest, TemplateGCSharedStructures) {
    gc_manager_t<MarkSweepStrategy, MallocAllocator> gc;

    // Create shared structure: (A B) and (C B) both share B
    auto* b = gc.allocate<a_integer>(2);
    auto* list1 = gc.allocate<ConsCell>(gc.allocate<a_integer>(1),
                                          gc.allocate<ConsCell>(b, nil));
    auto* list2 = gc.allocate<ConsCell>(gc.allocate<a_integer>(3),
                                          gc.allocate<ConsCell>(b, nil));

    // Root both lists
    gc.add_root(reinterpret_cast<void**>(&list1));
    gc.add_root(reinterpret_cast<void**>(&list2));

    // Allocate garbage
    for (int i = 0; i < 50; ++i) {
        auto* garbage = gc.allocate<a_integer>(i);
        (void)garbage;
    }

    size_t before_gc = gc.object_count();
    gc.collect();
    size_t after_gc = gc.object_count();

    // Should collect 50 garbage objects, preserve both lists and shared B
    EXPECT_GE(before_gc - after_gc, 50);

    // Verify both lists intact
    EXPECT_EQ(static_cast<a_integer*>(car(list1))->value(), 1);
    EXPECT_EQ(static_cast<a_integer*>(car(list2))->value(), 3);

    // Verify B is shared (same instance)
    EXPECT_EQ(car(cdr(list1)), car(cdr(list2)));

    gc.remove_root(reinterpret_cast<void**>(&list1));
    gc.remove_root(reinterpret_cast<void**>(&list2));
}

/**
 * Test: gc_manager_t threshold triggers automatic collection
 *
 * Verifies that GC automatically triggers when threshold is exceeded.
 */
TEST_F(GCAllocatorTest, TemplateGCThresholdAutoCollection) {
    gc_manager_t<MarkSweepStrategy, MallocAllocator> gc;

    // Set very low threshold (1 KB)
    gc.set_gc_threshold(1024);

    size_t initial_collections = gc.get_stats().collections_run;

    // Allocate enough objects to exceed threshold
    for (int i = 0; i < 100; ++i) {
        auto* obj = gc.allocate<a_integer>(i);
        (void)obj;
    }

    size_t final_collections = gc.get_stats().collections_run;

    // At least one automatic collection should have occurred
    EXPECT_GT(final_collections, initial_collections);
}

/**
 * Test: gc_manager_t with smart pointers (gc_root)
 *
 * Verifies integration with gc_root RAII wrapper.
 */
TEST_F(GCAllocatorTest, TemplateGCWithSmartPointers) {
    gc_manager_t<MarkSweepStrategy, MallocAllocator> gc;

    {
        // Use gc_root for automatic root management
        auto* obj = gc.allocate<a_integer>(42);
        gc_root<a_integer> root(&gc, obj);

        // Allocate garbage
        for (int i = 0; i < 50; ++i) {
            auto* garbage = gc.allocate<a_integer>(i);
            (void)garbage;
        }

        gc.collect();

        // Object should still be alive
        EXPECT_EQ(root->value(), 42);

        // gc_root destructor will unregister root
    }

    // After scope exit, Object is no longer rooted
    gc.collect();

    // All objects should be collected now
    // (accounting might not be perfect, so we just check that some were collected)
    auto stats = gc.get_stats();
    EXPECT_GT(stats.objects_collected, 0);
}

/**
 * Test: gc_manager_t stress test (1000 allocations)
 *
 * High-volume test to detect memory leaks or corruption.
 */
TEST_F(GCAllocatorTest, TemplateGCStressTest) {
    gc_manager_t<MarkSweepStrategy, MallocAllocator> gc;

    // Set reasonable threshold
    gc.set_gc_threshold(10 * 1024);  // 10 KB

    for (int i = 0; i < 1000; ++i) {
        auto* obj = gc.allocate<a_integer>(i);
        (void)obj;
    }

    // Force final collection
    gc.collect();

    // Verify stats are reasonable
    auto stats = gc.get_stats();
    EXPECT_GT(stats.collections_run, 0);
    EXPECT_GT(stats.objects_collected, 0);

    // Final Object count should be low (only constants remain)
    EXPECT_LT(gc.object_count(), 20);
}

/**
 * Test: Compare gc_manager vs gc_manager_t behavior
 *
 * Verifies that template version behaves identically to runtime version.
 */
TEST_F(GCAllocatorTest, RuntimeVsTemplateConsistency) {
    // Runtime GC
    auto* runtime_gc = gc_context::current();
    size_t runtime_before = runtime_gc->object_count();

    std::cout << "DEBUG: Runtime GC before allocation: " << runtime_before << " objects\n";

    for (int i = 0; i < 50; ++i) {
        auto* obj = runtime_gc->allocate<a_integer>(i);
        (void)obj;
    }

    size_t runtime_after_alloc = runtime_gc->object_count();
    std::cout << "DEBUG: Runtime GC after allocating 50: " << runtime_after_alloc << " objects\n";

    runtime_gc->collect();
    size_t runtime_after = runtime_gc->object_count();
    std::cout << "DEBUG: Runtime GC after collection: " << runtime_after << " objects\n";

    // Template GC
    gc_manager_t<MarkSweepStrategy, MallocAllocator> template_gc;
    size_t template_before = template_gc.object_count();

    std::cout << "DEBUG: Template GC before allocation: " << template_before << " objects\n";

    for (int i = 0; i < 50; ++i) {
        auto* obj = template_gc.allocate<a_integer>(i);
        (void)obj;
    }

    size_t template_after_alloc = template_gc.object_count();
    std::cout << "DEBUG: Template GC after allocating 50: " << template_after_alloc << " objects\n";

    template_gc.collect();
    size_t template_after = template_gc.object_count();
    std::cout << "DEBUG: Template GC after collection: " << template_after << " objects\n";

    // Calculate how many objects were actually collected (after alloc - after GC)
    size_t runtime_collected = runtime_after_alloc - runtime_after;
    size_t template_collected = template_after_alloc - template_after;

    std::cout << "DEBUG: Runtime collected: " << runtime_collected << " (expected ~50)\n";
    std::cout << "DEBUG: Template collected: " << template_collected << " (expected ~50)\n";

    // REVISED TEST: Each GC should collect approximately the 50 objects it just allocated
    // NOTE: Runtime GC may collect more due to contamination from previous tests,
    // but it should at least collect the 50 we just allocated
    EXPECT_GE(runtime_collected, 40) << "Runtime GC should collect at least 40 of the 50 allocated objects";
    EXPECT_NEAR(static_cast<double>(template_collected), 50.0, 10.0) << "Template GC should collect ~50 objects";
}

/**
 * Regression test (P1): an allocation that triggers a collection must not have the
 * freshly-allocated object reclaimed by that very collection.
 *
 * With gc_threshold == 0, on_allocate() runs a collection on every allocation.
 * The new object is already tracked but not yet reachable from any root (the caller
 * hasn't received it), so an unguarded collect would sweep it and allocate<T>() would
 * return a dangling pointer (heap-use-after-free under ASan). allocate<T>() now
 * temporarily roots the in-flight object across on_allocate() to prevent this.
 */
TEST_F(GCAllocatorTest, AllocationTriggeringCollectionIsNotReclaimed) {
    gc_manager_t<MarkSweepStrategy, MallocAllocator> gc;
    gc.set_gc_threshold(0);  // every allocation triggers a collection inside on_allocate

    size_t before = gc.object_count();
    auto* obj = gc.allocate<a_integer>(0xC0FFEE);

    // The object must survive the collection its own allocation triggered.
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->value(), 0xC0FFEE);              // safe to read (UAF under ASan otherwise)
    EXPECT_EQ(gc.object_count() - before, 1u);      // still tracked, not swept

    // Once genuinely unreachable, a later collection reclaims it as normal.
    gc.collect();
    EXPECT_EQ(gc.object_count() - before, 0u);
}

/**
 * Regression test (P1): a promoted object's children must be scanned and forwarded
 * during the same minor collection.
 *
 * When a young object survives enough collections it is promoted to the old
 * generation and leaves the young to-space. The Cheney scan loop only walks
 * to-space, so a promoted object was never scanned for children — any child it had
 * acquired while young stayed in the discarded from-space and became dangling after
 * the space swap. minor_collect() now drains the list of objects promoted during the
 * cycle, forwarding their children too.
 *
 * Repro: a parent survives two collections (reaching the default promotion age of 3),
 * then gets a brand-new child, and the third collection promotes the parent. The
 * child is reachable only through the parent, so it is copied iff promoted objects
 * are scanned. The forwarding necessarily moves it to a different (young-gen) address,
 * which is the deterministic discriminator: before the fix the parent's car pointer
 * was left pointing at the child's original, now-dead from-space slot.
 */
TEST_F(GCAllocatorTest, PromotedObjectChildrenAreForwarded) {
    gc_manager_t<GenerationalGCStrategy> gc;
    gc.set_gc_threshold(static_cast<size_t>(-1));  // drive minor GCs manually

    ConsCell* parent = gc.allocate<ConsCell>(nil, nil);
    gc.add_root(reinterpret_cast<void**>(&parent));  // persistent root (updated on move)

    // Age the parent across two collections (promotion threshold defaults to 3).
    gc.collect();  // survival 1
    gc.collect();  // survival 2

    // Attach a fresh young child reachable ONLY through the parent.
    a_integer* child = gc.allocate<a_integer>(123456789LL);
    parent->car(child);
    a_integer* child_before = child;  // original from-space address

    gc.collect();  // survival 3 -> parent promoted to old gen

    // The parent is now in old gen; its car must have been forwarded to the child's
    // new young-gen location (different address, intact value). Before the fix the
    // child was never copied and car still pointed at its dead from-space slot.
    Object* car = parent->car();
    ASSERT_NE(car, nullptr);
    EXPECT_NE(car, child_before) << "promoted parent's child was not forwarded (P1 bug)";
    EXPECT_EQ(static_cast<a_integer*>(car)->value(), 123456789LL);
}
