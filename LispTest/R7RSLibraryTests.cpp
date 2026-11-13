/**
 * @file R7RSLibraryTests.cpp
 * @brief Tests for R7RS library (define-library) support
 *
 * Tests the R7RS module system including:
 * - Library definition (define-library)
 * - Import declarations (import, only, except, prefix, rename)
 * - Export declarations (export)
 * - Standard libraries ((scheme base), (scheme read), etc.)
 */

#include "pch.h"
#include "TestFixture.h"
#include "library.h"
#include "library_registry.h"
#include "library_parser.h"
#include "scheme_reader.h"
#include "numeric_runtime.h"
#include "Lisp.h"
#include "r7rs_stdlib.h"
#include "gc_ptr.h"
#include <algorithm>

// Absolute path to the test-libs fixtures, normally injected by CMake so the
// file-loading tests are independent of the process working directory. Fallback
// to a repo-root-relative path if built outside the CMake target.
#ifndef ZOMBIE_TEST_LIBS_DIR
#define ZOMBIE_TEST_LIBS_DIR "test-libs"
#endif

// ============================================================================
// Phase 1: Basic Library Infrastructure Tests
// ============================================================================

class R7RSLibraryTest : public ZombieSchemeTest {
protected:
    void SetUp() override {
        // IMPORTANT: Clear library registry BEFORE setting up test
        // This ensures we start with a clean slate even if previous tests
        // from other suites left libraries registered
        r7rs::LibraryRegistry::instance().clear();

        ZombieSchemeTest::SetUp();
        // Additional setup for library system
    }

    void TearDown() override {
        // Clean up library registry AFTER test
        r7rs::LibraryRegistry::instance().clear();
        ZombieSchemeTest::TearDown();
    }
};

// ============================================================================
// Test 1: Empty Library Creation
// ============================================================================

TEST_F(R7RSLibraryTest, CreateEmptyLibrary) {
    // Goal: Create an empty library with no imports or exports
    // This is the simplest possible library

    using namespace r7rs;

    // Create library programmatically (parser support comes later)
    auto lib = make_gc_ptr<Library>(LibraryName{"test", "empty"});

    // Verify library properties
    EXPECT_EQ(lib->name().size(), 2);
    EXPECT_EQ(lib->name()[0], "test");
    EXPECT_EQ(lib->name()[1], "empty");
    EXPECT_EQ(lib->name_string(), "(test empty)");
    EXPECT_EQ(lib->import_count(), 0);
    EXPECT_EQ(lib->export_count(), 0);
    EXPECT_FALSE(lib->is_initialized());

    // Register library
    auto& registry = LibraryRegistry::instance();
    registry.clear();  // Start fresh
    registry.register_library(std::move(lib));

    // Verify registration
    EXPECT_EQ(registry.library_count(), 1);
    EXPECT_TRUE(registry.has_library(LibraryName{"test", "empty"}));

    Library* retrieved = registry.get_library(LibraryName{"test", "empty"});
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->name_string(), "(test empty)");
}

// ============================================================================
// Test 2: Parse Simple Library Definition
// ============================================================================

TEST_F(R7RSLibraryTest, ParseSimpleLibraryDefinition) {
    // Goal: Parse a define-library form from text

    using namespace r7rs;

    const char* code = R"(
        (define-library (test simple)
          (import (scheme base))
          (export square)
          (begin
            (define (square x) (* x x))))
    )";

    // Parse to cons list
    scheme_reader reader;
    Object* expr = reader.read_from_string(code);
    ASSERT_NE(expr, nullptr);

    // Check if it's a library definition
    LibraryParser parser;
    EXPECT_TRUE(parser.is_library_definition(expr));

    // Parse into Library Object
    auto lib = parser.parse_library_definition(expr);
    ASSERT_NE(lib, nullptr);

    // Verify library properties
    EXPECT_EQ(lib->name().size(), 2);
    EXPECT_EQ(lib->name()[0], "test");
    EXPECT_EQ(lib->name()[1], "simple");
    EXPECT_EQ(lib->name_string(), "(test simple)");

    // Check imports
    EXPECT_EQ(lib->import_count(), 1);
    const auto& imports = lib->imports();
    EXPECT_EQ(imports[0].type, ImportSpec::SIMPLE);
    EXPECT_EQ(imports[0].library.size(), 2);
    EXPECT_EQ(imports[0].library[0], "scheme");
    EXPECT_EQ(imports[0].library[1], "base");

    // Check exports
    EXPECT_EQ(lib->export_count(), 1);
    const auto& exports = lib->exports();
    EXPECT_EQ(exports[0].external_name, "square");
    EXPECT_TRUE(lib->exports_symbol("square"));
}

// ============================================================================
// Test 3: Parse Library with Multiple Imports
// ============================================================================

TEST_F(R7RSLibraryTest, ParseLibraryWithMultipleImports) {
    using namespace r7rs;

    const char* code = R"(
        (define-library (test multi-import)
          (import (scheme base))
          (import (scheme read))
          (import (scheme write))
          (export process-data))
    )";

    scheme_reader reader;
    Object* expr = reader.read_from_string(code);

    LibraryParser parser;
    auto lib = parser.parse_library_definition(expr);

    EXPECT_EQ(lib->import_count(), 3);
    EXPECT_EQ(lib->export_count(), 1);
}

// ============================================================================
// Test 4: Parse Import Only
// ============================================================================

TEST_F(R7RSLibraryTest, ParseImportOnly) {
    using namespace r7rs;

    const char* code = R"(
        (define-library (test only-import)
          (import (only (scheme base) + - * /))
          (export calc))
    )";

    scheme_reader reader;
    Object* expr = reader.read_from_string(code);

    LibraryParser parser;
    auto lib = parser.parse_library_definition(expr);

    ASSERT_EQ(lib->import_count(), 1);
    const auto& import = lib->imports()[0];
    EXPECT_EQ(import.type, ImportSpec::ONLY);
    EXPECT_EQ(import.symbols.size(), 4);
    EXPECT_TRUE(import.symbols.count("+") > 0);
    EXPECT_TRUE(import.symbols.count("-") > 0);
    EXPECT_TRUE(import.symbols.count("*") > 0);
    EXPECT_TRUE(import.symbols.count("/") > 0);
}

// ============================================================================
// Test 5: Parse Import Prefix
// ============================================================================

TEST_F(R7RSLibraryTest, ParseImportPrefix) {
    using namespace r7rs;

    const char* code = R"(
        (define-library (test prefix-import)
          (import (prefix (scheme base) Base:))
          (export foo))
    )";

    scheme_reader reader;
    Object* expr = reader.read_from_string(code);

    LibraryParser parser;
    auto lib = parser.parse_library_definition(expr);

    ASSERT_EQ(lib->import_count(), 1);
    const auto& import = lib->imports()[0];
    EXPECT_EQ(import.type, ImportSpec::PREFIX);
    EXPECT_EQ(import.prefix, "Base:");
}

// ============================================================================
// Test 6: Parse Import Rename
// ============================================================================

TEST_F(R7RSLibraryTest, ParseImportRename) {
    using namespace r7rs;

    const char* code = R"(
        (define-library (test rename-import)
          (import (rename (scheme base) (+ plus) (- minus)))
          (export compute))
    )";

    scheme_reader reader;
    Object* expr = reader.read_from_string(code);

    LibraryParser parser;
    auto lib = parser.parse_library_definition(expr);

    ASSERT_EQ(lib->import_count(), 1);
    const auto& import = lib->imports()[0];
    EXPECT_EQ(import.type, ImportSpec::RENAME);
    EXPECT_EQ(import.renames.size(), 2);
    EXPECT_EQ(import.renames.at("+"), "plus");
    EXPECT_EQ(import.renames.at("-"), "minus");
}

// ============================================================================
// Test 7: Parse Export Rename
// ============================================================================

TEST_F(R7RSLibraryTest, ParseExportRename) {
    using namespace r7rs;

    const char* code = R"(
        (define-library (test export-rename)
          (import (scheme base))
          (export (rename internal-func external-func)))
    )";

    scheme_reader reader;
    Object* expr = reader.read_from_string(code);

    LibraryParser parser;
    auto lib = parser.parse_library_definition(expr);

    ASSERT_EQ(lib->export_count(), 1);
    const auto& exp = lib->exports()[0];
    EXPECT_EQ(exp.internal_name, "internal-func");
    EXPECT_EQ(exp.external_name, "external-func");
}

// ============================================================================
// Test 8: Parse and Store Begin Blocks
// ============================================================================

TEST_F(R7RSLibraryTest, ParseAndStoreBeginBlocks) {
    using namespace r7rs;

    const char* code = R"(
        (define-library (test begin-blocks)
          (import (scheme base))
          (export square cube)
          (begin
            (define (square x) (* x x))
            (define (cube x) (* x x x))))
    )";

    scheme_reader reader;
    Object* expr = reader.read_from_string(code);

    LibraryParser parser;
    auto lib = parser.parse_library_definition(expr);

    // Check that begin block was stored
    EXPECT_EQ(lib->begin_blocks().size(), 1);
    EXPECT_NE(lib->begin_blocks()[0], nullptr);
}

// ============================================================================
// Test 9: Initialize Empty Library
// ============================================================================

TEST_F(R7RSLibraryTest, InitializeEmptyLibrary) {
    using namespace r7rs;

    // Create empty library
    auto lib = make_gc_ptr<Library>(LibraryName{"test", "empty"});

    // Initialize with null parent (no primitives)
    std::string error;
    bool success = lib->initialize(nullptr, error);

    EXPECT_TRUE(success);
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(lib->is_initialized());
    EXPECT_NE(lib->environment(), nullptr);
}

// ============================================================================
// Test 10: Initialize with Simple Import
// ============================================================================

TEST_F(R7RSLibraryTest, InitializeWithSimpleImport) {
    using namespace r7rs;

    // Step 1: Create and register a Base library with exports
    auto base_lib = make_gc_ptr<Library>(LibraryName{"base", "lib"});
    base_lib->add_export("plus");
    base_lib->add_export("minus");
    base_lib->create_environment();

    // Define some symbols in Base library
    Object* plus_val = numeric::make_integer(100);  // Placeholder values
    Object* minus_val = numeric::make_integer(200);
    base_lib->define_symbol("plus", plus_val);
    base_lib->define_symbol("minus", minus_val);
    base_lib->set_initialized(true);

    LibraryRegistry& registry = LibraryRegistry::instance();
    registry.register_library(std::move(base_lib));

    // Step 2: Create importing library
    auto lib = make_gc_ptr<Library>(LibraryName{"test", "import"});
    lib->add_import(ImportSpec::make_simple({"base", "lib"}));

    // Step 3: Initialize
    std::string error;
    bool success = lib->initialize(nullptr, error);

    EXPECT_TRUE(success) << "Error: " << error;
    EXPECT_TRUE(lib->is_initialized());

    // Step 4: Check that imported symbols are available
    EXPECT_TRUE(lib->has_symbol("plus"));
    EXPECT_TRUE(lib->has_symbol("minus"));
    EXPECT_EQ(lib->get_symbol("plus"), plus_val);
    EXPECT_EQ(lib->get_symbol("minus"), minus_val);

    // Cleanup
    registry.clear();
}

// ============================================================================
// Test 11: Initialize with Import ONLY
// ============================================================================

TEST_F(R7RSLibraryTest, InitializeWithImportOnly) {
    using namespace r7rs;

    // Step 1: Create Base library
    auto base_lib = make_gc_ptr<Library>(LibraryName{"base", "lib"});
    base_lib->add_export("a");
    base_lib->add_export("b");
    base_lib->add_export("c");
    base_lib->create_environment();
    base_lib->define_symbol("a", numeric::make_integer(1));
    base_lib->define_symbol("b", numeric::make_integer(2));
    base_lib->define_symbol("c", numeric::make_integer(3));
    base_lib->set_initialized(true);

    LibraryRegistry& registry = LibraryRegistry::instance();
    registry.register_library(std::move(base_lib));

    // Step 2: Create library with ONLY import
    auto lib = make_gc_ptr<Library>(LibraryName{"test", "only"});
    lib->add_import(ImportSpec::make_only({"base", "lib"}, {"a", "c"}));

    // Step 3: Initialize
    std::string error;
    bool success = lib->initialize(nullptr, error);

    EXPECT_TRUE(success) << "Error: " << error;

    // Step 4: Check that only selected symbols are imported
    EXPECT_TRUE(lib->has_symbol("a"));
    EXPECT_FALSE(lib->has_symbol("b"));  // Not in ONLY list
    EXPECT_TRUE(lib->has_symbol("c"));

    // Cleanup
    registry.clear();
}

// ============================================================================
// Test 12: Initialize with Import EXCEPT
// ============================================================================

TEST_F(R7RSLibraryTest, InitializeWithImportExcept) {
    using namespace r7rs;

    // Step 1: Create Base library
    auto base_lib = make_gc_ptr<Library>(LibraryName{"base", "lib"});
    base_lib->add_export("x");
    base_lib->add_export("y");
    base_lib->add_export("z");
    base_lib->create_environment();
    base_lib->define_symbol("x", numeric::make_integer(10));
    base_lib->define_symbol("y", numeric::make_integer(20));
    base_lib->define_symbol("z", numeric::make_integer(30));
    base_lib->set_initialized(true);

    LibraryRegistry& registry = LibraryRegistry::instance();
    registry.register_library(std::move(base_lib));

    // Step 2: Create library with EXCEPT import
    auto lib = make_gc_ptr<Library>(LibraryName{"test", "except"});
    lib->add_import(ImportSpec::make_except({"base", "lib"}, {"y"}));

    // Step 3: Initialize
    std::string error;
    bool success = lib->initialize(nullptr, error);

    EXPECT_TRUE(success) << "Error: " << error;

    // Step 4: Check that excluded symbol is not imported
    EXPECT_TRUE(lib->has_symbol("x"));
    EXPECT_FALSE(lib->has_symbol("y"));  // In EXCEPT list
    EXPECT_TRUE(lib->has_symbol("z"));

    // Cleanup
    registry.clear();
}

// ============================================================================
// Test 13: Initialize with Import PREFIX
// ============================================================================

TEST_F(R7RSLibraryTest, InitializeWithImportPrefix) {
    using namespace r7rs;

    // Step 1: Create Base library
    auto base_lib = make_gc_ptr<Library>(LibraryName{"base", "lib"});
    base_lib->add_export("func");
    base_lib->create_environment();
    base_lib->define_symbol("func", numeric::make_integer(42));
    base_lib->set_initialized(true);

    LibraryRegistry& registry = LibraryRegistry::instance();
    registry.register_library(std::move(base_lib));

    // Step 2: Create library with PREFIX import
    auto lib = make_gc_ptr<Library>(LibraryName{"test", "prefix"});
    lib->add_import(ImportSpec::make_prefix({"base", "lib"}, "Base:"));

    // Step 3: Initialize
    std::string error;
    bool success = lib->initialize(nullptr, error);

    EXPECT_TRUE(success) << "Error: " << error;

    // Step 4: Check that symbol has prefix
    EXPECT_FALSE(lib->has_symbol("func"));      // Original name not imported
    EXPECT_TRUE(lib->has_symbol("Base:func"));  // Prefixed name imported

    // Cleanup
    registry.clear();
}

// ============================================================================
// Test 14: Initialize with Import RENAME
// ============================================================================

TEST_F(R7RSLibraryTest, InitializeWithImportRename) {
    using namespace r7rs;

    // Step 1: Create Base library
    auto base_lib = make_gc_ptr<Library>(LibraryName{"base", "lib"});
    base_lib->add_export("old-name");
    base_lib->create_environment();
    base_lib->define_symbol("old-name", numeric::make_integer(99));
    base_lib->set_initialized(true);

    LibraryRegistry& registry = LibraryRegistry::instance();
    registry.register_library(std::move(base_lib));

    // Step 2: Create library with RENAME import
    auto lib = make_gc_ptr<Library>(LibraryName{"test", "rename"});
    std::map<std::string, std::string> renames = {{"old-name", "new-name"}};
    lib->add_import(ImportSpec::make_rename({"base", "lib"}, renames));

    // Step 3: Initialize
    std::string error;
    bool success = lib->initialize(nullptr, error);

    EXPECT_TRUE(success) << "Error: " << error;

    // Step 4: Check that symbol was renamed
    EXPECT_FALSE(lib->has_symbol("old-name"));  // Old name not imported
    EXPECT_TRUE(lib->has_symbol("new-name"));   // New name imported

    // Cleanup
    registry.clear();
}

// ============================================================================
// Test 15: Recursive Dependency Initialization
// ============================================================================

TEST_F(R7RSLibraryTest, RecursiveDependencyInitialization) {
    using namespace r7rs;

    LibraryRegistry& registry = LibraryRegistry::instance();

    // Step 1: Create Base library (leaf dependency)
    auto Base = make_gc_ptr<Library>(LibraryName{"level", "0"});
    Base->add_export("value");
    Base->create_environment();
    Base->define_symbol("value", numeric::make_integer(1));
    Base->set_initialized(true);
    registry.register_library(std::move(Base));

    // Step 2: Create middle library (depends on Base)
    auto middle = make_gc_ptr<Library>(LibraryName{"level", "1"});
    middle->add_import(ImportSpec::make_simple({"level", "0"}));
    middle->add_export("computed");
    middle->create_environment();
    middle->define_symbol("computed", numeric::make_integer(2));
    registry.register_library(std::move(middle));

    // Step 3: Create top library (depends on middle, which depends on Base)
    auto top = make_gc_ptr<Library>(LibraryName{"level", "2"});
    top->add_import(ImportSpec::make_simple({"level", "1"}));

    // Step 4: Initialize top library (should recursively initialize middle)
    std::string error;
    bool success = top->initialize(nullptr, error);

    EXPECT_TRUE(success) << "Error: " << error;

    // Step 5: Check that symbols from both dependencies are available
    EXPECT_TRUE(top->has_symbol("computed"));  // From middle

    // Cleanup
    registry.clear();
}

// ============================================================================
// Test 17: Evaluate Begin Blocks with Simple Define
// ============================================================================

TEST_F(R7RSLibraryTest, EvaluateBeginBlocksSimpleDefine) {
    using namespace r7rs;

    // Parse library with begin block
    const char* code = R"(
        (define-library (test eval)
          (export my-value)
          (begin
            (define my-value 42)))
    )";

    scheme_reader reader;
    Object* expr = reader.read_from_string(code);

    LibraryParser parser;
    auto lib = parser.parse_library_definition(expr);

    // Initialize library
    std::string error;
    ASSERT_TRUE(lib->initialize(nullptr, error)) << error;

    // Check that my-value is NOT defined yet (begin blocks not evaluated)
    EXPECT_FALSE(lib->has_symbol("my-value"));

    // Evaluate begin blocks using Lisp evaluator
    auto eval_func = [this](Object* form, Environment* env_arg) {
        return this->interpreter->new_eval(form, env_arg);
    };

    ASSERT_TRUE(lib->evaluate_begin_blocks(eval_func, error)) << error;

    // Now my-value should be defined
    EXPECT_TRUE(lib->has_symbol("my-value"));
    Object* value = lib->get_symbol("my-value");
    ASSERT_NE(value, nullptr);
    EXPECT_TRUE(numeric::is_integer_value(value));
    EXPECT_EQ(numeric::to_int64(value), 42);
}

// ============================================================================
// Test 18: Evaluate Begin Blocks with Multiple Definitions
// ============================================================================

TEST_F(R7RSLibraryTest, EvaluateBeginBlocksMultipleDefines) {
    using namespace r7rs;

    const char* code = R"(
        (define-library (test multi)
          (export a b c)
          (begin
            (define a 1)
            (define b 2)
            (define c 3)))
    )";

    scheme_reader reader;
    Object* expr = reader.read_from_string(code);

    LibraryParser parser;
    auto lib = parser.parse_library_definition(expr);

    std::string error;
    ASSERT_TRUE(lib->initialize(nullptr, error)) << error;

    // Evaluate begin blocks
    auto eval_func = [this](Object* form, Environment* env_arg) {
        return this->interpreter->new_eval(form, env_arg);
    };

    ASSERT_TRUE(lib->evaluate_begin_blocks(eval_func, error)) << error;

    // Check all symbols are defined
    EXPECT_TRUE(lib->has_symbol("a"));
    EXPECT_TRUE(lib->has_symbol("b"));
    EXPECT_TRUE(lib->has_symbol("c"));

    EXPECT_EQ(numeric::to_int64(lib->get_symbol("a")), 1);
    EXPECT_EQ(numeric::to_int64(lib->get_symbol("b")), 2);
    EXPECT_EQ(numeric::to_int64(lib->get_symbol("c")), 3);
}

// ============================================================================
// Test 19: Evaluate Begin Blocks with Function Definition
// ============================================================================

TEST_F(R7RSLibraryTest, EvaluateBeginBlocksWithFunction) {
    using namespace r7rs;

    const char* code = R"(
        (define-library (test func)
          (export square)
          (begin
            (define (square x) (* x x))))
    )";

    scheme_reader reader;
    Object* expr = reader.read_from_string(code);

    LibraryParser parser;
    auto lib = parser.parse_library_definition(expr);

    std::string error;
    ASSERT_TRUE(lib->initialize(env, error)) << error;  // Use test fixture's env for primitives

    // Evaluate begin blocks
    auto eval_func = [this](Object* form, Environment* env_arg) {
        return this->interpreter->new_eval(form, env_arg);
    };

    ASSERT_TRUE(lib->evaluate_begin_blocks(eval_func, error)) << error;

    // Check that square is defined
    EXPECT_TRUE(lib->has_symbol("square"));
    Object* square_proc = lib->get_symbol("square");
    ASSERT_NE(square_proc, nullptr);
    EXPECT_NE(square_proc, nil);

    // Square should be a Procedure (not an atom)
    EXPECT_FALSE(square_proc->is_atom());
}

// ============================================================================
// Test 20: Bootstrap (scheme base) Standard Library
// ============================================================================

TEST_F(R7RSLibraryTest, BootstrapSchemeBase) {
    using namespace r7rs;

    // Bootstrap the (scheme base) standard library
    bootstrap_scheme_base(env);

    // Verify the library is registered
    Library* lib = LibraryRegistry::instance().get_library({"scheme", "base"});
    ASSERT_NE(lib, nullptr);

    // Check library name
    EXPECT_EQ(lib->name_string(), "(scheme base)");

    // Check that it's marked as initialized
    EXPECT_TRUE(lib->is_initialized());

    // Verify it exports many symbols (should be 150+)
    EXPECT_GT(lib->export_count(), 100);

    // Check a few key exports
    EXPECT_TRUE(lib->exports_symbol("+"));
    EXPECT_TRUE(lib->exports_symbol("-"));
    EXPECT_TRUE(lib->exports_symbol("*"));
    EXPECT_TRUE(lib->exports_symbol("/"));
    EXPECT_TRUE(lib->exports_symbol("cons"));
    EXPECT_TRUE(lib->exports_symbol("car"));
    EXPECT_TRUE(lib->exports_symbol("cdr"));
    EXPECT_TRUE(lib->exports_symbol("list"));
    EXPECT_TRUE(lib->exports_symbol("lambda"));
    EXPECT_TRUE(lib->exports_symbol("if"));
    EXPECT_TRUE(lib->exports_symbol("define"));
}

// ============================================================================
// Test 21: Bootstrap All Standard Libraries
// ============================================================================

TEST_F(R7RSLibraryTest, BootstrapAllStandardLibraries) {
    using namespace r7rs;

    // Bootstrap all standard libraries
    bootstrap_standard_libraries(env);

    // Verify (scheme base)
    Library* Base = LibraryRegistry::instance().get_library({"scheme", "base"});
    ASSERT_NE(Base, nullptr);
    EXPECT_TRUE(Base->is_initialized());

    // Verify (scheme read)
    Library* read_lib = LibraryRegistry::instance().get_library({"scheme", "read"});
    ASSERT_NE(read_lib, nullptr);
    EXPECT_TRUE(read_lib->is_initialized());
    EXPECT_TRUE(read_lib->exports_symbol("read"));

    // Verify (scheme write)
    Library* write_lib = LibraryRegistry::instance().get_library({"scheme", "write"});
    ASSERT_NE(write_lib, nullptr);
    EXPECT_TRUE(write_lib->is_initialized());
    EXPECT_TRUE(write_lib->exports_symbol("write"));
    EXPECT_TRUE(write_lib->exports_symbol("display"));

    // Verify (scheme file)
    Library* file_lib = LibraryRegistry::instance().get_library({"scheme", "file"});
    ASSERT_NE(file_lib, nullptr);
    EXPECT_TRUE(file_lib->is_initialized());
    EXPECT_TRUE(file_lib->exports_symbol("open-input-file"));
    EXPECT_TRUE(file_lib->exports_symbol("open-output-file"));
}

// ============================================================================
// Test 22: (scheme base) Has Access to Primitives
// ============================================================================

TEST_F(R7RSLibraryTest, SchemeBaseHasAccessToPrimitives) {
    using namespace r7rs;

    // Bootstrap (scheme base)
    bootstrap_scheme_base(env);

    Library* lib = LibraryRegistry::instance().get_library({"scheme", "base"});
    ASSERT_NE(lib, nullptr);

    // Check that the library has access to Primitive functions
    // The library should be able to look up exported symbols
    EXPECT_TRUE(lib->has_symbol("+"));
    EXPECT_TRUE(lib->has_symbol("cons"));
    EXPECT_TRUE(lib->has_symbol("car"));

    // Get actual Primitive objects
    Object* plus = lib->get_symbol("+");
    ASSERT_NE(plus, nullptr);
    EXPECT_NE(plus, nil);

    Object* cons_proc = lib->get_symbol("cons");
    ASSERT_NE(cons_proc, nullptr);
    EXPECT_NE(cons_proc, nil);
}

// ============================================================================
// Test 23: Import (scheme base) Into User Library
// ============================================================================

TEST_F(R7RSLibraryTest, ImportSchemeBaseIntoUserLibrary) {
    using namespace r7rs;

    // Bootstrap (scheme base) first
    bootstrap_scheme_base(env);

    // Create a user library that imports (scheme base)
    const char* code = R"(
        (define-library (mylib math)
          (import (scheme base))
          (export square)
          (begin
            (define (square x) (* x x))))
    )";

    scheme_reader reader;
    Object* expr = reader.read_from_string(code);

    LibraryParser parser;
    auto lib = parser.parse_library_definition(expr);

    // Initialize the library (should process import)
    std::string error;
    ASSERT_TRUE(lib->initialize(env, error)) << error;

    // Check that import was processed
    EXPECT_TRUE(lib->has_symbol("+"));  // Should have + from (scheme base)
    EXPECT_TRUE(lib->has_symbol("*"));  // Should have * from (scheme base)

    // Evaluate begin blocks to define square
    auto eval_func = [this](Object* form, Environment* env_arg) {
        return this->interpreter->new_eval(form, env_arg);
    };

    ASSERT_TRUE(lib->evaluate_begin_blocks(eval_func, error)) << error;

    // Verify square is defined
    EXPECT_TRUE(lib->has_symbol("square"));
    Object* square_proc = lib->get_symbol("square");
    ASSERT_NE(square_proc, nullptr);
    EXPECT_NE(square_proc, nil);
}

// ============================================================================
// Test 24: Library with Single Export (was DISABLED)
// ============================================================================

TEST_F(R7RSLibraryTest, LibraryWithSingleExport) {
    // Goal: Define a library that exports one function
    const char* code = R"(
        (define-library (test single-export)
          (import (scheme base))
          (export square)
          (begin
            (define (square x) (* x x))))
    )";

    // TODO: Implement library definition
    // Object* result = eval_string(code);
    // EXPECT_NE(result, nullptr);

    // TODO: Test that square is exported
    // EXPECT_TRUE(library_exports("test single-export", "square"));
}

// ============================================================================
// Test 25: Complete End-to-End Library Workflow
// ============================================================================

TEST_F(R7RSLibraryTest, CompleteEndToEndWorkflow) {
    using namespace r7rs;

    // Step 1: Bootstrap standard libraries
    bootstrap_standard_libraries(env);

    // Step 2: Define a user library that imports (scheme base)
    const char* lib_code = R"(
        (define-library (demo math)
          (import (scheme base))
          (export square cube add-one factorial)
          (begin
            (define (square x) (* x x))
            (define (cube x) (* x x x))
            (define (add-one x) (+ x 1))
            (define (factorial n)
              (if (= n 0)
                  1
                  (* n (factorial (- n 1)))))))
    )";

    scheme_reader reader;
    Object* lib_expr = reader.read_from_string(lib_code);

    LibraryParser parser;
    auto lib = parser.parse_library_definition(lib_expr);

    // Step 3: Initialize library (processes imports)
    std::string error;
    ASSERT_TRUE(lib->initialize(env, error)) << error;

    // Verify imports were processed
    EXPECT_TRUE(lib->has_symbol("+"));
    EXPECT_TRUE(lib->has_symbol("*"));
    EXPECT_TRUE(lib->has_symbol("-"));
    EXPECT_TRUE(lib->has_symbol("="));

    // Step 4: Evaluate begin blocks to define functions
    auto eval_func = [this](Object* form, Environment* env_arg) {
        return this->interpreter->new_eval(form, env_arg);
    };

    ASSERT_TRUE(lib->evaluate_begin_blocks(eval_func, error)) << error;

    // Step 5: Verify all exported functions are defined
    EXPECT_TRUE(lib->has_symbol("square"));
    EXPECT_TRUE(lib->has_symbol("cube"));
    EXPECT_TRUE(lib->has_symbol("add-one"));
    EXPECT_TRUE(lib->has_symbol("factorial"));

    // Step 6: Get function objects
    Object* square_proc = lib->get_symbol("square");
    Object* cube_proc = lib->get_symbol("cube");
    Object* add_one_proc = lib->get_symbol("add-one");
    Object* factorial_proc = lib->get_symbol("factorial");

    ASSERT_NE(square_proc, nullptr);
    ASSERT_NE(cube_proc, nullptr);
    ASSERT_NE(add_one_proc, nullptr);
    ASSERT_NE(factorial_proc, nullptr);

    // Step 7: Call functions and verify results
    // Note: We need to temporarily switch interpreter's environment to library's environment
    Environment* saved_env = interpreter->get_environment();
    interpreter->set_environment(lib->environment());

    // Call square(5) - should return 25
    Object* square_code = reader.read_from_string("(square 5)");
    Object* square_result = interpreter->new_eval(square_code, lib->environment());
    ASSERT_TRUE(numeric::is_integer_value(square_result));
    EXPECT_EQ(numeric::to_int64(square_result), 25);

    // Call cube(3) - should return 27
    Object* cube_code = reader.read_from_string("(cube 3)");
    Object* cube_result = interpreter->new_eval(cube_code, lib->environment());
    ASSERT_TRUE(numeric::is_integer_value(cube_result));
    EXPECT_EQ(numeric::to_int64(cube_result), 27);

    // Call add-one(41) - should return 42
    Object* add_one_code = reader.read_from_string("(add-one 41)");
    Object* add_one_result = interpreter->new_eval(add_one_code, lib->environment());
    ASSERT_TRUE(numeric::is_integer_value(add_one_result));
    EXPECT_EQ(numeric::to_int64(add_one_result), 42);

    // Call factorial(5) - should return 120
    Object* factorial_code = reader.read_from_string("(factorial 5)");
    Object* factorial_result = interpreter->new_eval(factorial_code, lib->environment());
    ASSERT_TRUE(numeric::is_integer_value(factorial_result));
    EXPECT_EQ(numeric::to_int64(factorial_result), 120);

    // Restore original environment
    interpreter->set_environment(saved_env);

    // Step 8: Register library globally and verify lookup
    LibraryRegistry::instance().register_library(std::move(lib));

    Library* registered_lib = LibraryRegistry::instance().get_library({"demo", "math"});
    ASSERT_NE(registered_lib, nullptr);
    EXPECT_EQ(registered_lib->name_string(), "(demo math)");
    EXPECT_TRUE(registered_lib->is_initialized());
    EXPECT_EQ(registered_lib->export_count(), 4);
}

// ============================================================================
// Test 26: Multiple Libraries with Cross-Dependencies (Phase 2)
// ============================================================================

TEST_F(R7RSLibraryTest, MultipleLibrariesWithDependencies) {
    using namespace r7rs;

    // Bootstrap standard libraries
    bootstrap_standard_libraries(env);

    // Define library A: basic operations
    const char* lib_a_code = R"(
        (define-library (libs a)
          (import (scheme base))
          (export double triple)
          (begin
            (define (double x) (* x 2))
            (define (triple x) (* x 3))))
    )";

    // Parse and initialize library A
    scheme_reader reader;
    Object* lib_a_expr = reader.read_from_string(lib_a_code);

    LibraryParser parser;
    auto lib_a = parser.parse_library_definition(lib_a_expr);

    std::string error;
    ASSERT_TRUE(lib_a->initialize(env, error)) << error;

    auto eval_func = [this](Object* form, Environment* env_arg) {
        return this->interpreter->new_eval(form, env_arg);
    };

    ASSERT_TRUE(lib_a->evaluate_begin_blocks(eval_func, error)) << error;

    // Verify library A has the functions defined
    EXPECT_TRUE(lib_a->has_symbol("double"));
    EXPECT_TRUE(lib_a->has_symbol("triple"));
    ASSERT_NE(lib_a->get_symbol("double"), nullptr);
    ASSERT_NE(lib_a->get_symbol("triple"), nullptr);

    // Register library A
    LibraryRegistry::instance().register_library(std::move(lib_a));

    // Verify library A is in registry and still has symbols
    Library* registered_lib_a = LibraryRegistry::instance().get_library({"libs", "a"});
    ASSERT_NE(registered_lib_a, nullptr);
    EXPECT_TRUE(registered_lib_a->has_symbol("double")) << "Registered library A should have 'double'";
    EXPECT_TRUE(registered_lib_a->has_symbol("triple")) << "Registered library A should have 'triple'";
    EXPECT_TRUE(registered_lib_a->exports_symbol("double")) << "Library A should export 'double'";
    EXPECT_TRUE(registered_lib_a->exports_symbol("triple")) << "Library A should export 'triple'";

    // Define library B: depends on library A
    const char* lib_b_code = R"(
        (define-library (libs b)
          (import (scheme base) (libs a))
          (export quad sextuple)
          (begin
            (define (quad x) (double (double x)))
            (define (sextuple x) (double (triple x)))))
    )";

    Object* lib_b_expr = reader.read_from_string(lib_b_code);
    auto lib_b = parser.parse_library_definition(lib_b_expr);

    // Initialize library B (should automatically initialize and use library A)
    ASSERT_TRUE(lib_b->initialize(env, error)) << error;

    // Debug: Check what library A exports
    const auto& lib_a_exports = registered_lib_a->exports();
    EXPECT_EQ(lib_a_exports.size(), 2) << "Library A should have 2 exports";
    if (lib_a_exports.size() >= 1) {
        EXPECT_EQ(lib_a_exports[0].external_name, "double");
    }
    if (lib_a_exports.size() >= 2) {
        EXPECT_EQ(lib_a_exports[1].external_name, "triple");
    }

    // Debug: Try to get symbols directly from library A
    Object* double_from_a = registered_lib_a->get_symbol("double");
    Object* triple_from_a = registered_lib_a->get_symbol("triple");
    EXPECT_NE(double_from_a, nullptr) << "Should be able to get 'double' from library A";
    EXPECT_NE(triple_from_a, nullptr) << "Should be able to get 'triple' from library A";

    // Debug: Check library B's import count
    EXPECT_GT(lib_b->import_count(), 0) << "Library B should have imports";

    // WORKAROUND: Temporarily disabled to see debug output from automatic import
    // if (!lib_b->has_symbol("double") && double_from_a) {
    //     lib_b->define_symbol("double", double_from_a);
    // }
    // if (!lib_b->has_symbol("triple") && triple_from_a) {
    //     lib_b->define_symbol("triple", triple_from_a);
    // }

    // Now library B should have the symbols
    EXPECT_TRUE(lib_b->has_symbol("double")) << "Library B should have 'double' (after manual binding)";
    EXPECT_TRUE(lib_b->has_symbol("triple")) << "Library B should have 'triple' (after manual binding)";

    // Now evaluate library B's begin blocks (defines quad and sextuple)
    ASSERT_TRUE(lib_b->evaluate_begin_blocks(eval_func, error)) << error;

    // Verify library B's own functions are now defined
    EXPECT_TRUE(lib_b->has_symbol("quad"));
    EXPECT_TRUE(lib_b->has_symbol("sextuple"));

    // Test calling library B's functions (which use library A's functions)
    // Switch to library B's environment for evaluation
    Environment* saved_env = interpreter->get_environment();
    interpreter->set_environment(lib_b->environment());

    // Test quad(5) = double(double(5)) = double(10) = 20
    Object* quad_code = reader.read_from_string("(quad 5)");
    Object* quad_result = interpreter->new_eval(quad_code, lib_b->environment());
    ASSERT_TRUE(numeric::is_integer_value(quad_result));
    EXPECT_EQ(numeric::to_int64(quad_result), 20);

    // Test sextuple(7) = double(triple(7)) = double(21) = 42
    Object* sextuple_code = reader.read_from_string("(sextuple 7)");
    Object* sextuple_result = interpreter->new_eval(sextuple_code, lib_b->environment());
    ASSERT_TRUE(numeric::is_integer_value(sextuple_result));
    EXPECT_EQ(numeric::to_int64(sextuple_result), 42);

    // Restore original environment
    interpreter->set_environment(saved_env);
}

// ============================================================================
// Test 3: Import and Use Library (REPL integration - Phase 2)
// ============================================================================

TEST_F(R7RSLibraryTest, ImportAndUseLibrary) {
    using namespace r7rs;

    // Bootstrap standard libraries first
    bootstrap_standard_libraries(env);

    // Step 1: Define library
    const char* lib_def = R"(
        (define-library (mylib math)
          (import (scheme base))
          (export add-one)
          (begin
            (define (add-one x) (+ x 1))))
    )";

    Object* lib_result = eval(lib_def);
    EXPECT_NE(lib_result, nullptr);

    // Step 2: Import and use
    const char* usage = R"(
        (import (mylib math))
        (add-one 41)
    )";

    Object* result = eval(usage);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(numeric::to_int64(result), 42);
}

// ============================================================================
// Test 4: Standard Library (scheme base) Import
// ============================================================================

TEST_F(R7RSLibraryTest, ImportSchemeBase) {
    using namespace r7rs;

    // Bootstrap standard libraries
    bootstrap_standard_libraries(env);

    // Goal: Import (scheme base) and use its exports
    const char* code = R"(
        (import (scheme base))
        (+ 1 2 3)
    )";

    Object* result = eval(code);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(numeric::to_int64(result), 6);
}

// ============================================================================
// Test 5: Selective Import (only)
// ============================================================================

TEST_F(R7RSLibraryTest, SelectiveImportOnly) {
    // Goal: Import only specific symbols from a library
    using namespace r7rs;

    // Bootstrap standard libraries into registry
    bootstrap_standard_libraries(env);

    // Create a fresh empty environment for selective import testing
    Environment* empty_env = gc->allocate<Environment>();
    gc_root<Object> empty_guard((Object*)empty_env);  // Use gc_root (heap-based) not gc_root_guard (stack-based)

    // Import only + and - into the empty environment
    Object* import_expr = reader->read_from_string("(import (only (scheme base) + -))");
    interpreter->new_eval(import_expr, empty_env);

    // Should be able to use + and -
    Object* test_expr = reader->read_from_string("(+ 10 (- 5 2))");
    Object* result = interpreter->new_eval(test_expr, empty_env);
    int actual = static_cast<int>(numeric::to_int64(result));
    EXPECT_EQ(actual, 13);

    // Verify that other symbols like * are not available
    Object* bad_expr = reader->read_from_string("(* 2 3)");
    EXPECT_THROW(interpreter->new_eval(bad_expr, empty_env), std::runtime_error);
}

// ============================================================================
// Test 6: Import with Prefix
// ============================================================================

TEST_F(R7RSLibraryTest, ImportWithPrefix) {
    // Goal: Import symbols with a prefix
    using namespace r7rs;

    // Bootstrap standard libraries
    bootstrap_standard_libraries(env);

    const char* code = R"(
        (import (prefix (scheme base) Base:))
        (Base:+ 1 2)
    )";

    Object* result = eval(code);
    int actual = static_cast<int>(numeric::to_int64(result));
    EXPECT_EQ(actual, 3);
}

// ============================================================================
// Test 7: Import with Rename
// ============================================================================

TEST_F(R7RSLibraryTest, ImportWithRename) {
    // Goal: Import symbols with different names
    using namespace r7rs;

    // Bootstrap standard libraries
    bootstrap_standard_libraries(env);

    const char* code = R"(
        (import (rename (scheme base) (+ plus) (- minus)))
        (plus 10 (minus 5 2))
    )";

    Object* result = eval(code);
    int actual = static_cast<int>(numeric::to_int64(result));
    EXPECT_EQ(actual, 13);
}

// ============================================================================
// Test 8: Import with Except
// ============================================================================

TEST_F(R7RSLibraryTest, ImportWithExcept) {
    // Goal: Import all except specific symbols
    using namespace r7rs;

    // Bootstrap standard libraries into registry
    bootstrap_standard_libraries(env);

    // Create a fresh empty environment for selective import testing
    Environment* empty_env = gc->allocate<Environment>();
    gc_root<Object> empty_guard((Object*)empty_env);  // Use gc_root (heap-based) not gc_root_guard (stack-based)

    // Import all from (scheme base) except +
    Object* import_expr = reader->read_from_string("(import (except (scheme base) +))");
    interpreter->new_eval(import_expr, empty_env);

    // Should be able to use - and other symbols
    Object* test_expr = reader->read_from_string("(- 10 3)");
    Object* result = interpreter->new_eval(test_expr, empty_env);
    int actual = static_cast<int>(numeric::to_int64(result));
    EXPECT_EQ(actual, 7);

    // Verify that + is not available
    Object* bad_expr = reader->read_from_string("(+ 1 2)");
    EXPECT_THROW(interpreter->new_eval(bad_expr, empty_env), std::runtime_error);
}

// ============================================================================
// Test 9: Multiple Libraries
// ============================================================================

TEST_F(R7RSLibraryTest, MultipleLibraries) {
    using namespace r7rs;

    // Bootstrap standard libraries
    bootstrap_standard_libraries(env);

    // Goal: Define and use multiple libraries
    const char* lib1 = R"(
        (define-library (lib one)
          (import (scheme base))
          (export double)
          (begin
            (define (double x) (* x 2))))
    )";

    const char* lib2 = R"(
        (define-library (lib two)
          (import (scheme base) (lib one))
          (export quad)
          (begin
            (define (quad x) (double (double x)))))
    )";

    const char* usage = R"(
        (import (lib two))
        (quad 5)
    )";

    Object* result1 = eval(lib1);
    EXPECT_NE(result1, nullptr);

    Object* result2 = eval(lib2);
    EXPECT_NE(result2, nullptr);

    Object* result = eval(usage);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(numeric::to_int64(result), 20);
}

// ============================================================================
// Test 10: Export Validation
// ============================================================================

TEST_F(R7RSLibraryTest, ExportUndefinedSymbolError) {
    // Goal: Error when exporting undefined symbol

    using namespace r7rs;

    // Bootstrap standard libraries
    bootstrap_standard_libraries(env);

    const char* code = R"(
        (define-library (test bad-export)
          (import (scheme base))
          (export undefined-function)
          (begin
            (define (defined-function x) x)))
    )";

    // Parse and initialize the library
    scheme_reader reader;
    Object* lib_expr = reader.read_from_string(code);
    ASSERT_NE(lib_expr, nullptr);

    LibraryParser parser;
    auto lib = parser.parse_library_definition(lib_expr);

    std::string error;
    ASSERT_TRUE(lib->initialize(env, error)) << error;

    auto eval_func = [this](Object* form, Environment* env_arg) {
        return this->interpreter->new_eval(form, env_arg);
    };
    ASSERT_TRUE(lib->evaluate_begin_blocks(eval_func, error)) << error;

    // Validation should fail because undefined-function is not defined
    std::string validation_errors;
    EXPECT_FALSE(lib->validate(validation_errors));
    EXPECT_NE(validation_errors.find("undefined-function"), std::string::npos)
        << "Error should mention the undefined symbol. Got: " << validation_errors;
    EXPECT_NE(validation_errors.find("not defined"), std::string::npos)
        << "Error should say symbol is not defined. Got: " << validation_errors;
}

// ============================================================================
// Test 11: Namespace Isolation
// ============================================================================

TEST_F(R7RSLibraryTest, NamespaceIsolation) {
    // Goal: Verify that libraries have isolated namespaces

    using namespace r7rs;

    // Bootstrap standard libraries
    bootstrap_standard_libraries(env);

    const char* lib1 = R"(
        (define-library (lib isolated-one)
          (import (scheme base))
          (export public-func)
          (begin
            (define (private-func x) x)
            (define (public-func x) (private-func x))))
    )";

    // Parse and initialize the library
    scheme_reader reader;
    Object* lib_expr = reader.read_from_string(lib1);
    ASSERT_NE(lib_expr, nullptr);

    LibraryParser parser;
    auto lib = parser.parse_library_definition(lib_expr);

    std::string error;
    ASSERT_TRUE(lib->initialize(env, error)) << error;

    auto eval_func = [this](Object* form, Environment* env_arg) {
        return this->interpreter->new_eval(form, env_arg);
    };
    ASSERT_TRUE(lib->evaluate_begin_blocks(eval_func, error)) << error;

    // Verify the library has both symbols
    EXPECT_TRUE(lib->has_symbol("private-func")) << "Library should have private-func";
    EXPECT_TRUE(lib->has_symbol("public-func")) << "Library should have public-func";

    // Verify only public-func is exported
    EXPECT_FALSE(lib->exports_symbol("private-func")) << "private-func should not be exported";
    EXPECT_TRUE(lib->exports_symbol("public-func")) << "public-func should be exported";

    LibraryRegistry::instance().register_library(std::move(lib));

    // Use scheme_engine to import and test
    scheme_engine* eng = get_engine();

    // public-func should work
    Object* result = static_cast<Object*>(eng->eval_string("(import (lib isolated-one)) (public-func 42)"));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(numeric::to_int64(result), 42);

    // private-func should NOT be accessible (unbound variable error)
    Object* private_result = static_cast<Object*>(eng->eval_string("(private-func 42)"));
    EXPECT_EQ(private_result, nullptr) << "private-func should not be accessible after import";
    // Check that an error was set
    std::string last_error = eng->get_last_error();
    EXPECT_NE(last_error.find("Unbound variable"), std::string::npos)
        << "Should get unbound variable error. Got: " << last_error;
}

// ============================================================================
// Test 12: Re-export Imported Symbols
// ============================================================================

TEST_F(R7RSLibraryTest, ReExportImportedSymbols) {
    // Goal: Library can re-export symbols it imports

    using namespace r7rs;

    // Bootstrap standard libraries
    bootstrap_standard_libraries(env);

    // Define lib1: (lib original) with foo
    const char* lib1 = R"(
        (define-library (lib original)
          (import (scheme base))
          (export foo)
          (begin
            (define (foo x) (* x 2))))
    )";

    scheme_reader reader;
    Object* lib1_expr = reader.read_from_string(lib1);
    ASSERT_NE(lib1_expr, nullptr);

    LibraryParser parser;
    auto lib1_obj = parser.parse_library_definition(lib1_expr);

    std::string error;
    ASSERT_TRUE(lib1_obj->initialize(env, error)) << error;

    auto eval_func = [this](Object* form, Environment* env_arg) {
        return this->interpreter->new_eval(form, env_arg);
    };
    ASSERT_TRUE(lib1_obj->evaluate_begin_blocks(eval_func, error)) << error;

    LibraryRegistry::instance().register_library(std::move(lib1_obj));

    // Define lib2: (lib reexporter) that re-exports foo from original
    const char* lib2 = R"(
        (define-library (lib reexporter)
          (import (lib original))
          (export foo))
    )";

    Object* lib2_expr = reader.read_from_string(lib2);
    ASSERT_NE(lib2_expr, nullptr);

    auto lib2_obj = parser.parse_library_definition(lib2_expr);

    ASSERT_TRUE(lib2_obj->initialize(env, error)) << error;
    // No begin blocks to evaluate in lib2

    // Verify that lib2 has foo in its environment (from import)
    EXPECT_TRUE(lib2_obj->has_symbol("foo")) << "lib2 should have foo from import";
    EXPECT_TRUE(lib2_obj->exports_symbol("foo")) << "lib2 should export foo";

    LibraryRegistry::instance().register_library(std::move(lib2_obj));

    // Use lib2's re-exported symbol
    scheme_engine* eng = get_engine();
    Object* result = static_cast<Object*>(eng->eval_string("(import (lib reexporter)) (foo 21)"));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(numeric::to_int64(result), 42);
}

// ============================================================================
// Test 13: Hierarchical Library Names
// ============================================================================

TEST_F(R7RSLibraryTest, HierarchicalLibraryNames) {
    // Goal: Support multi-level library names like (myapp utils string)

    using namespace r7rs;

    // Bootstrap standard libraries
    bootstrap_standard_libraries(env);

    const char* code = R"(
        (define-library (myapp utils string)
          (import (scheme base))
          (export string-double)
          (begin
            (define (string-double s) (string-append s s))))
    )";

    // Parse and initialize the library
    scheme_reader reader;
    Object* lib_expr = reader.read_from_string(code);
    ASSERT_NE(lib_expr, nullptr);

    LibraryParser parser;
    auto lib = parser.parse_library_definition(lib_expr);

    // Verify the library name has 3 components
    EXPECT_EQ(lib->name().size(), 3);
    EXPECT_EQ(lib->name()[0], "myapp");
    EXPECT_EQ(lib->name()[1], "utils");
    EXPECT_EQ(lib->name()[2], "string");
    EXPECT_EQ(lib->name_string(), "(myapp utils string)");

    // Initialize and evaluate
    std::string error;
    ASSERT_TRUE(lib->initialize(env, error)) << error;

    auto eval_func = [this](Object* form, Environment* env_arg) {
        return this->interpreter->new_eval(form, env_arg);
    };
    ASSERT_TRUE(lib->evaluate_begin_blocks(eval_func, error)) << error;

    // Register the library
    LibraryRegistry::instance().register_library(std::move(lib));

    // Import and use it via scheme_engine
    scheme_engine* eng = get_engine();
    Object* result = static_cast<Object*>(eng->eval_string("(import (myapp utils string)) (string-double \"hello\")"));
    ASSERT_NE(result, nullptr);

    a_string* str = dynamic_cast<a_string*>(result);
    ASSERT_NE(str, nullptr);
    EXPECT_EQ(str->value(), "hellohello");
}

// ============================================================================
// Test 14: Circular Dependency Detection
// ============================================================================

TEST_F(R7RSLibraryTest, CircularDependencyError) {
    // Goal: Detect and report circular dependencies
    using namespace r7rs;

    // Bootstrap standard libraries
    bootstrap_standard_libraries(env);

    const char* lib1 = R"(
        (define-library (lib a)
          (import (scheme base) (lib b))
          (export func-a)
          (begin
            (define (func-a) 1)))
    )";

    const char* lib2 = R"(
        (define-library (lib b)
          (import (scheme base) (lib a))
          (export func-b)
          (begin
            (define (func-b) 2)))
    )";

    // Parse library A
    scheme_reader reader;
    Object* lib1_expr = reader.read_from_string(lib1);
    ASSERT_NE(lib1_expr, nullptr);

    LibraryParser parser;
    auto lib1_obj = parser.parse_library_definition(lib1_expr);
    ASSERT_NE(lib1_obj, nullptr);

    // Parse library B
    Object* lib2_expr = reader.read_from_string(lib2);
    ASSERT_NE(lib2_expr, nullptr);

    auto lib2_obj = parser.parse_library_definition(lib2_expr);
    ASSERT_NE(lib2_obj, nullptr);

    // Register both libraries (not yet initialized)
    LibraryRegistry::instance().register_library(std::move(lib1_obj));
    LibraryRegistry::instance().register_library(std::move(lib2_obj));

    // Now try to initialize library A
    // This should fail because A imports B, and B imports A (circular!)
    Library* lib_a = LibraryRegistry::instance().get_library({"lib", "a"});
    ASSERT_NE(lib_a, nullptr);

    std::string error;
    bool success = lib_a->initialize(env, error);

    // Initialization should fail with circular dependency error
    EXPECT_FALSE(success) << "Expected initialization to fail due to circular dependency";

    // Check for "Circular" (case-insensitive)
    std::string error_lower = error;
    std::transform(error_lower.begin(), error_lower.end(), error_lower.begin(), ::tolower);
    EXPECT_NE(error_lower.find("circular"), std::string::npos) << "Error should mention circular dependency, got: " << error;
}

// ============================================================================
// Documentation Test
// ============================================================================

TEST_F(R7RSLibraryTest, DocumentationExample) {
    // This test documents the R7RS library system for future reference
    // Demonstrates the expected syntax and usage

    /*
    R7RS Library System Overview:

    1. Define a library:
       (define-library (library name)
         (import (other library))
         (export exported-symbol ...)
         (begin
           (define (exported-symbol ...) ...)))

    2. Import forms:
       (import (library name))                     ; Simple import
       (import (only (lib) sym1 sym2))             ; Import only specific symbols
       (import (except (lib) sym1))                ; Import all except
       (import (prefix (lib) prefix:))             ; Import with prefix
       (import (rename (lib) (old new)))           ; Import with rename

    3. Standard libraries:
       (scheme base)        - Core R7RS functionality
       (scheme read)        - Reading S-expressions
       (scheme write)       - Writing output
       (scheme file)        - File I/O
       (scheme char)        - Character operations
       ... and more

    4. Example multi-file program:

       ;; File: mylib.scm
       (define-library (myapp core)
         (import (scheme base))
         (export process-data)
         (begin
           (define (process-data x) (* x x))))

       ;; File: main.scm
       (import (scheme base)
               (scheme write)
               (myapp core))

       (display (process-data 42))
    */

    SUCCEED() << "See test source for R7RS library documentation";
}

// ============================================================================
// Test 27: File-Based Library Loading
// ============================================================================

TEST_F(R7RSLibraryTest, LoadLibraryFromFile) {
    // Get scheme_engine wrapper for R7RS library file loading
    scheme_engine* eng = get_engine();

    // Ensure registry is clear before bootstrapping (defensive)
    r7rs::LibraryRegistry::instance().clear();

    // Bootstrap standard R7RS libraries
    r7rs::bootstrap_standard_libraries(eng->get_environment());

    // Load a library from a .scm file
    // Try multiple possible paths (depends on where test is run from)
    std::vector<std::string> possible_paths = {
        std::string(ZOMBIE_TEST_LIBS_DIR) + "/my/math.scm",  // CWD-independent (CMake-provided)
        "test-libs/my/math.scm",           // Running from repo root
        "../../test-libs/my/math.scm",     // Running from x64/Debug
        "../../../test-libs/my/math.scm"   // Running from deeper directory
    };

    bool loaded = false;
    for (const auto& path : possible_paths) {
        loaded = eng->load_library_file(path);
        if (loaded) break;
    }
    ASSERT_TRUE(loaded) << "Failed to load library: " << eng->get_last_error();

    // Import and use the library
    Object* result = static_cast<Object*>(eng->eval_string("(import (my math)) (square 5)"));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(numeric::to_int64(result), 25);

    // Test other functions from the file
    result = static_cast<Object*>(eng->eval_string("(cube 3)"));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(numeric::to_int64(result), 27);

    result = static_cast<Object*>(eng->eval_string("(factorial 5)"));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(numeric::to_int64(result), 120);

    result = static_cast<Object*>(eng->eval_string("(add-two 10 32)"));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(numeric::to_int64(result), 42);
}

// ============================================================================
// Test 28: Auto-Load Library From File
// ============================================================================

TEST_F(R7RSLibraryTest, AutoLoadLibraryFromFile) {
    // Get scheme_engine wrapper for R7RS library file loading
    scheme_engine* eng = get_engine();

    // Ensure registry is clear before bootstrapping (defensive)
    r7rs::LibraryRegistry::instance().clear();

    // Bootstrap standard R7RS libraries
    r7rs::bootstrap_standard_libraries(eng->get_environment());

    // Add all possible search paths (depends on where test is run from)
    eng->add_library_search_path(ZOMBIE_TEST_LIBS_DIR);  // CWD-independent (CMake-provided)
    eng->add_library_search_path("test-libs");
    eng->add_library_search_path("../../test-libs");
    eng->add_library_search_path("../../../test-libs");
    eng->set_auto_load_libraries(true);

    // Import should automatically load the library from file
    Object* result = static_cast<Object*>(eng->eval_string("(import (my math)) (square 7)"));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(numeric::to_int64(result), 49);

    // Test that it works for subsequent uses
    result = static_cast<Object*>(eng->eval_string("(factorial 4)"));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(numeric::to_int64(result), 24);
}

// ============================================================================
// Test 29: Nested Library Dependencies From Files
// ============================================================================

TEST_F(R7RSLibraryTest, NestedLibraryDependenciesFromFiles) {
    // Get scheme_engine wrapper for R7RS library file loading
    scheme_engine* eng = get_engine();

    // Ensure registry is clear before bootstrapping (defensive)
    r7rs::LibraryRegistry::instance().clear();

    // Bootstrap standard R7RS libraries
    r7rs::bootstrap_standard_libraries(eng->get_environment());

    // Add all possible search paths (depends on where test is run from)
    eng->add_library_search_path(ZOMBIE_TEST_LIBS_DIR);  // CWD-independent (CMake-provided)
    eng->add_library_search_path("test-libs");
    eng->add_library_search_path("../../test-libs");
    eng->add_library_search_path("../../../test-libs");
    eng->set_auto_load_libraries(true);

    // First import the Base library so it's available for the advanced library
    Object* import_base = static_cast<Object*>(eng->eval_string("(import (my math))"));
    ASSERT_NE(import_base, nullptr) << "Error loading Base library: " << eng->get_last_error();

    // Load advanced library which depends on basic math library
    // (my math advanced) imports (my math), both should auto-load
    Object* result = static_cast<Object*>(eng->eval_string("(import (my math advanced)) (power 2 10)"));
    ASSERT_NE(result, nullptr) << "Error: " << eng->get_last_error();
    EXPECT_EQ(numeric::to_int64(result), 1024);

    // Test function that uses imported function from dependency
    result = static_cast<Object*>(eng->eval_string("(sum-of-squares 3 4)"));
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(numeric::to_int64(result), 25);  // 3^2 + 4^2 = 9 + 16 = 25
}
