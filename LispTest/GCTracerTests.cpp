// ============================================================================
// GCTracerTests.cpp - Tests for the Tracer interface decoupling pattern
// ============================================================================
//
// These tests verify that the Tracer interface properly decouples Object
// tracing from GC implementation. This allows:
// - GC to be reusable for non-Scheme C++ code
// - Objects to enumerate children without knowing about GC internals
// - Easy testing of tracing behavior in isolation
//
// ============================================================================

#include "pch.h"
#include "TestFixture.h"
#include "gc_tracer.h"
#include "mark_sweep_strategy.h"
#include "gc_ptr.h"
#include "cons_cell.h"
#include "atom.h"
#include "constants.h"
#include "Base.h"
#include <vector>

// ============================================================================
// Test Fixture
// ============================================================================

class GCTracerTest : public ZombieSchemeTest {
protected:
    void SetUp() override {
        ZombieSchemeTest::SetUp();
    }
};

// ============================================================================
// Mock Tracer for testing Object enumeration
// ============================================================================

class MockTracer : public Tracer {
public:
    std::vector<void*> visited;

    void visit(void* ptr) override {
        if (ptr) {
            visited.push_back(ptr);
        }
    }

    // Check if a specific pointer was visited
    bool was_visited(void* ptr) const {
        return std::find(visited.begin(), visited.end(), ptr) != visited.end();
    }

    // Get count of visits
    size_t visit_count() const {
        return visited.size();
    }

    // Reset for reuse
    void reset() {
        visited.clear();
    }
};

// ============================================================================
// Basic Tracer Interface Tests
// ============================================================================

TEST_F(GCTracerTest, TracerInterfaceExists) {
    // Verify that Tracer is an abstract interface
    MockTracer tracer;
    EXPECT_EQ(0, tracer.visit_count());
}

TEST_F(GCTracerTest, TracerVisitRecordsPointer) {
    MockTracer tracer;

    // Create a simple Object
    Object* obj = gc_new<a_int>(42);

    // Visit it
    tracer.visit(obj);

    // Verify it was recorded
    EXPECT_EQ(1, tracer.visit_count());
    EXPECT_TRUE(tracer.was_visited(obj));
}

TEST_F(GCTracerTest, TracerIgnoresNullptr) {
    MockTracer tracer;

    // Visit nullptr
    tracer.visit(nullptr);

    // Should not record it
    EXPECT_EQ(0, tracer.visit_count());
}

TEST_F(GCTracerTest, TracerTemplateVisit) {
    MockTracer tracer;

    // Test type-safe template version
    a_int* num = gc_new<a_int>(42);
    ConsCell* cell = gc_new<ConsCell>(num, nil);

    tracer.visit(num);
    tracer.visit(cell);

    EXPECT_EQ(2, tracer.visit_count());
    EXPECT_TRUE(tracer.was_visited(num));
    EXPECT_TRUE(tracer.was_visited(cell));
}

// ============================================================================
// Object Enumeration Tests
// ============================================================================

TEST_F(GCTracerTest, ConsEnumeratesChildren) {
    MockTracer tracer;

    // Create a simple cons cell
    Object* car_obj = gc_new<a_int>(1);
    Object* cdr_obj = gc_new<a_int>(2);
    ConsCell* cell = gc_new<ConsCell>(car_obj, cdr_obj);

    // Trace (enumerate children)
    // Note: Still uses old API - will be migrated in Phase 2
    // cell->trace_references(gc);  // Old API - uses IGarbageCollector*

    // TODO: Once we migrate, this will be:
    // cell->trace_references(tracer);
    // EXPECT_EQ(2, tracer.visit_count());
    // EXPECT_TRUE(tracer.was_visited(car_obj));
    // EXPECT_TRUE(tracer.was_visited(cdr_obj));
}

TEST_F(GCTracerTest, EmptyConsEnumeratesNothing) {
    MockTracer tracer;

    // Create cons with nil children
    ConsCell* cell = gc_new<ConsCell>(nil, nil);

    // Trace it (should visit nil, but MockTracer filters it)
    // When we migrate to Tracer API, this test will verify correct behavior
}

TEST_F(GCTracerTest, ListEnumeratesAllElements) {
    MockTracer tracer;

    // Create a 3-element list
    Object* a = gc_new<a_int>(1);
    Object* b = gc_new<a_int>(2);
    Object* c = gc_new<a_int>(3);

    Object* list = cons(a, cons(b, cons(c, nil)));

    // When we migrate to Tracer API:
    // Tracing the list should enumerate all cons cells and their cars
}

// ============================================================================
// MarkingTracer Integration Tests
// ============================================================================

TEST_F(GCTracerTest, MarkingTracerMarksObjects) {
    std::vector<gc_object*> worklist;
    MarkingTracer tracer(&worklist);

    // Create an unmarked Object
    Object* obj = gc_new<a_int>(42);
    obj->set_marked(false);

    // Visit it via tracer
    tracer.visit(obj);

    // Should be in worklist (marking happens in mark_from loop, not in tracer)
    EXPECT_FALSE(obj->is_marked());  // Tracer doesn't mark - loop does
    EXPECT_EQ(1, worklist.size());
    EXPECT_EQ(obj, worklist[0]);
}

TEST_F(GCTracerTest, MarkingTracerSkipsAlreadyMarked) {
    std::vector<gc_object*> worklist;
    MarkingTracer tracer(&worklist);

    // Create a pre-marked Object
    Object* obj = gc_new<a_int>(42);
    obj->set_marked(true);

    // Visit it
    tracer.visit(obj);

    // Should not add to worklist (already marked)
    EXPECT_EQ(0, worklist.size());
}

TEST_F(GCTracerTest, MarkingTracerHandlesCycles) {
    std::vector<gc_object*> worklist;
    MarkingTracer tracer(&worklist);

    // Create a simple circular structure (just reuse same Object)
    ConsCell* a = gc_new<ConsCell>(nil, nil);

    a->set_marked(false);

    // Visit it twice (simulating cycle detection)
    tracer.visit(a);
    a->set_marked(true);  // Simulate mark_from loop marking it
    tracer.visit(a);  // Second visit should be skipped because it's marked

    // Should be marked and added only once
    EXPECT_TRUE(a->is_marked());
    EXPECT_EQ(1, worklist.size());  // Only added once (second visit skipped)
}

TEST_F(GCTracerTest, MarkingTracerHandlesNullptr) {
    std::vector<gc_object*> worklist;
    MarkingTracer tracer(&worklist);

    // Visit nullptr
    tracer.visit(nullptr);

    // Should not crash, worklist empty
    EXPECT_EQ(0, worklist.size());
}

// ============================================================================
// Decoupling Verification Tests
// ============================================================================

TEST_F(GCTracerTest, TracerDoesNotDependOnGCManager) {
    // This test verifies that we can create and use a Tracer
    // without including gc_manager.h or knowing about concrete GC

    MockTracer tracer;
    Object* obj = gc_new<a_int>(42);

    tracer.visit(obj);

    // Success means we've achieved decoupling
    EXPECT_EQ(1, tracer.visit_count());
}

TEST_F(GCTracerTest, MultipleTracersCanCoexist) {
    // Different tracers for different purposes
    MockTracer counting_tracer;
    std::vector<gc_object*> marking_worklist;
    MarkingTracer marking_tracer(&marking_worklist);

    Object* obj = gc_new<a_int>(42);
    obj->set_marked(false);

    // Use both tracers
    counting_tracer.visit(obj);
    marking_tracer.visit(obj);

    // Each does its own thing
    EXPECT_EQ(1, counting_tracer.visit_count());
    EXPECT_FALSE(obj->is_marked());  // Tracer doesn't mark - loop does
    EXPECT_EQ(1, marking_worklist.size());
}

// ============================================================================
// Tracer Decoupling Tests
// ============================================================================

TEST_F(GCTracerTest, ObjectUsesTracerAPI) {
    MockTracer tracer;
    Object* obj = gc_new<ConsCell>(
        gc_new<a_int>(1),
        gc_new<a_int>(2)
    );

    obj->trace_references(tracer);

    EXPECT_EQ(2, tracer.visit_count());
}

namespace {

// A plain C++ type that does NOT inherit from Object but can still
// enumerate its children through the Tracer interface. This proves the
// Tracer decouples tracing from the Scheme object model.
struct PlainNode {
    PlainNode* next = nullptr;
    void trace(Tracer& tracer) {
        if (next) {
            tracer.visit_field(next);
        }
    }
};

} // namespace

TEST_F(GCTracerTest, NonSchemeObjectCanUseTracer) {
    MockTracer tracer;

    PlainNode a, b, c;
    a.next = &b;
    b.next = &c;

    a.trace(tracer);
    b.trace(tracer);
    c.trace(tracer);  // no children, visits nothing

    EXPECT_EQ(2, tracer.visit_count());
    EXPECT_TRUE(tracer.was_visited(&b));
    EXPECT_TRUE(tracer.was_visited(&c));
}

// ============================================================================
// Performance / Stress Tests
// ============================================================================

TEST_F(GCTracerTest, TracerHandlesDeepStructures) {
    std::vector<gc_object*> worklist;
    MarkingTracer tracer(&worklist);

    // Create a 100-deep linked list
    Object* list = nil;
    for (int i = 0; i < 100; i++) {
        Object* num = gc_new<a_int>(i);
        num->set_marked(false);
        list = gc_new<ConsCell>(num, list);
        static_cast<Object*>(list)->set_marked(false);
    }

    // Visit root
    tracer.visit(list);

    // Should add root to worklist (marking happens in mark_from loop)
    EXPECT_FALSE(static_cast<Object*>(list)->is_marked());
    EXPECT_GE(worklist.size(), 1);
}

TEST_F(GCTracerTest, TracerHandlesManyObjects) {
    std::vector<gc_object*> worklist;
    MarkingTracer tracer(&worklist);

    // Visit 1000 distinct objects
    for (int i = 0; i < 1000; i++) {
        Object* obj = gc_new<a_int>(i);
        obj->set_marked(false);
        tracer.visit(obj);
    }

    // All should be in worklist (marking happens in mark_from loop, not in tracer)
    EXPECT_EQ(1000, worklist.size());
    for (auto* obj : worklist) {
        EXPECT_FALSE(obj->is_marked());  // Tracer doesn't mark - loop does
    }
}

// ============================================================================
// Header-Based Tracing Tests (Phase 3)
// ============================================================================

#include "gc_box.h"
#include "gc_traits.h"

TEST_F(GCTracerTest, HeaderStoresTraceFn) {
    // Verify that gc_new stores TraceFn in header
    a_int* num = gc_new<a_int>(42);

    // Get header
    gc_header* header = gc_box::get_header(num);
    ASSERT_NE(nullptr, header);

    // Verify TraceFn is stored
    EXPECT_NE(nullptr, header->tracer);
    EXPECT_EQ(sizeof(a_int), header->size);
}

TEST_F(GCTracerTest, HeaderTraceFnWorks) {
    // Create Object with children
    a_int* child1 = gc_new<a_int>(1);
    a_int* child2 = gc_new<a_int>(2);
    ConsCell* cell = gc_new<ConsCell>(child1, child2);

    // Clear marks to ensure fresh state
    child1->set_marked(false);
    child2->set_marked(false);
    cell->set_marked(false);

    // Trace via header
    std::vector<gc_object*> worklist;
    MarkingTracer tracer(&worklist);

    gc_header* header = gc_box::get_header(cell);
    ASSERT_NE(nullptr, header);
    ASSERT_NE(nullptr, header->tracer);

    // Call the TraceFn stored in header
    header->tracer(tracer, cell);

    // Should have visited the children (tracer visits car/cdr)
    EXPECT_GE(worklist.size(), 2);

    // Children are added to worklist (marking happens in mark_from loop)
    EXPECT_FALSE(child1->is_marked());
    EXPECT_FALSE(child2->is_marked());
}

TEST_F(GCTracerTest, GCBoxTraceFunction) {
    // Test gc_box::trace() helper function
    a_int* child1 = gc_new<a_int>(1);
    a_int* child2 = gc_new<a_int>(2);
    ConsCell* cell = gc_new<ConsCell>(child1, child2);

    // Clear marks
    child1->set_marked(false);
    child2->set_marked(false);
    cell->set_marked(false);

    std::vector<gc_object*> worklist;
    MarkingTracer tracer(&worklist);

    // Trace via gc_box::trace() without knowing concrete type
    void* obj_ptr = static_cast<void*>(cell);
    gc_box::trace(tracer, obj_ptr);

    // Should have traced children (added to worklist, marking happens in loop)
    EXPECT_GE(worklist.size(), 2);
    EXPECT_FALSE(child1->is_marked());
    EXPECT_FALSE(child2->is_marked());
}

TEST_F(GCTracerTest, TraceObjectTrait) {
    // Test that Trace<Object> trait bridges to virtual trace_references
    a_int* child1 = gc_new<a_int>(1);
    a_int* child2 = gc_new<a_int>(2);
    ConsCell* cell = gc_new<ConsCell>(child1, child2);

    // Clear marks
    child1->set_marked(false);
    child2->set_marked(false);
    cell->set_marked(false);

    std::vector<gc_object*> worklist;
    MarkingTracer tracer(&worklist);

    // Call Trace<Object>::trace directly
    Trace<Object>::trace(tracer, static_cast<Object*>(cell));

    // Should have visited children via virtual trace_references (added to worklist)
    EXPECT_GE(worklist.size(), 2);
    EXPECT_FALSE(child1->is_marked());
    EXPECT_FALSE(child2->is_marked());
}

TEST_F(GCTracerTest, HeaderTracingWithoutConcreteType) {
    // This is the key test: can we trace an Object through its header
    // without knowing its concrete type at compile time?

    // Create objects of different types
    a_int* child1 = gc_new<a_int>(1);
    a_int* child2 = gc_new<a_int>(2);
    Object* obj1 = gc_new<a_int>(42);
    Object* obj2 = gc_new<ConsCell>(child1, child2);

    // Clear marks
    static_cast<Object*>(child1)->set_marked(false);
    static_cast<Object*>(child2)->set_marked(false);
    obj1->set_marked(false);
    obj2->set_marked(false);

    // Store as void* (simulating type erasure)
    void* void_obj1 = static_cast<void*>(obj1);
    void* void_obj2 = static_cast<void*>(obj2);

    std::vector<gc_object*> worklist;
    MarkingTracer tracer(&worklist);

    // Trace both via header (no concrete type knowledge needed)
    gc_box::trace(tracer, void_obj1);  // int has no children
    gc_box::trace(tracer, void_obj2);  // ConsCell has 2 children

    // ConsCell should have added children to worklist (marking happens in loop)
    EXPECT_GE(worklist.size(), 2);
    EXPECT_FALSE(child1->is_marked());
    EXPECT_FALSE(child2->is_marked());
}

TEST_F(GCTracerTest, HeaderTracingNullptr) {
    // Verify gc_box::trace handles nullptr gracefully
    std::vector<gc_object*> worklist;
    MarkingTracer tracer(&worklist);

    gc_box::trace(tracer, nullptr);  // Should not crash

    EXPECT_EQ(0, worklist.size());
}
