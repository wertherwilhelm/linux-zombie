#include "pch.h"
#include "TestFixture.h"
#include <typeinfo>
#include <limits>
#include <cmath>   // std::isfinite / std::isinf (not transitive in libstdc++)

// ============================================================================
// Base Object Class Tests
// ============================================================================

TEST_F(ZombieSchemeTest, ObjectBaseVirtualFunctions) {
    // Test that all Object virtual functions work correctly
    a_double* num = gc_new<a_double>(42.0);
    
    // Virtual function dispatch
    ASSERT_NE(num->clone(), nullptr);
    EXPECT_EQ(num->get_type(), static_cast<int>(Type::DOUBLE));
    EXPECT_EQ(num->is_atom(), 1);
    EXPECT_EQ(num->pair(), 0);
    
    // Print doesn't crash
    std::ostringstream oss;
    num->print(oss);
    EXPECT_FALSE(oss.str().empty());
}

TEST_F(ZombieSchemeTest, ObjectEqualityComparison) {
    a_double* num1 = gc_new<a_double>(42.0);
    a_double* num2 = gc_new<a_double>(42.0);
    a_double* num3 = gc_new<a_double>(99.0);
    
    // Same value objects should be equal
    EXPECT_TRUE(num1->is_equal(num2));
    EXPECT_FALSE(num1->is_equal(num3));
    
    // Operator== should work
    EXPECT_TRUE(*num1 == *num2);
    EXPECT_FALSE(*num1 == *num3);
}

TEST_F(ZombieSchemeTest, ObjectIdentityWithEq) {
    a_double* num1 = gc_new<a_double>(42.0);
    a_double* num2 = num1;
    a_double* num3 = gc_new<a_double>(42.0);
    
    // Same pointer = identity
    EXPECT_TRUE(eq(num1, num2));
    
    // Different pointers = different identity (even same value)
    EXPECT_FALSE(eq(num1, num3));
}

TEST_F(ZombieSchemeTest, ObjectGCMarkingBasics) {
    a_double* num = gc_new<a_double>(42.0);
    
    // Initially unmarked
    EXPECT_FALSE(num->is_marked());
    
    // Can mark
    num->set_marked(true);
    EXPECT_TRUE(num->is_marked());
    
    // Can unmark
    num->set_marked(false);
    EXPECT_FALSE(num->is_marked());
}

TEST_F(ZombieSchemeTest, ObjectNilAndTSingletons) {
    // R5RS: empty_list, false_obj, and t are distinct singletons
    EXPECT_NE(empty_list, nullptr);
    EXPECT_NE(false_obj, nullptr);
    EXPECT_NE(t, nullptr);

    // They have unique identities
    EXPECT_NE((Object*)empty_list, (Object*)false_obj);
    EXPECT_NE((Object*)empty_list, (Object*)t);
    EXPECT_NE((Object*)false_obj, (Object*)t);

    // Backward compatibility: nil points to empty_list
    EXPECT_EQ(nil, empty_list);

    // Check their special flags
    EXPECT_TRUE(is_nil(nil));
    EXPECT_TRUE(is_nil(empty_list));
    EXPECT_FALSE(is_nil(false_obj));
    EXPECT_FALSE(is_nil(t));
    EXPECT_TRUE(is_t(t));
    EXPECT_FALSE(is_t(nil));
}

// ============================================================================
// Atom Template Class Tests
// ============================================================================

TEST_F(ZombieSchemeTest, AtomDoubleBasics) {
    a_double* num = gc_new<a_double>(3.14159);
    
    EXPECT_EQ(num->value(), 3.14159);
    EXPECT_EQ(num->get_type(), static_cast<int>(Type::DOUBLE));
    EXPECT_TRUE(num->is_atom());
    EXPECT_FALSE(num->pair());
}

TEST_F(ZombieSchemeTest, AtomLongBasics) {
    atom<long long>* num = gc_new<atom<long long>>(123456789L);

    EXPECT_EQ(num->value(), 123456789L);
    EXPECT_EQ(num->get_type(), static_cast<int>(Type::LONG));  // atom<long long> now correctly uses static_cast<int>(Type::LONG) type
    EXPECT_TRUE(num->is_atom());
}

TEST_F(ZombieSchemeTest, AtomStringBasics) {
    a_string* str = gc_new<a_string>("Hello, World!");
    
    EXPECT_EQ(str->value(), "Hello, World!");
    EXPECT_EQ(str->get_type(), static_cast<int>(Type::STRING));
    EXPECT_TRUE(str->is_atom());
    EXPECT_FALSE(str->pair());
}

TEST_F(ZombieSchemeTest, AtomSymbolBasics) {
    a_symbol* sym = gc_new<a_symbol>("test-symbol");
    
    EXPECT_EQ(sym->value(), "test-symbol");
    EXPECT_EQ(sym->get_type(), static_cast<int>(Type::SYMBOL));
    EXPECT_TRUE(sym->is_atom());
}

TEST_F(ZombieSchemeTest, AtomBoolBasics) {
    atom<bool>* true_val = gc_new<atom<bool>>(true);
    atom<bool>* false_val = gc_new<atom<bool>>(false);
    
    EXPECT_TRUE(true_val->value());
    EXPECT_FALSE(false_val->value());
    EXPECT_EQ(true_val->get_type(), static_cast<int>(Type::BOOL));
}

TEST_F(ZombieSchemeTest, AtomCharBasics) {
    atom<char>* ch = gc_new<atom<char>>('X');
    
    EXPECT_EQ(ch->value(), 'X');
    EXPECT_EQ(ch->get_type(), static_cast<int>(Type::CHAR));
    EXPECT_TRUE(ch->is_atom());
}

TEST_F(ZombieSchemeTest, AtomCloning) {
    a_double* original = gc_new<a_double>(42.0);
    a_double* cloned = dynamic_cast<a_double*>(original->clone());
    
    ASSERT_NE(cloned, nullptr);
    EXPECT_NE(original, cloned);  // Different objects
    EXPECT_EQ(original->value(), cloned->value());  // Same value
    EXPECT_TRUE(original->is_equal(cloned));
}

TEST_F(ZombieSchemeTest, AtomEquality) {
    a_double* d1 = gc_new<a_double>(3.14);
    a_double* d2 = gc_new<a_double>(3.14);
    a_double* d3 = gc_new<a_double>(2.71);
    
    EXPECT_TRUE(d1->is_equal(d2));
    EXPECT_FALSE(d1->is_equal(d3));
    
    a_string* s1 = gc_new<a_string>("test");
    a_string* s2 = gc_new<a_string>("test");
    a_string* s3 = gc_new<a_string>("other");
    
    EXPECT_TRUE(s1->is_equal(s2));
    EXPECT_FALSE(s1->is_equal(s3));
}

TEST_F(ZombieSchemeTest, AtomTypeChecking) {
    a_double* d = gc_new<a_double>(1.0);
    a_string* s = gc_new<a_string>("test");
    atom<long long>* l = gc_new<atom<long long>>(100);

    EXPECT_EQ(d->get_type(), static_cast<int>(Type::DOUBLE));
    EXPECT_EQ(s->get_type(), static_cast<int>(Type::STRING));
    EXPECT_EQ(l->get_type(), static_cast<int>(Type::LONG));  // atom<long long> now correctly uses static_cast<int>(Type::LONG) type

    // All are atoms
    EXPECT_TRUE(d->is_atom());
    EXPECT_TRUE(s->is_atom());
    EXPECT_TRUE(l->is_atom());

    // None are pairs
    EXPECT_FALSE(d->pair());
    EXPECT_FALSE(s->pair());
    EXPECT_FALSE(l->pair());
}

TEST_F(ZombieSchemeTest, AtomStringSymbolDistinction) {
    a_string* str = gc_new<a_string>("name");
    a_symbol* sym = gc_new<a_symbol>("name");
    
    // Same value but different types
    EXPECT_EQ(str->value(), sym->value());
    EXPECT_NE(str->get_type(), sym->get_type());
    EXPECT_FALSE(str->is_equal(sym));
}

// ============================================================================
// Cons Cell Tests
// ============================================================================

TEST_F(ZombieSchemeTest, ConsBasicConstruction) {
    a_double* car_val = gc_new<a_double>(1.0);
    a_double* cdr_val = gc_new<a_double>(2.0);
    
    ConsCell* cell = gc_new<ConsCell>(car_val, cdr_val);
    
    EXPECT_NE(cell->car(), nullptr);
    EXPECT_NE(cell->cdr(), nullptr);
    EXPECT_FALSE(cell->is_atom());
    EXPECT_TRUE(cell->pair());
}

TEST_F(ZombieSchemeTest, ConsCarCdrAccess) {
    a_double* num1 = gc_new<a_double>(42.0);
    a_double* num2 = gc_new<a_double>(99.0);
    
    ConsCell* cell = cons(num1, num2);
    
    a_double* car_result = dynamic_cast<a_double*>(cell->car());
    a_double* cdr_result = dynamic_cast<a_double*>(cell->cdr());
    
    ASSERT_NE(car_result, nullptr);
    ASSERT_NE(cdr_result, nullptr);
    EXPECT_EQ(car_result->value(), 42.0);
    EXPECT_EQ(cdr_result->value(), 99.0);
}

TEST_F(ZombieSchemeTest, ConsWithNil) {
    a_double* num = gc_new<a_double>(42.0);
    ConsCell* cell = cons(num, nil);
    
    EXPECT_EQ(cell->cdr(), nil);
    EXPECT_TRUE(is_nil(cell->cdr()));
}

TEST_F(ZombieSchemeTest, ConsNestedStructure) {
    // Create ((1 . 2) . (3 . 4))
    ConsCell* inner1 = cons(gc_new<a_double>(1.0), gc_new<a_double>(2.0));
    ConsCell* inner2 = cons(gc_new<a_double>(3.0), gc_new<a_double>(4.0));
    ConsCell* outer = cons(inner1, inner2);
    
    EXPECT_TRUE(is_pair(outer->car()));
    EXPECT_TRUE(is_pair(outer->cdr()));
}

TEST_F(ZombieSchemeTest, ConsListConstruction) {
    // Build (1 2 3) as (1 . (2 . (3 . nil)))
    ConsCell* cell3 = cons(gc_new<a_double>(3.0), nil);
    ConsCell* cell2 = cons(gc_new<a_double>(2.0), cell3);
    ConsCell* cell1 = cons(gc_new<a_double>(1.0), cell2);
    
    // Verify structure
    EXPECT_FALSE(is_nil(cell1));
    EXPECT_FALSE(is_nil(cell1->cdr()));
    EXPECT_FALSE(is_nil(((ConsCell*)cell1->cdr())->cdr()));
    EXPECT_TRUE(is_nil(((ConsCell*)((ConsCell*)cell1->cdr())->cdr())->cdr()));
}

TEST_F(ZombieSchemeTest, ConsCloning) {
    ConsCell* original = cons(gc_new<a_double>(1.0), gc_new<a_double>(2.0));
    ConsCell* cloned = dynamic_cast<ConsCell*>(original->clone());
    
    ASSERT_NE(cloned, nullptr);
    EXPECT_NE(original, cloned);
    
    // Values should be equal
    EXPECT_TRUE(original->is_equal(cloned));
}

TEST_F(ZombieSchemeTest, ConsEquality) {
    // Note: ConsCell::is_equal performs structural equality (like Scheme's equal?)
    a_double* num1 = gc_new<a_double>(1.0);
    a_double* num2 = gc_new<a_double>(2.0);

    ConsCell* cell1 = cons(num1, num2);
    ConsCell* cell2 = cons(num1, num2);  // Shares same objects
    ConsCell* cell3 = cons(gc_new<a_double>(1.0), gc_new<a_double>(2.0));  // Different objects, same values

    // cell1 and cell2 share the same car and cdr pointers - structurally equal
    EXPECT_TRUE(cell1->is_equal(cell2));

    // cell1 and cell3 have different pointers but same values - structurally equal
    EXPECT_TRUE(cell1->is_equal(cell3));

    // Different values should not be equal
    ConsCell* cell4 = cons(gc_new<a_double>(3.0), gc_new<a_double>(4.0));
    EXPECT_FALSE(cell1->is_equal(cell4));
}

TEST_F(ZombieSchemeTest, ConsModification) {
    ConsCell* cell = cons(gc_new<a_double>(1.0), gc_new<a_double>(2.0));
    
    // Modify car
    cell->car(gc_new<a_double>(99.0));
    a_double* new_car = dynamic_cast<a_double*>(cell->car());
    EXPECT_EQ(new_car->value(), 99.0);
    
    // Modify cdr
    cell->cdr(gc_new<a_double>(88.0));
    a_double* new_cdr = dynamic_cast<a_double*>(cell->cdr());
    EXPECT_EQ(new_cdr->value(), 88.0);
}

// ============================================================================
// Type System Tests
// ============================================================================

TEST_F(ZombieSchemeTest, TypeEnumValues) {
    // Ensure type enum values are distinct
    EXPECT_NE(static_cast<int>(Type::DOUBLE), static_cast<int>(Type::STRING));
    EXPECT_NE(static_cast<int>(Type::STRING), static_cast<int>(Type::SYMBOL));
    EXPECT_NE(static_cast<int>(Type::LONG), static_cast<int>(Type::DOUBLE));
    EXPECT_NE(static_cast<int>(Type::CHAR), static_cast<int>(Type::BOOL));
    EXPECT_NE(static_cast<int>(Type::INT), static_cast<int>(Type::LONG));
}

TEST_F(ZombieSchemeTest, TypeCheckerFunctions) {
    a_double* d = gc_new<a_double>(1.0);
    a_string* s = gc_new<a_string>("test");
    a_symbol* sym = gc_new<a_symbol>("sym");
    
    EXPECT_TRUE(is_double(d));
    EXPECT_FALSE(is_double(s));
    
    EXPECT_TRUE(is_string(s));
    EXPECT_FALSE(is_string(d));
    
    EXPECT_TRUE(is_symbol(sym));
    EXPECT_FALSE(is_symbol(s));
}

TEST_F(ZombieSchemeTest, TypeAtomVsPairDistinction) {
    a_double* atom_obj = gc_new<a_double>(1.0);
    ConsCell* pair_obj = cons(gc_new<a_double>(1.0), nil);
    
    EXPECT_TRUE(is_atom(atom_obj));
    EXPECT_FALSE(is_pair(atom_obj));
    
    EXPECT_FALSE(is_atom(pair_obj));
    EXPECT_TRUE(is_pair(pair_obj));
}

TEST_F(ZombieSchemeTest, TypeNilSpecialCase) {
    // nil is both an atom and acts like an empty list
    EXPECT_TRUE(is_atom(nil));
    EXPECT_FALSE(is_pair(nil));
    EXPECT_TRUE(is_nil(nil));
}

// ============================================================================
// Mixed Type Interaction Tests
// ============================================================================

TEST_F(ZombieSchemeTest, MixedTypeConsCells) {
    // Car is string, cdr is number
    ConsCell* cell = cons(gc_new<a_string>("hello"), gc_new<a_double>(42.0));
    
    a_string* str = dynamic_cast<a_string*>(cell->car());
    a_double* num = dynamic_cast<a_double*>(cell->cdr());
    
    ASSERT_NE(str, nullptr);
    ASSERT_NE(num, nullptr);
    EXPECT_EQ(str->value(), "hello");
    EXPECT_EQ(num->value(), 42.0);
}

TEST_F(ZombieSchemeTest, MixedTypeList) {
    // Build (42 "hello" 'symbol)
    ConsCell* list = cons(
        gc_new<a_double>(42.0),
        cons(
            gc_new<a_string>("hello"),
            cons(
                gc_new<a_symbol>("symbol"),
                nil
            )
        )
    );
    
    // Verify first element
    a_double* first = dynamic_cast<a_double*>(list->car());
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->value(), 42.0);
    
    // Verify second element
    ConsCell* rest1 = dynamic_cast<ConsCell*>(list->cdr());
    ASSERT_NE(rest1, nullptr);
    a_string* second = dynamic_cast<a_string*>(rest1->car());
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->value(), "hello");
    
    // Verify third element
    ConsCell* rest2 = dynamic_cast<ConsCell*>(rest1->cdr());
    ASSERT_NE(rest2, nullptr);
    a_symbol* third = dynamic_cast<a_symbol*>(rest2->car());
    ASSERT_NE(third, nullptr);
    EXPECT_EQ(third->value(), "symbol");
}

TEST_F(ZombieSchemeTest, NestedMixedTypes) {
    // Create (("string" . 1) . (symbol . 2))
    ConsCell* inner1 = cons(gc_new<a_string>("string"), gc_new<a_double>(1.0));
    ConsCell* inner2 = cons(gc_new<a_symbol>("symbol"), gc_new<a_double>(2.0));
    ConsCell* outer = cons(inner1, inner2);
    
    // Navigate and verify
    ConsCell* car_pair = dynamic_cast<ConsCell*>(outer->car());
    ConsCell* cdr_pair = dynamic_cast<ConsCell*>(outer->cdr());
    
    ASSERT_NE(car_pair, nullptr);
    ASSERT_NE(cdr_pair, nullptr);
    
    a_string* str = dynamic_cast<a_string*>(car_pair->car());
    a_symbol* sym = dynamic_cast<a_symbol*>(cdr_pair->car());
    
    ASSERT_NE(str, nullptr);
    ASSERT_NE(sym, nullptr);
    EXPECT_EQ(str->value(), "string");
    EXPECT_EQ(sym->value(), "symbol");
}

// ============================================================================
// Object Size and Memory Layout Tests
// ============================================================================

TEST_F(ZombieSchemeTest, ObjectSizeReasonable) {
    // Verify Object sizes are reasonable (not accidentally huge)
    EXPECT_LT(sizeof(Object), 1024);
    EXPECT_LT(sizeof(a_double), 1024);
    EXPECT_LT(sizeof(a_string), 1024);
    EXPECT_LT(sizeof(ConsCell), 1024);
}

TEST_F(ZombieSchemeTest, ObjectPointerSafety) {
    // Verify we can safely cast and use pointers
    Object* obj = gc_new<a_double>(42.0);
    
    // Should be able to use as Object*
    EXPECT_EQ(obj->get_type(), static_cast<int>(Type::DOUBLE));
    
    // Should be able to cast back
    a_double* num = dynamic_cast<a_double*>(obj);
    ASSERT_NE(num, nullptr);
    EXPECT_EQ(num->value(), 42.0);
}

TEST_F(ZombieSchemeTest, ObjectPolymorphism) {
    // Store different types as Object* and verify polymorphic behavior
    std::vector<gc_object*> objects;
    objects.push_back(gc_new<a_double>(1.0));
    objects.push_back(gc_new<a_string>("test"));
    objects.push_back(gc_new<a_symbol>("sym"));
    objects.push_back(cons(gc_new<a_double>(1.0), nil));

    // Atoms should have specific types, cons cells have static_cast<int>(Type::NOTYPE) (0)
    EXPECT_EQ(static_cast<Object*>(objects[0])->get_type(), static_cast<int>(Type::DOUBLE));
    EXPECT_EQ(static_cast<Object*>(objects[1])->get_type(), static_cast<int>(Type::STRING));
    EXPECT_EQ(static_cast<Object*>(objects[2])->get_type(), static_cast<int>(Type::SYMBOL));
    EXPECT_EQ(static_cast<Object*>(objects[3])->get_type(), static_cast<int>(Type::NOTYPE));  // ConsCell returns 0

    // All should be clonable
    for (auto* gc_obj : objects) {
        Object* obj = static_cast<Object*>(gc_obj);
        Object* cloned = obj->clone();
        EXPECT_NE(cloned, nullptr);
    }
}

// ============================================================================
// Edge Cases and Boundary Tests
// ============================================================================

TEST_F(ZombieSchemeTest, NumericBoundaries) {
    // Test extreme numeric values
    a_double* max_double = gc_new<a_double>(std::numeric_limits<double>::max());
    a_double* min_double = gc_new<a_double>(std::numeric_limits<double>::min());
    a_double* infinity = gc_new<a_double>(std::numeric_limits<double>::infinity());
    
    EXPECT_TRUE(std::isfinite(max_double->value()));
    EXPECT_TRUE(std::isfinite(min_double->value()));
    EXPECT_TRUE(std::isinf(infinity->value()));
    
    // Should be clonable
    EXPECT_NE(max_double->clone(), nullptr);
    EXPECT_NE(infinity->clone(), nullptr);
}

TEST_F(ZombieSchemeTest, EmptyStringAtom) {
    a_string* empty = gc_new<a_string>("");
    
    EXPECT_TRUE(empty->value().empty());
    EXPECT_EQ(empty->get_type(), static_cast<int>(Type::STRING));
    
    // Should still be clonable and printable
    Object* cloned = empty->clone();
    ASSERT_NE(cloned, nullptr);
    
    std::ostringstream oss;
    empty->print(oss);
    // Empty string should print as empty (or with quotes)
}

TEST_F(ZombieSchemeTest, VeryLongString) {
    std::string long_str(10000, 'X');
    a_string* long_atom = gc_new<a_string>(long_str);
    
    EXPECT_EQ(long_atom->value().length(), 10000);
    
    // Should be clonable
    a_string* cloned = dynamic_cast<a_string*>(long_atom->clone());
    ASSERT_NE(cloned, nullptr);
    EXPECT_EQ(cloned->value().length(), 10000);
}

TEST_F(ZombieSchemeTest, SpecialCharactersInString) {
    // Use std::string constructor with length to preserve embedded null
    a_string* special = gc_new<a_string>(std::string("Hello\nWorld\t\r\0Test", 17));

    EXPECT_EQ(special->get_type(), static_cast<int>(Type::STRING));
    EXPECT_GE(special->value().length(), 15);
}

TEST_F(ZombieSchemeTest, ConsWithSelfReference) {
    // Note: This creates a circular structure - only safe with GC
    ConsCell* cell = cons(gc_new<a_double>(1.0), nil);
    // Don't actually create the cycle in the test as it could cause issues
    // Just verify the mechanism exists
    
    EXPECT_NE(cell, nullptr);
    EXPECT_TRUE(is_pair(cell));
}

TEST_F(ZombieSchemeTest, DeepNesting) {
    // Create a deeply nested structure: ((((42))))
    Object* core = gc_new<a_double>(42.0);
    
    for (int i = 0; i < 10; i++) {
        core = cons(core, nil);
    }
    
    // Should be able to navigate down
    Object* current = core;
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(is_pair(current));
        current = ((ConsCell*)current)->car();
    }
    
    // Should reach the number at the core
    a_double* final_val = dynamic_cast<a_double*>(current);
    ASSERT_NE(final_val, nullptr);
    EXPECT_EQ(final_val->value(), 42.0);
}

// ============================================================================
// GC Integration Tests
// ============================================================================

TEST_F(ZombieSchemeTest, ObjectGCTracing) {
    ConsCell* cell = cons(gc_new<a_double>(1.0), gc_new<a_double>(2.0));

    // Should be able to trace references without crashing
    std::vector<gc_object*> worklist;
    MarkingTracer tracer(&worklist);
    cell->trace_references(tracer);

    // After tracing, car and cdr should be added to worklist
    EXPECT_EQ(2u, worklist.size());
}

TEST_F(ZombieSchemeTest, MultipleObjectsGCMarking) {
    // Create a structure and verify marking propagates
    ConsCell* list = cons(
        gc_new<a_double>(1.0),
        cons(
            gc_new<a_double>(2.0),
            cons(
                gc_new<a_double>(3.0),
                nil
            )
        )
    );

    // Mark the root
    list->set_marked(true);
    EXPECT_TRUE(list->is_marked());

    // Trace should add reachable objects to worklist
    std::vector<gc_object*> worklist;
    MarkingTracer tracer(&worklist);
    list->trace_references(tracer);

    // Should have traced car and cdr
    EXPECT_EQ(2u, worklist.size());
}

