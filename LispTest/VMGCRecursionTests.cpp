/**
 * @file VMGCRecursionTests.cpp
 * @brief Regression test for the bytecode VM's and toy VM's GC rooting.
 *
 * Both VMs keep their call frames (and the intermediate values those frames
 * hold) in heap-allocated members, not on the C++ stack and not reachable from
 * the object graph. A recursion that holds one call's result live across a
 * second call + allocation loses that intermediate if a collection fires and
 * the VM's frames aren't traced -> wrong result / "cannot take first of an
 * atom!" / toy-VM segfault. The fix registers each VM's trace_references with
 * the GC (root-tracer callbacks invoked during mark()).
 *
 * The suite runs with a 100MB GC threshold, so collections almost never fire.
 * To exercise the fix we must make GC run *during* the recursion. We first
 * collect() to reclaim accumulated garbage, then set the threshold just above
 * the (now small) live set, so collections fire during fib's allocation while
 * scanning only a small heap -- fast regardless of how much the rest of the
 * suite has allocated. Covers all four evaluators (the bytecode VM backs both
 * --bytecode and --coreir).
 */

#include "gtest/gtest.h"
#include "zombie_runtime_types.h"
#include "scheme_engine.h"
#include "gc_manager.h"

extern gc_manager* gc;

namespace {

// fib, but the first sibling's result `a` is held live across a garbage
// allocation and the second recursive call. Value is still fib(n).
const char* kDefine =
    "(define (fib n)"
    "  (if (< n 2) n"
    "      (let ((a (fib (- n 1))))"
    "        (make-list 120 0)"       // churn the heap while `a` is live
    "        (+ a (fib (- n 2))))))";

void expect_fib_under_gc(EvaluatorType type) {
    size_t saved = gc->gc_threshold();
    scheme_engine engine(type);
    engine.eval_string(kDefine);

    // Reclaim accumulated garbage, then force collections during the recursion
    // while keeping each collection cheap (scan only the small live set).
    gc->collect();
    gc->set_gc_threshold(gc->bytes_allocated() + 1024 * 1024);

    Object* r = static_cast<Object*>(engine.eval_string("(fib 20)"));  // 6765

    gc->set_gc_threshold(saved);  // restore before asserting
    ASSERT_NE(r, nullptr) << "evaluator " << static_cast<int>(type);
    EXPECT_EQ(engine.print_value(r), "6765") << "evaluator " << static_cast<int>(type);
}

}  // namespace

TEST(VMGCRecursion, TreeWalkFibUnderGC)  { expect_fib_under_gc(EvaluatorType::LISP); }
TEST(VMGCRecursion, ToyVMFibUnderGC)     { expect_fib_under_gc(EvaluatorType::TOY_VM); }
TEST(VMGCRecursion, BytecodeFibUnderGC)  { expect_fib_under_gc(EvaluatorType::BYTECODE); }
TEST(VMGCRecursion, CoreIRFibUnderGC)    { expect_fib_under_gc(EvaluatorType::COREIR); }
