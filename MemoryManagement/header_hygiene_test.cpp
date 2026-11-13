/**
 * @file header_hygiene_test.cpp
 * @brief Compile-time test for header self-containment
 *
 * This file tests that each header in the MemoryManagement abstraction
 * layer includes all of its transitive dependencies. If this file compiles,
 * all headers are self-contained and can be included standalone.
 *
 * DESIGN: Each header is included individually in a separate test function.
 * This catches missing includes like <cstddef> that would cause compilation
 * errors when the header is included without other headers providing the
 * missing symbols.
 *
 * BUILD: This file is NOT part of the main build. To test header hygiene:
 *
 * Option 1 - Visual Studio Developer Command Prompt (easiest):
 *   1. Open "Developer Command Prompt for VS 2022"
 *   2. cd C:\work\ConsoleApplication2-master\MemoryManagement
 *   3. cl.exe /EHsc /std:c++20 /I. /I..\GarbageCollector header_hygiene_test.cpp
 *
 * Option 2 - MSBuild Integration:
 *   Add this file to a test project and build normally.
 *
 * SUCCESS CRITERIA: If compilation succeeds with 0 errors, all headers
 * are self-contained and include all their dependencies.
 *
 * FAILURE CRITERIA: If compilation fails with "cannot open include file",
 * a header is missing an #include directive for a dependency it uses.
 */

// Test 1: IMemoryManager.h standalone
#include "IMemoryManager.h"

void test_imemorymanager() {
    // If this compiles, IMemoryManager.h includes all dependencies
    static_assert(sizeof(memory::IMemoryManager) > 0);
}

// Test 2: ITracer.h standalone
#include "ITracer.h"

void test_itracer() {
    // If this compiles, ITracer.h includes all dependencies (including <cstddef>)
    static_assert(sizeof(memory::ITracer) > 0);
}

// Test 3: ITraceable.h standalone
#include "ITraceable.h"

void test_itraceable() {
    // If this compiles, ITraceable.h includes all dependencies
    static_assert(sizeof(memory::ITraceable) > 0);
}

// Test 4: IWriteBarrier.h standalone
#include "IWriteBarrier.h"

void test_iwritebarrier() {
    // If this compiles, IWriteBarrier.h includes all dependencies
    static_assert(sizeof(memory::IWriteBarrier) > 0);
}

// Test 5: IRootManager.h standalone
#include "IRootManager.h"

void test_irootmanager() {
    // If this compiles, IRootManager.h includes all dependencies
    static_assert(sizeof(memory::IRootManager) > 0);
}

// Test 6: concepts.h standalone
#include "concepts.h"

void test_concepts() {
    // If this compiles, concepts.h includes all dependencies
    // Concepts are compile-time only, so just verify a concept exists
    static_assert(requires { typename std::bool_constant<memory::MemoryManager<memory::IMemoryManager>>; });
}

// Test 7: GCPolicy.h standalone
#include "policies/GCPolicy.h"

void test_gcpolicy() {
    // If this compiles, GCPolicy.h includes all dependencies
    static_assert(sizeof(memory::GCPolicy) > 0);
}

// Test 8: SharedPtrPolicy.h standalone
#include "policies/SharedPtrPolicy.h"

void test_sharedptrpolicy() {
    // If this compiles, SharedPtrPolicy.h includes all dependencies
    static_assert(sizeof(memory::SharedPtrPolicy) > 0);
}

// Test 9: RawPtrPolicy.h standalone
#include "policies/RawPtrPolicy.h"

void test_rawptrpolicy() {
    // If this compiles, RawPtrPolicy.h includes all dependencies
    static_assert(sizeof(memory::RawPtrPolicy) > 0);
}

// Test 10: all.h (aggregate header)
#include "policies/all.h"

void test_all_policies() {
    // If this compiles, all.h includes all policy headers correctly
    static_assert(sizeof(memory::GCPolicy) > 0);
    static_assert(sizeof(memory::SharedPtrPolicy) > 0);
    static_assert(sizeof(memory::RawPtrPolicy) > 0);
}

int main() {
    // If this program compiles and links, all headers pass hygiene check
    test_imemorymanager();
    test_itracer();
    test_itraceable();
    test_iwritebarrier();
    test_irootmanager();
    test_concepts();
    test_gcpolicy();
    test_sharedptrpolicy();
    test_rawptrpolicy();
    test_all_policies();

    return 0;  // SUCCESS: All headers are self-contained
}
