#include "pch.h"
#include "TestFixture.h"


// Access to global GC instance
extern gc_manager* gc;

// specific collection behavior, but conservative stack scanning finds stale pointers
// from test frames, preventing deterministic collection
TEST_F(ZombieSchemeTest, GCRootsPreventCollection) {
    // Note: interpreter and env are already protected by the test fixture
    size_t objects_before = gc->object_count();
    // Create an Object and add it as a root
    a_double* rooted = gc->allocate<a_double>(999.0);
    gc->add_root(reinterpret_cast<void**>(&rooted));
    //std::cout << "  Created rooted Object: " << rooted->value() << std::endl;
    // Create some temporary objects that are NOT rooted
    for (int i = 0; i < 10; i++) {
        gc->allocate<a_double>(i * 1.0);
    }
    size_t objects_after_alloc = gc->object_count();
    std::cout << "  Created 10 temporary objects" << std::endl;
    std::cout << "  Objects after allocation: " << objects_after_alloc << std::endl;
    // Force GC collection
    gc->collect();
    size_t objects_after_gc = gc->object_count();
    std::cout << "  Objects after GC: " << objects_after_gc << std::endl;
    // The rooted Object should still exist
    bool rooted_survived = (rooted->value() == 999.0);
    //std::cout << "  Rooted Object value: " << rooted->value() << std::endl;
    //std::cout << "  Rooted Object survived: " << (rooted_survived ? "YES" : "NO") << std::endl;
    //std::cout << "  Test: " << (rooted_survived ? "PASS" : "FAIL") << std::endl;
    // Clean up - remove root and collect to fully clean up for next test
    gc->remove_root(reinterpret_cast<void**>(&rooted));
    gc->collect();  // Collect the unrooted Object to leave clean state for next test
    ASSERT_TRUE(rooted_survived);
}
// Test passes in isolation but fails when run with full suite due to conservative stack scanning
TEST_F(ZombieSchemeTest, GCRootsAllowAccessAfterCollection) {
    // Create an Object and add it as a root
    a_double* rooted = gc->allocate<a_double>(1234.0);
    gc->add_root(reinterpret_cast<void**>(&rooted));
    //std::cout << "  Created rooted Object: " << rooted->value() << std::endl;
    // Force GC collection
    gc->collect();
    // Attempt to access the rooted Object
    double value_after_gc = rooted->value();
    //std::cout << "  Accessed rooted Object value after GC: " << value_after_gc << std::endl;
    // The value should be unchanged
    bool access_successful = (value_after_gc == 1234.0);
    //std::cout << "  Access successful: " << (access_successful ? "YES" : "NO") << std::endl;
    //std::cout << "  Test: " << (access_successful ? "PASS" : "FAIL") << std::endl;
    // Clean up
    gc->remove_root(reinterpret_cast<void**>(&rooted));
    ASSERT_TRUE(access_successful);
}
// Test passes in isolation but fails when run with full suite due to conservative stack scanning
TEST_F(ZombieSchemeTest, GCRootsDoNotLeakAfterRemoval) {
    // Note: interpreter and env are already protected by the test fixture
    // Create some permanent guard objects to ensure heap isn't empty
    // (prevents potential edge cases in GC)
    std::vector<a_double*> guards;
    for (int i = 0; i < 5; i++) {
        auto* guard = gc->allocate<a_double>(i * 100.0);
        gc->add_root(reinterpret_cast<void**>(&guard));
        guards.push_back(guard);
    }
    // Create the test Object and add it as a root
    a_double* rooted = gc->allocate<a_double>(5678.0);
    gc->add_root(reinterpret_cast<void**>(&rooted));
    // Force GC collection - Object should survive because it's rooted
    gc->collect();
    // Record state before removing root
    size_t objects_before = gc->object_count();
    auto stats_before = gc->get_stats();
    // Remove the root
    gc->remove_root(reinterpret_cast<void**>(&rooted));
    // Force GC collection again - Object should be collected now
    gc->collect();
    // Verify the Object was collected by checking GC statistics
    size_t objects_after = gc->object_count();
    auto stats_after = gc->get_stats();
    // The Object count should decrease by at least 1
    ASSERT_LT(objects_after, objects_before);
    // The number of objects collected should increase
    ASSERT_GT(stats_after.objects_collected, stats_before.objects_collected);
    // Clean up guard objects
    for (auto& guard : guards) {
        gc->remove_root(reinterpret_cast<void**>(&guard));
    }
    // Note: We do NOT access 'rooted' or 'guard' pointers after removal
    // because they may be freed memory (undefined behavior)
}
// Test passes in isolation but fails when run with full suite due to conservative stack scanning
TEST_F(ZombieSchemeTest, GCRootMultipleObjects) {
    // Note: interpreter and env are already protected by the test fixture
    // Create multiple objects and add them as roots
    const int num_roots = 5;
    a_double* roots[num_roots];
    for (int i = 0; i < num_roots; i++) {
        roots[i] = gc->allocate<a_double>(i * 10.0);
        gc->add_root(reinterpret_cast<void**>(&roots[i]));
    }
    // Force GC collection
    gc->collect();
    // Verify all rooted objects still exist
    bool all_survived = true;
    for (int i = 0; i < num_roots; i++) {
        if (roots[i]->value() != i * 10.0) {
            all_survived = false;
            break;
        }
    }
    //std::cout << "  All rooted objects survived: " << (all_survived ? "YES" : "NO") << std::endl;
    //std::cout << "  Test: " << (all_survived ? "PASS" : "FAIL") << std::endl;
    // Clean up
    for (int i = 0; i < num_roots; i++) {
        gc->remove_root(reinterpret_cast<void**>(&roots[i]));
    }
    ASSERT_TRUE(all_survived);
}
