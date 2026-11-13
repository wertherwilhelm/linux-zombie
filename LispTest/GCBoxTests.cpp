#include "pch.h"
#include "gtest/gtest.h"
#include "gc_manager.h"
#include "gc_box.h"
#include "Object.h"
#include "atom.h"
#include "cons_cell.h"
#include "constants.h"


// Access to global GC instance
extern gc_manager* gc;

// Type aliases for convenience
using a_integer = atom<long long>;
using a_double = atom<double>;
class GCBoxTest : public ::testing::Test {
protected:
    void SetUp() override {
        init_lisp_constants();
    }
    void TearDown() override {
        shutdown_gc();
    }
};
// Test that gc_box::allocate creates an Object with a header
TEST_F(GCBoxTest, AllocateCreatesHeader) {
    // Allocate via gc_box
    a_integer* num = gc_box::allocate<a_integer>(42);
    ASSERT_NE(num, nullptr);
    // Should be able to get header
    gc_header* header = gc_box::get_header(num);
    ASSERT_NE(header, nullptr);
    // Header should have a tracer
    EXPECT_NE(header->tracer, nullptr);
    // Header should have size
    EXPECT_EQ(header->size, sizeof(a_integer));
    // Object should have correct value
    EXPECT_EQ(num->value(), 42);
}
// Test that gc_manager::allocate uses gc_box
TEST_F(GCBoxTest, GCManagerUsesGCBox) {
    // Allocate via gc_manager
    a_integer* num = gc->allocate<a_integer>(100);
    ASSERT_NE(num, nullptr);
    // Should have a header (from gc_box)
    gc_header* header = gc_box::get_header(num);
    ASSERT_NE(header, nullptr);
    // Header should have tracer
    EXPECT_NE(header->tracer, nullptr);
    // Object value should be correct
    EXPECT_EQ(num->value(), 100);
}
// Test header metadata
TEST_F(GCBoxTest, HeaderMetadata) {
    a_double* num = gc_box::allocate<a_double>(3.14);
    gc_header* header = gc_box::get_header(num);
    // Check initial state
    EXPECT_FALSE(header->marked);
    EXPECT_EQ(header->size, sizeof(a_double));
    // Test marking
    gc_box::set_marked(num, true);
    EXPECT_TRUE(gc_box::is_marked(num));
    EXPECT_TRUE(header->marked);
    // Test unmarking
    gc_box::set_marked(num, false);
    EXPECT_FALSE(gc_box::is_marked(num));
    EXPECT_FALSE(header->marked);
    // Test size retrieval
    EXPECT_EQ(gc_box::get_size(num), sizeof(a_double));
}
// Test deallocation
TEST_F(GCBoxTest, DeallocateWorks) {
    // Allocate Object
    a_integer* num = gc_box::allocate<a_integer>(42);
    ASSERT_NE(num, nullptr);
    // Verify it has correct value
    EXPECT_EQ(num->value(), 42);
    // Deallocate (should not crash)
    gc_box::deallocate(num);
    // Note: Can't verify Object is gone without using after free (undefined behavior)
}
// Test that different types get different tracers
TEST_F(GCBoxTest, DifferentTypesHaveDifferentTracers) {
    a_integer* num = gc_box::allocate<a_integer>(42);
    a_double* dbl = gc_box::allocate<a_double>(3.14);
    gc_header* num_header = gc_box::get_header(num);
    gc_header* dbl_header = gc_box::get_header(dbl);
    // Different types should have different tracer functions
    EXPECT_NE(num_header->tracer, dbl_header->tracer);
}
// Test ConsCell allocation
TEST_F(GCBoxTest, ConsCellAllocation) {
    a_integer* car_val = gc->allocate<a_integer>(1);
    a_integer* cdr_val = gc->allocate<a_integer>(2);
    ConsCell* cell = gc->allocate<ConsCell>(car_val, cdr_val);
    ASSERT_NE(cell, nullptr);
    // Should have header
    gc_header* header = gc_box::get_header(cell);
    ASSERT_NE(header, nullptr);
    EXPECT_NE(header->tracer, nullptr);
    // Cell should work correctly (compare pointers directly)
    EXPECT_TRUE(cell->car() == car_val);
    EXPECT_TRUE(cell->cdr() == cdr_val);
}
// Test null pointer handling
TEST_F(GCBoxTest, NullPointerHandling) {
    // get_header on null should return null
    EXPECT_EQ(gc_box::get_header((void*)nullptr), nullptr);
    // is_marked on null should return false
    EXPECT_FALSE(gc_box::is_marked(nullptr));
    // set_marked on null should not crash
    gc_box::set_marked(nullptr, true);
    // get_size on null should return 0
    EXPECT_EQ(gc_box::get_size(nullptr), 0);
    // deallocate on null should not crash
    gc_box::deallocate(nullptr);
}
// Test that gc_box works with GC collection
TEST_F(GCBoxTest, WorksWithGCCollection) {
    size_t initial_count = gc->object_count();
    // Allocate some objects
    for (int i = 0; i < 10; i++) {
        gc->allocate<a_integer>(i);
    }
    EXPECT_EQ(gc->object_count(), initial_count + 10);
    // Trigger collection (no roots, so all should be collected)
    gc->collect();
    // All temporary objects should be collected
    EXPECT_LT(gc->object_count(), initial_count + 10);
}
// Test that marked objects survive collection
TEST_F(GCBoxTest, MarkedObjectsSurviveCollection) {
    size_t initial_count = gc->object_count();
    // Allocate and root an Object
    a_integer* num = gc->allocate<a_integer>(42);
    gc->add_root(num);
    // Allocate some garbage
    for (int i = 0; i < 5; i++) {
        gc->allocate<a_integer>(i);
    }
    size_t before_gc = gc->object_count();
    // Trigger collection
    gc->collect();
    size_t after_gc = gc->object_count();
    // Rooted Object should survive
    EXPECT_LT(after_gc, before_gc);
    EXPECT_GE(after_gc, initial_count + 1);  // At least our rooted Object
    // Verify Object is still valid
    EXPECT_EQ(num->value(), 42);
    // Clean up
    gc->remove_root(num);
}
// Test header alignment
TEST_F(GCBoxTest, HeaderAlignment) {
    a_integer* num = gc_box::allocate<a_integer>(42);
    // Header should be immediately before Object
    gc_header* header = gc_box::get_header(num);
    void* header_end = reinterpret_cast<char*>(header) + sizeof(gc_header);
    void* obj_start = reinterpret_cast<void*>(num);
    EXPECT_EQ(header_end, obj_start);
}
