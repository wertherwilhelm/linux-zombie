/**
 * @file VMGCRecursionTests.cpp
 * @brief Regression test for the bytecode VM's GC rooting.
 *
 * The BytecodeVM keeps its call frames (and the intermediate values they hold)
 * in a heap-allocated vector, not on the C++ stack and not reachable from the
 * object graph. A recursion that holds one call's result live across a second
 * call + allocation will lose that intermediate if a collection fires and the
 * VM's frames aren't traced -> wrong result or "cannot take first of an atom!".
 * The fix registers the VM's trace_references with the GC (a root-tracer
 * callback invoked during mark()).
 *
 * The regular suite uses a 100MB GC threshold so collections almost never fire.
 * Here we drop the threshold AND make the recursion allocate garbage while an
 * intermediate is live, so GC runs mid-recursion -- which is what exercises the
 * fix. Covers every evaluator (the bytecode VM backs both --bytecode and
 * --coreir).
 */

#include "gtest/gtest.h"
#include "zombie_runtime_types.h"
#include "scheme_engine.h"
#include "gc_manager.h"

extern gc_manager* gc;

namespace {

// fib, but the first sibling's result `a` is held live across a garbage
// allocation (make-list) and the second recursive call, so a collection
// triggered there must not reclaim it. Value is still fib(n).
const char* kProgram =
    "(define (fib n)"
    "  (if (< n 2) n"
    "      (let ((a (fib (- n 1))))"
    "        (make-list 120 0)"       // churn the heap while `a` is live
    "        (+ a (fib (- n 2))))))"
    "(fib 20)";  // 6765

void expect_fib_under_gc(EvaluatorType type) {
    size_t saved = gc->gc_threshold();
    gc->set_gc_threshold(512 * 1024);  // low enough to collect during the recursion
    {
        scheme_engine engine(type);
        Object* r = static_cast<Object*>(engine.eval_string(kProgram));
        gc->set_gc_threshold(saved);  // restore before asserting (keeps teardown sane)
        ASSERT_NE(r, nullptr) << "evaluator " << static_cast<int>(type);
        EXPECT_EQ(engine.print_value(r), "6765")
            << "evaluator " << static_cast<int>(type);
    }
    gc->set_gc_threshold(saved);
}

}  // namespace

TEST(VMGCRecursion, TreeWalkFibUnderGC)  { expect_fib_under_gc(EvaluatorType::LISP); }
TEST(VMGCRecursion, ToyVMFibUnderGC)     { expect_fib_under_gc(EvaluatorType::TOY_VM); }
TEST(VMGCRecursion, BytecodeFibUnderGC)  { expect_fib_under_gc(EvaluatorType::BYTECODE); }
TEST(VMGCRecursion, CoreIRFibUnderGC)    { expect_fib_under_gc(EvaluatorType::COREIR); }
