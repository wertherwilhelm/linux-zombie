#pragma once
#include "pch.h"
#include <string>
#include <sstream>
#include <vector>

// Zombie Lisp includes
#include "Object.h"
#include "atom.h"
#include "cons_cell.h"
#include "Lisp.h"
#include "Environment.h"
#include "primitive_base.h"
#include "primitive.h"  // For Primitive::install_primitives()
#include "constants.h"
#include "gc_manager.h"
#include "scheme_reader.h"
#include "numeric_runtime.h"
#include "Base.h"
#include "scheme_engine.h"

// Access to global GC instance
extern gc_manager* gc;

// NOTE: operator<< for a_double and a_string are provided by atom.h template
// (std::ostream& operator<<(std::ostream& os, const atom<T>* t))
// No need to define them here - doing so causes ambiguity with ADL

// Test fixture for Zombie Scheme tests
class ZombieSchemeTest : public ::testing::Test {
protected:
    Lisp* interpreter;
    Environment* env;
    scheme_reader* reader;

    // GC root guards to protect interpreter and env from collection
    gc_root_guard* guard_interpreter;
    gc_root_guard* guard_env;

    // Optional scheme_engine wrapper for higher-level API tests (R7RS libraries, etc.)
    scheme_engine* engine;

    void SetUp() override {
        // Initialize GC and constants if not already done
        static bool initialized = false;
        if (!initialized) {
            init_lisp_constants();
            initialized = true;
        }

        // Create interpreter and environment for each test
        // IMPORTANT: Use gc->allocate to ensure GC tracking
        interpreter = gc->allocate<Lisp>();
        env = gc->allocate<Environment>();

        // Protect from GC immediately
        guard_interpreter = new gc_root_guard((Object*)interpreter);
        guard_env = new gc_root_guard((Object*)env);

        zombie::runtime::Primitive::install_primitives(env);
        interpreter->set_environment(env);
        reader = new scheme_reader();

        // Initialize engine to nullptr (created on demand)
        engine = nullptr;
    }

    void TearDown() override {
        // Cleanup is handled by GC
        // Don't delete interpreter/env manually - GC owns them
        delete reader;

        // Delete engine if it was created
        if (engine) {
            delete engine;
            engine = nullptr;
        }

        // Unregister GC roots
        delete guard_env;
        delete guard_interpreter;
    }

    // Helper: Evaluate a Scheme expression from string
    // If the string contains multiple expressions, wraps them in (begin ...)
    Object* eval(const std::string& code) {
        // Try to parse as program (multiple expressions)
        Object* program = reader->read_program_from_string(code);

        // If single expression, eval it directly
        if (!program || is_nil(program)) {
            return nil;
        }

        // GC SAFETY: Protect program during evaluation
        gc_root<Object> guard_program(program);

        // If it's a list with one element, eval that element
        if (program->pair() && is_nil(cdr(program))) {
            return interpreter->new_eval(car(program), env);
        }

        // Multiple expressions: wrap in (begin ...)
        // IMPORTANT: Use gc_new instead of raw new to ensure GC tracking
        Object* begin_list = cons(gc_new<a_symbol>("begin"), program);
        gc_root<Object> guard_begin(begin_list);  // GC SAFETY
        return interpreter->new_eval(begin_list, env);
    }

    // Helper: Evaluate and expect integer result
    void expectInt(const std::string& code, int expected) {
        Object* result = eval(code);
        ASSERT_NE(result, nullptr) << "Expression returned null: " << code;

        ASSERT_TRUE(numeric::is_number(result)) << "Result is not a number: " << code;

        // Convert to integer value
        int actual;
        if (result->get_type() == static_cast<int>(Type::INT) || result->get_type() == static_cast<int>(Type::LONG)) {
            actual = static_cast<int>(numeric::to_int64(result));
        } else {
            actual = static_cast<int>(numeric::to_double(result));
        }

        EXPECT_EQ(actual, expected) << "For expression: " << code;
    }

    // Helper: Evaluate and expect double result
    void expectDouble(const std::string& code, double expected, double tolerance = 0.0001) {
        Object* result = eval(code);
        ASSERT_NE(result, nullptr) << "Expression returned null: " << code;

        ASSERT_TRUE(numeric::is_number(result)) << "Result is not a number: " << code;

        // Convert to double value
        double actual = numeric::to_double(result);
        EXPECT_NEAR(actual, expected, tolerance) << "For expression: " << code;
    }

    // Helper: Evaluate and expect string result
    void expectString(const std::string& code, const std::string& expected) {
        Object* result = eval(code);
        ASSERT_NE(result, nullptr) << "Expression returned null: " << code;

        a_string* str = dynamic_cast<a_string*>(result);
        ASSERT_NE(str, nullptr) << "Result is not a string: " << code;
        EXPECT_EQ(str->value(), expected) << "For expression: " << code;
    }

    // Helper: Evaluate and expect symbol result
    void expectSymbol(const std::string& code, const std::string& expected) {
        Object* result = eval(code);
        ASSERT_NE(result, nullptr) << "Expression returned null: " << code;

        a_symbol* sym = dynamic_cast<a_symbol*>(result);
        ASSERT_NE(sym, nullptr) << "Result is not a symbol: " << code;
        EXPECT_EQ(sym->getName(), expected) << "For expression: " << code;
    }

    // Helper: Evaluate and expect boolean result
    void expectBool(const std::string& code, bool expected) {
        Object* result = eval(code);
        ASSERT_NE(result, nullptr) << "Expression returned null: " << code;

        // R5RS: #f is the only false value, everything else is true
        if (expected) {
            EXPECT_NE(result, false_obj) << "Expected true for: " << code;
        } else {
            EXPECT_EQ(result, false_obj) << "Expected false for: " << code;
        }
    }

    // Helper: Evaluate and expect nil result
    void expectNil(const std::string& code) {
        Object* result = eval(code);
        EXPECT_EQ(result, nil) << "Expected nil for: " << code;
    }

    // Helper: Evaluate multiple expressions (like begin)
    Object* evalMulti(const std::vector<std::string>& expressions) {
        Object* result = nil;
        for (const auto& expr : expressions) {
            result = eval(expr);
        }
        return result;
    }

    // Helper: Check if a Primitive/function exists
    bool primitiveExists(const std::string& name) {
        Object* prim = eval(name);
        return (prim != nullptr && prim != nil);
    }

    // Helper: Get or create scheme_engine wrapper (for R7RS library tests)
    scheme_engine* get_engine() {
        if (!engine) {
            engine = new scheme_engine();
        }
        return engine;
    }
};
