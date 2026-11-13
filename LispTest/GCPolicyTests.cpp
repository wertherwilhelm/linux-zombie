#include "pch.h"
#include "gtest/gtest.h"
#include "gc_manager.h"
#include "gc_context.h"
#include "atom.h"
#include "MemoryManagement/policies/GCPolicy.h"

// Tests for GCPolicy (memory::policies::GCPolicy) as a thin adapter over the
// tracing collector.
//
// Regression context: GCPolicy::allocate() used to call the raw
// IMemoryManager::allocate(size, align) + placement-new. That produced objects
// with no gc_header and no registration with the collector, so they were
// invisible to GC (leaked, or double-freed if routed through reclaim). It also
// referenced a non-existent gc::current() symbol, so it could not even link.
//
// GCPolicy now delegates to IGarbageCollector::allocate<T>(), which installs the
// gc_header (type-specific tracer + destructor) and registers via on_allocate().
// A local gc_manager + gc_scope isolates these tests from the global GC.

using GCPolicy = memory::policies::GCPolicy;
using a_integer = atom<long long>;

TEST(GCPolicyIntegration, AllocatesTrackedObjects) {
    gc_manager local;
    gc_scope scope(&local);  // GCPolicy uses get_current_gc()

    size_t base = local.object_count();
    auto* a = GCPolicy::allocate<a_integer>(123LL);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->value(), 123LL);
    // The object must now be tracked by the collector (previously it was not).
    EXPECT_EQ(local.object_count(), base + 1);
}

TEST(GCPolicyIntegration, UnreachableObjectsAreCollected) {
    gc_manager local;
    gc_scope scope(&local);

    size_t base = local.object_count();
    for (int i = 0; i < 10; ++i) {
        auto* o = GCPolicy::allocate<a_integer>(static_cast<long long>(i));
        (void)o;  // intentionally not rooted
    }
    EXPECT_EQ(local.object_count(), base + 10);

    // Because the objects carry gc_headers and are tracked, a collection reclaims
    // the unreachable ones correctly (header-less objects would have crashed the
    // mark/sweep or leaked).
    local.collect();
    EXPECT_EQ(local.object_count(), base);
}

TEST(GCPolicyIntegration, DeallocateIsNoOp) {
    gc_manager local;
    gc_scope scope(&local);

    size_t base = local.object_count();
    auto* a = GCPolicy::allocate<a_integer>(55LL);
    GCPolicy::deallocate(a);  // must not free or desync tracking
    EXPECT_EQ(local.object_count(), base + 1);
    EXPECT_EQ(a->value(), 55LL);  // still valid
}
