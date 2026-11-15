// DifferentialTests.cpp - cross-evaluator conformance.
//
// Runs one corpus of programs through all four evaluators (tree-walk, bytecode,
// CoreIR, toy VM) and asserts each produces the tree-walk reference's result.
// This is the regression guard for the "evaluators silently disagree" class of
// bug (set! return, null?/'(), define return, CoreIR n-ary arithmetic were all
// found this way).
//
// Each case carries a `mask` of the evaluators expected to agree. Evaluators
// with a KNOWN GAP on a program are masked OUT so they are not even executed —
// important because some gaps are hard crashes (toy VM segfaults on `map`) that
// would take down the whole test binary, not just fail an assertion. The mask
// is the living gap list: when a gap is fixed, flip the bit back to ALL and the
// case starts guarding that evaluator too. See the KNOWN GAPS block below.
#include "gtest/gtest.h"
#include "zombie_runtime_types.h"
#include "scheme_engine.h"
#include "constants.h"
#include <string>
#include <vector>

namespace {

enum EvBit { L = 1, B = 2, C = 4, V = 8, ALL = 15 };

struct Case { const char* code; const char* expect; int mask; };

struct EvEntry { int bit; EvaluatorType type; const char* name; };
const EvEntry kEvs[] = {
    {L, EvaluatorType::LISP,     "lisp"},
    {B, EvaluatorType::BYTECODE, "bytecode"},
    {C, EvaluatorType::COREIR,   "coreir"},
    {V, EvaluatorType::TOY_VM,   "toyvm"},
};

std::string run_in(EvaluatorType t, const std::string& code) {
    scheme_engine engine(t);
    void* r = engine.eval_string(code);
    return r ? engine.print_value(r) : std::string("<null>");
}

// -------------------------------------------------------------------------
// KNOWN GAPS (masked out below; remove the mask when fixed): none currently.
// FIXED (now covered on all four):
//   - closure applied from inside a primitive (map/filter/for-each) — was a
//     toy-VM re-entrant-eval hang/segfault.
//   - named let `(let loop ...)` — was broken in bytecode and toy VM; the shared
//     Expander now desugars it to letrec for every evaluator.
//   - one-armed `if` / `when` / `unless` — toy VM read the (absent) else branch
//     unconditionally; now returns unspecified when the else branch is missing.
//   - `procedure?` — was missing in every evaluator; now a registered primitive.
// -------------------------------------------------------------------------
const Case kCorpus[] = {
    // arithmetic / comparison (incl. n-ary — the CoreIR drop-operand regression)
    {"(+ 1 2 3)", "6", ALL}, {"(* 2 3 4)", "24", ALL}, {"(- 10 3 2)", "5", ALL},
    {"(/ 24 2 3)", "4", ALL}, {"(+ 5)", "5", ALL}, {"(modulo 17 5)", "2", ALL},
    {"(quotient 17 5)", "3", ALL}, {"(remainder 17 5)", "2", ALL},
    {"(max 3 1 4 1 5)", "5", ALL}, {"(min 3 1 4 1 5)", "1", ALL}, {"(abs -7)", "7", ALL},
    {"(< 1 2 3)", "#t", ALL}, {"(< 3 2 1)", "#f", ALL}, {"(= 2 2 2)", "#t", ALL},
    {"(> 3 2 1)", "#t", ALL}, {"(expt 2 10)", "1024", ALL}, {"(gcd 12 18)", "6", ALL},
    // predicates
    {"(zero? 0)", "#t", ALL}, {"(even? 4)", "#t", ALL}, {"(odd? 3)", "#t", ALL},
    {"(number? 5)", "#t", ALL}, {"(null? '())", "#t", ALL}, {"(null? '(1))", "#f", ALL},
    {"(pair? '(1))", "#t", ALL}, {"(symbol? 'a)", "#t", ALL}, {"(boolean? #t)", "#t", ALL},
    {"(procedure? car)", "#t", ALL}, {"(procedure? (lambda (x) x))", "#t", ALL},
    {"(procedure? 5)", "#f", ALL},
    // booleans / equivalence
    {"(and 1 2 3)", "3", ALL}, {"(or #f #f 7)", "7", ALL}, {"(not #f)", "#t", ALL},
    {"(not 5)", "#f", ALL}, {"(eq? 'a 'a)", "#t", ALL}, {"(equal? '(1 2) '(1 2))", "#t", ALL},
    // conditionals
    {"(if (< 1 2) 'yes 'no)", "yes", ALL},
    {"(cond ((= 1 2) 'a) ((= 2 2) 'b) (else 'c))", "b", ALL},
    {"(case 2 ((1) 'one) ((2) 'two) (else 'other))", "two", ALL},
    {"(if #f 'yes)", "()", ALL}, {"(if #t 'yes)", "yes", ALL},
    {"(when #t 1 2 3)", "3", ALL}, {"(when #f 1)", "()", ALL},
    {"(unless #f 9)", "9", ALL}, {"(unless #t 9)", "()", ALL},
    // binding forms
    {"(let ((x 1) (y 2)) (+ x y))", "3", ALL},
    {"(let* ((x 1) (y (+ x 1))) (* x y))", "2", ALL},
    {"(letrec ((e? (lambda (n) (if (= n 0) #t (o? (- n 1))))) (o? (lambda (n) (if (= n 0) #f (e? (- n 1)))))) (e? 10))", "#t", ALL},
    {"(let loop ((i 0) (s 0)) (if (< i 5) (loop (+ i 1) (+ s i)) s))", "10", ALL},
    {"(let loop ((i 0)) (if (< i 50) (loop (+ i 1)) i))", "50", ALL},
    // lambda / closures / apply
    {"((lambda (x) (* x x)) 6)", "36", ALL},
    {"((lambda (a b c) (+ a b c)) 1 2 3)", "6", ALL},
    {"(((lambda (x) (lambda (y) (+ x y))) 3) 4)", "7", ALL},
    {"(apply + '(1 2 3 4))", "10", ALL},
    // list operations
    {"(car '(1 2 3))", "1", ALL}, {"(cdr '(1 2 3))", "(2 3)", ALL},
    {"(cons 1 '(2 3))", "(1 2 3)", ALL}, {"(cons 1 2)", "(1 . 2)", ALL},
    {"(list 1 2 3)", "(1 2 3)", ALL}, {"(length '(1 2 3 4))", "4", ALL},
    {"(append '(1 2) '(3 4))", "(1 2 3 4)", ALL}, {"(reverse '(1 2 3))", "(3 2 1)", ALL},
    {"(member 2 '(1 2 3))", "(2 3)", ALL}, {"(assoc 'b '((a 1) (b 2)))", "(b 2)", ALL},
    {"(cadr '(1 2 3))", "2", ALL}, {"(caddr '(1 2 3))", "3", ALL},
    // Closure applied from inside a primitive (map/filter/for-each) — exercises
    // re-entrant eval on every evaluator (was a toy-VM hang/segfault).
    {"(map (lambda (x) (* x x)) '(1 2 3))", "(1 4 9)", ALL},
    {"(map + '(1 2 3) '(10 20 30))", "(11 22 33)", ALL},
    {"(filter (lambda (x) (> x 2)) '(1 2 3 4))", "(3 4)", ALL},
    // quote / quasiquote
    {"(quote (a b c))", "(a b c)", ALL}, {"'(1 2 3)", "(1 2 3)", ALL},
    {"`(1 ,(+ 1 1) 3)", "(1 2 3)", ALL}, {"`(1 ,@(list 2 3) 4)", "(1 2 3 4)", ALL},
    // strings / symbols (display-style printing: no quotes)
    {"(string-length \"hello\")", "5", ALL},
    {"(string-append \"a\" \"b\" \"c\")", "abc", ALL},
    {"(substring \"hello\" 1 3)", "el", ALL},
    {"(symbol->string 'foo)", "foo", ALL}, {"(number->string 42)", "42", ALL},
    // sequencing / define / set!
    {"(begin 1 2 3)", "3", ALL},
    {"(begin (define z 5) (set! z (* z z)) z)", "25", ALL},
    {"(let ((x 1)) (set! x 10) x)", "10", ALL},
    // recursion
    {"(letrec ((fact (lambda (n) (if (< n 2) 1 (* n (fact (- n 1))))))) (fact 6))", "720", ALL},
    {"(letrec ((fib (lambda (n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2))))))) (fib 10))", "55", ALL},
};

} // namespace

class DifferentialTest : public ::testing::Test {
protected:
    void SetUp() override { init_lisp_constants(); }
};

TEST_F(DifferentialTest, AllEvaluatorsMatchTreeWalk) {
    for (const auto& c : kCorpus) {
        for (const auto& ev : kEvs) {
            if (!(c.mask & ev.bit)) continue;  // masked-out: known gap, do not run
            std::string got;
            try {
                got = run_in(ev.type, c.code);
            } catch (const std::exception& e) {
                ADD_FAILURE() << ev.name << " threw on  " << c.code << " : " << e.what();
                continue;
            }
            EXPECT_EQ(got, c.expect) << ev.name << " diverged on: " << c.code;
        }
    }
}
