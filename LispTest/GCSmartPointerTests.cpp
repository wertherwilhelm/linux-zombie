#include "pch.h"
#include "gtest/gtest.h"
#include "gc_manager.h"
#include "gc_ptr.h"
#include "gc_manager_template.h"
#include "Object.h"
#include "atom.h"
#include "cons_cell.h"
#include "constants.h"

// Access to global GC instance
extern gc_manager* gc;

// Type aliases for convenience (matches a_int and a_double in zombie_runtime_types.h)
using a_integer = atom<long long>;

// NOTE: operator<< for atom types is provided by atom.h template
// (std::ostream& operator<<(std::ostream& os, const atom<T>* t))
// No need to define a local operator<< - doing so causes ambiguity with ADL

class GCSmartPointerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Global environment already initialized init_lisp_constants() in main
        // Just ensure gc is valid
        ASSERT_NE(gc, nullptr);

        // Note: We don't call gc->collect() here because when running in the full suite,
        // previous tests (especially BytecodeVM) may leave thousands of objects allocated,
        // and gc->collect() becomes extremely slow with large Object graphs
    }

    void TearDown() override {
        // Don't shutdown_gc() - it's shared across all tests
        // Don't call gc->collect() either - let GC run automatically when needed
    }

    // Helper: Wrapper for gc->collect() for consistency
    // Performance issue fixed by using unordered_set in mark_sweep_strategy.cpp
    void safe_collect() {
        gc->collect();
    }
};

// Test gc_root<T> - RAII root guard
TEST_F(GCSmartPointerTest, GCRootBasic) {
    size_t initial_count = gc->object_count();

    {
        // Allocate Object and protect with gc_root
        gc_root<a_integer> num(gc->allocate<a_integer>(42));

        // Object should exist
        EXPECT_NE(num.get(), nullptr);
        EXPECT_EQ(num->value(), 42);

        // Allocate garbage
        for (int i = 0; i < 5; i++) {
            gc->allocate<a_integer>(i);
        }

        // Trigger GC (but skip if too many objects from previous tests)
        safe_collect();

        // Rooted Object should survive
        EXPECT_EQ(num->value(), 42);

    } // gc_root destructor unregisters

    // Clean up
    safe_collect();
}

// Test gc_root<T> move semantics (now shares root with copy semantics)
TEST_F(GCSmartPointerTest, GCRootMove) {
    gc_root<a_integer> num1(gc->allocate<a_integer>(42));
    EXPECT_EQ(num1->value(), 42);

    // Move construct (with refcounted roots, this transfers shared ownership)
    gc_root<a_integer> num2(std::move(num1));
    EXPECT_EQ(num2->value(), 42);

    // With copyable handles, move doesn't null the source
    // (It's now equivalent to copy due to shared_ptr semantics)

    // Move assign
    gc_root<a_integer> num3;
    num3 = std::move(num2);
    EXPECT_EQ(num3->value(), 42);

    // Verify Object is still rooted and survives GC
    safe_collect();
    EXPECT_EQ(num3->value(), 42);
}

// Test gc_root<T> reset
TEST_F(GCSmartPointerTest, GCRootReset) {
    gc_root<a_integer> num(gc->allocate<a_integer>(42));
    EXPECT_EQ(num->value(), 42);

    // Reset to new Object
    num.reset(gc->allocate<a_integer>(100));
    EXPECT_EQ(num->value(), 100);

    // Reset to null
    num.reset();
    EXPECT_EQ(static_cast<Object*>(num.get()), nullptr);
}

// Test gc_ptr<T> - Smart pointer with auto-rooting
TEST_F(GCSmartPointerTest, GCPtrBasic) {
    {
        gc_ptr<a_integer> num(gc->allocate<a_integer>(42));

        // Smart pointer interface
        EXPECT_NE(num.get(), nullptr);
        EXPECT_EQ(num->value(), 42);
        EXPECT_EQ((*num).value(), 42);

        // Boolean conversion
        EXPECT_TRUE(num);

        // Allocate garbage
        for (int i = 0; i < 5; i++) {
            gc->allocate<a_integer>(i);
        }

        // GC should preserve rooted Object
        safe_collect();
        EXPECT_EQ(num->value(), 42);

    } // gc_ptr destructor unroots
}

// Test gc_ptr<T> move semantics (now shares root with copy semantics)
TEST_F(GCSmartPointerTest, GCPtrMove) {
    gc_ptr<a_integer> num1(gc->allocate<a_integer>(42));

    // Move construct (with refcounted roots, this transfers shared ownership)
    gc_ptr<a_integer> num2(std::move(num1));
    EXPECT_EQ(num2->value(), 42);

    // With copyable handles, move doesn't null the source
    // (It's now equivalent to copy due to shared_ptr semantics)

    // Move assign
    gc_ptr<a_integer> num3;
    num3 = std::move(num2);
    EXPECT_EQ(num3->value(), 42);

    // Verify Object is still rooted and survives GC
    safe_collect();
    EXPECT_EQ(num3->value(), 42);
}

// Test gc_ptr<T> with ConsCell
TEST_F(GCSmartPointerTest, GCPtrWithConsCel) {
    gc_ptr<a_integer> car_val(gc->allocate<a_integer>(1));
    gc_ptr<a_integer> cdr_val(gc->allocate<a_integer>(2));

    gc_ptr<ConsCell> cell(gc->allocate<ConsCell>(car_val.get(), cdr_val.get()));

    // Access through smart pointer
    EXPECT_TRUE(cell->car() == car_val.get());
    EXPECT_TRUE(cell->cdr() == cdr_val.get());

    // All objects rooted, should survive GC
    gc->collect();

    EXPECT_TRUE(cell->car() == car_val.get());
}

// Test make_gc_ptr factory
TEST_F(GCSmartPointerTest, MakeGCPtr) {
    auto num = make_gc_ptr<a_integer>(42);

    EXPECT_TRUE(num);
    EXPECT_EQ(num->value(), 42);

    // Should survive GC (rooted)
    gc->collect();
    EXPECT_EQ(num->value(), 42);
}

// Test make_gc_root factory
TEST_F(GCSmartPointerTest, MakeGCRoot) {
    auto num = make_gc_root<a_integer>(100);

    EXPECT_TRUE(num);
    EXPECT_EQ(num->value(), 100);

    // Should survive GC (rooted)
    gc->collect();
    EXPECT_EQ(num->value(), 100);
}

// Test gc_ptr comparison operators
TEST_F(GCSmartPointerTest, GCPtrComparison) {
    auto num1 = make_gc_ptr<a_integer>(42);
    auto num2 = make_gc_ptr<a_integer>(42);
    gc_ptr<a_integer> null_ptr;

    // Different objects are not equal
    EXPECT_NE(num1, num2);

    // Null comparison
    EXPECT_EQ(null_ptr, nullptr);
    EXPECT_NE(num1, nullptr);
}

// Test gc_root with complex Object graph
TEST_F(GCSmartPointerTest, GCRootComplexGraph) {
    // Build a list: (1 2 3)
    gc_root<ConsCell> list(
        gc->allocate<ConsCell>(
            gc->allocate<a_integer>(1),
            gc->allocate<ConsCell>(
                gc->allocate<a_integer>(2),
                gc->allocate<ConsCell>(
                    gc->allocate<a_integer>(3),
                    nil
                )
            )
        )
    );

    // All referenced objects should survive GC
    gc->collect();

    // Verify list structure
    EXPECT_NE(list.get(), nullptr);
    a_integer* first = static_cast<a_integer*>(list->car());
    EXPECT_EQ(first->value(), 1);
}

// Test release() - transfer ownership out of gc_ptr
TEST_F(GCSmartPointerTest, GCPtrRelease) {
    auto num = make_gc_ptr<a_integer>(42);
    a_integer* raw_ptr = num.release();

    // gc_ptr is now empty
    EXPECT_FALSE(num);
    EXPECT_EQ(static_cast<Object*>(num.get()), nullptr);

    // Raw pointer is valid
    EXPECT_NE(static_cast<Object*>(raw_ptr), nullptr);
    EXPECT_EQ(raw_ptr->value(), 42);

    // Note: raw_ptr is no longer rooted, need to protect manually
    gc->add_root(reinterpret_cast<void**>(&raw_ptr));
    gc->collect();
    EXPECT_EQ(raw_ptr->value(), 42);
    gc->remove_root(reinterpret_cast<void**>(&raw_ptr));
}

// Test nested gc_root usage
TEST_F(GCSmartPointerTest, NestedGCRoots) {
    gc_root<a_integer> outer(gc->allocate<a_integer>(1));

    {
        gc_root<a_integer> inner(gc->allocate<a_integer>(2));

        // Both rooted
        safe_collect();
        EXPECT_EQ(outer->value(), 1);
        EXPECT_EQ(inner->value(), 2);

    } // inner unrooted

    // Only outer rooted
    gc->collect();
    EXPECT_EQ(outer->value(), 1);
}

// Test gc_ptr in std::vector
TEST_F(GCSmartPointerTest, GCPtrInVector) {
    std::vector<gc_ptr<a_integer>> nums;

    // Add several gc_ptrs to vector
    for (int i = 0; i < 5; i++) {
        nums.push_back(make_gc_ptr<a_integer>(i * 10));
    }

    // All objects should be rooted and survive GC
    gc->collect();

    // Verify all values
    for (size_t i = 0; i < nums.size(); i++) {
        EXPECT_EQ(nums[i]->value(), static_cast<long>(i * 10));
    }
}

// Test gc_manager::make_gc_ptr method
TEST_F(GCSmartPointerTest, GCManagerMakeGCPtr) {
    // Use gc->make_gc_ptr instead of make_gc_ptr or gc_ptr constructor
    auto num = gc->make_gc_ptr<a_integer>(42);

    EXPECT_TRUE(num);
    EXPECT_EQ(num->value(), 42);

    // Should survive GC (rooted)
    gc->collect();
    EXPECT_EQ(num->value(), 42);
}

// Test gc_manager::make_gc_root method
TEST_F(GCSmartPointerTest, GCManagerMakeGCRoot) {
    // Use gc->make_gc_root instead of make_gc_root or gc_root constructor
    auto num = gc->make_gc_root<a_integer>(100);

    EXPECT_TRUE(num);
    EXPECT_EQ(num->value(), 100);

    // Should survive GC (rooted)
    gc->collect();
    EXPECT_EQ(num->value(), 100);
}

// Test gc_manager methods with complex Object
TEST_F(GCSmartPointerTest, GCManagerMakeComplexObject) {
    // Build a list using gc->make_gc_ptr: (1 2 3)
    auto list = gc->make_gc_ptr<ConsCell>(
        gc->allocate<a_integer>(1),
        gc->allocate<ConsCell>(
            gc->allocate<a_integer>(2),
            gc->allocate<ConsCell>(
                gc->allocate<a_integer>(3),
                nil
            )
        )
    );

    // All referenced objects should survive GC
    gc->collect();

    // Verify list structure
    EXPECT_NE(list.get(), nullptr);
    a_integer* first = static_cast<a_integer*>(list->car());
    EXPECT_EQ(first->value(), 1);
}

// Test refcounting: Multiple handles to same Object
TEST_F(GCSmartPointerTest, RefcountingMultipleHandles) {
    a_integer* raw_ptr = gc->allocate<a_integer>(42);

    {
        // Create first handle - should add root with refcount=1
        gc_root<a_integer> handle1(raw_ptr);
        EXPECT_EQ(handle1->value(), 42);

        {
            // Create second handle to SAME Object - should increment refcount to 2
            gc_root<a_integer> handle2(raw_ptr);
            EXPECT_EQ(handle2->value(), 42);

            // Allocate garbage
            for (int i = 0; i < 5; i++) {
                gc->allocate<a_integer>(i);
            }

            // GC should NOT collect raw_ptr (refcount=2)
            safe_collect();
            EXPECT_EQ(handle1->value(), 42);
            EXPECT_EQ(handle2->value(), 42);

        } // handle2 dies - should decrement refcount to 1 (NOT remove root)

        // Object should STILL be rooted (refcount=1)
        safe_collect();
        EXPECT_EQ(handle1->value(), 42);

    } // handle1 dies - should decrement refcount to 0 and remove root

    // Now Object is unrooted and should be collectable
    // (We can't verify collection without triggering use-after-free)
}

// Test refcounting: Copy semantics
TEST_F(GCSmartPointerTest, RefcountingCopySemantics) {
    a_integer* raw_ptr = gc->allocate<a_integer>(100);

    gc_root<a_integer> handle1(raw_ptr);
    gc_root<a_integer> handle2 = handle1;  // Copy - both should share root

    EXPECT_EQ(handle1->value(), 100);
    EXPECT_EQ(handle2->value(), 100);

    // Both should keep Object rooted
    safe_collect();
    EXPECT_EQ(handle1->value(), 100);
    EXPECT_EQ(handle2->value(), 100);
}
