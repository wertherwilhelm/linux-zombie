#include "pch.h"
#include "gtest/gtest.h"
#include "constants.h"
#include "gc_manager.h"


// Access to global GC instance
extern gc_manager* gc;

// Global test environment to ensure init_lisp_constants() is called
// exactly once before any tests run, regardless of filtering
class GlobalTestEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        // Initialize Lisp constants once for the entire test run
        init_lisp_constants();
        // Increase GC threshold to prevent automatic collections during tests
        // Conservative stack scanning can interact poorly with automatic GC
        // during test execution, causing false retention of freed objects
        gc->set_gc_threshold(100 * 1024 * 1024);  // 100MB threshold
    }
    void TearDown() override {
        // Optionally clean up (shutdown_gc() if needed)
        // For now, let process exit handle cleanup
    }
};
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    // Register global test environment
    ::testing::AddGlobalTestEnvironment(new GlobalTestEnvironment());
    return RUN_ALL_TESTS();
}
