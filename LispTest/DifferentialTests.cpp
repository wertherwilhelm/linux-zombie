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
    {"(boolean=? #t #t)", "#t", ALL}, {"(boolean=? #t #f)", "#f", ALL},
    // conditionals
    {"(if (< 1 2) 'yes 'no)", "yes", ALL},
    {"(cond ((= 1 2) 'a) ((= 2 2) 'b) (else 'c))", "b", ALL},
    // cond edge cases: => arrow, no-body clause (returns the test value),
    // exhausted no-else (unspecified -> ()).
    {"(cond ((assv 2 '((1 . a) (2 . b))) => cdr) (else 'none))", "b", ALL},
    {"(cond ((memv 3 '(1 2 3)) => car) (else 0))", "3", ALL},
    {"(cond (#f 1) (42))", "42", ALL}, {"(cond (7))", "7", ALL},
    {"(cond (#f 1))", "()", ALL}, {"(cond (#f 1) (#f 2) (else 9))", "9", ALL},
    {"(case 2 ((1) 'one) ((2) 'two) (else 'other))", "two", ALL},
    {"(if #f 'yes)", "()", ALL}, {"(if #t 'yes)", "yes", ALL},
    {"(when #t 1 2 3)", "3", ALL}, {"(when #f 1)", "()", ALL},
    {"(unless #f 9)", "9", ALL}, {"(unless #t 9)", "()", ALL},
    // binding forms
    {"(let ((x 1) (y 2)) (+ x y))", "3", ALL},
    {"(let* ((x 1) (y (+ x 1))) (* x y))", "2", ALL},
    {"(letrec ((e? (lambda (n) (if (= n 0) #t (o? (- n 1))))) (o? (lambda (n) (if (= n 0) #f (e? (- n 1)))))) (e? 10))", "#t", ALL},
    {"(letrec* ((a 1) (b (+ a 1))) (+ a b))", "3", ALL},
    {"(let-values (((a b) (values 1 2))) (+ a b))", "3", ALL},
    {"(let-values (((q r) (values 17 5)) ((s) (values 100))) (+ q r s))", "122", ALL},
    {"(let loop ((i 0) (s 0)) (if (< i 5) (loop (+ i 1) (+ s i)) s))", "10", ALL},
    {"(let loop ((i 0)) (if (< i 50) (loop (+ i 1)) i))", "50", ALL},
    // lambda / closures / apply
    {"((lambda (x) (* x x)) 6)", "36", ALL},
    {"((lambda (a b c) (+ a b c)) 1 2 3)", "6", ALL},
    {"(((lambda (x) (lambda (y) (+ x y))) 3) 4)", "7", ALL},
    {"(apply + '(1 2 3 4))", "10", ALL},
    // variadic / dotted lambda formals
    {"((lambda args args) 1 2 3)", "(1 2 3)", ALL},
    {"((lambda (a . rest) rest) 1 2 3)", "(2 3)", ALL},
    {"((lambda (a b . rest) (list a b rest)) 1 2 3 4)", "(1 2 (3 4))", ALL},
    {"((lambda args (apply + args)) 1 2 3 4)", "10", ALL},
    // multiple values
    {"(call-with-values (lambda () (values 1 2 3)) +)", "6", ALL},
    {"(call-with-values (lambda () (values 4 5)) (lambda (a b) (- a b)))", "-1", ALL},
    // list operations
    {"(car '(1 2 3))", "1", ALL}, {"(cdr '(1 2 3))", "(2 3)", ALL},
    {"(cons 1 '(2 3))", "(1 2 3)", ALL}, {"(cons 1 2)", "(1 . 2)", ALL},
    {"(list 1 2 3)", "(1 2 3)", ALL}, {"(length '(1 2 3 4))", "4", ALL},
    {"(append '(1 2) '(3 4))", "(1 2 3 4)", ALL}, {"(reverse '(1 2 3))", "(3 2 1)", ALL},
    {"(member 2 '(1 2 3))", "(2 3)", ALL}, {"(assoc 'b '((a 1) (b 2)))", "(b 2)", ALL},
    {"(cadr '(1 2 3))", "2", ALL}, {"(caddr '(1 2 3))", "3", ALL},
    {"(last-pair '(1 2 3))", "(3)", ALL}, {"(list-tail '(1 2 3 4) 2)", "(3 4)", ALL},
    // Closure applied from inside a primitive (map/filter/for-each) — exercises
    // re-entrant eval on every evaluator (was a toy-VM hang/segfault).
    {"(map (lambda (x) (* x x)) '(1 2 3))", "(1 4 9)", ALL},
    {"(map + '(1 2 3) '(10 20 30))", "(11 22 33)", ALL},
    {"(filter (lambda (x) (> x 2)) '(1 2 3 4))", "(3 4)", ALL},
    // quote / quasiquote
    {"(quote (a b c))", "(a b c)", ALL}, {"'(1 2 3)", "(1 2 3)", ALL},
    {"`(1 ,(+ 1 1) 3)", "(1 2 3)", ALL}, {"`(1 ,@(list 2 3) 4)", "(1 2 3 4)", ALL},
    // quasiquote into vectors, dotted tail, and nested-depth preservation
    {"(vector-ref `#(1 ,(+ 1 1) 3) 1)", "2", ALL},
    {"`(x . ,(+ 2 3))", "(x . 5)", ALL},
    {"(equal? `(1 `(2 ,(+ 1 2))) '(1 (quasiquote (2 (unquote (+ 1 2))))))", "#t", ALL},
    // self-evaluating vector literal (was a CoreIR lowering gap)
    {"(vector-ref #(10 20 30) 1)", "20", ALL},
    {"(vector? #(1 2 3))", "#t", ALL},
    // self-evaluating bytevector literal #u8(...) (reader + lowering gap)
    {"(bytevector-u8-ref #u8(10 20 30) 1)", "20", ALL},
    {"(bytevector-length #u8(1 2 3))", "3", ALL},
    {"(bytevector? #u8(0 255))", "#t", ALL},
    // define-record-type desugars to a tagged vector; works on every evaluator
    {"(begin (define-record-type pt (mk x y) pt? (x gx) (y gy)) (gx (mk 7 8)))", "7", ALL},
    {"(begin (define-record-type pt (mk x y) pt? (x gx) (y gy)) (pt? (mk 1 2)))", "#t", ALL},
    {"(begin (define-record-type c (mk v) c? (v gv sv!)) (let ((o (mk 1))) (sv! o 9) (gv o)))", "9", ALL},
    {"(begin (define-record-type pt (mk x) pt? (x gx)) (pt? 5))", "#f", ALL},
    // round ties-to-even; string->number radix; optional range/comparator args
    {"(round 2.5)", "2.0", ALL}, {"(round 3.5)", "4.0", ALL}, {"(round -2.5)", "-2.0", ALL},
    {"(string->number \"ff\" 16)", "255", ALL},
    {"(string->number \"101\" 2)", "5", ALL},
    {"(string-copy \"hello\" 1 3)", "el", ALL},
    {"(assoc 2.0 '((1 . a) (2 . b)) =)", "(2 . b)", ALL},
    {"(member 2.0 '(1 2 3) =)", "(2 3)", ALL},
    // floor/ceiling/truncate exactness, symbol=?, define-values, vector ranges
    {"(exact? (floor 7/2))", "#t", ALL}, {"(ceiling 7/2)", "4", ALL},
    {"(symbol=? 'a 'a 'a)", "#t", ALL}, {"(symbol=? 'a 'b)", "#f", ALL},
    {"(begin (define-values (a b) (values 3 4)) (+ a b))", "7", ALL},
    {"(begin (define-values (a . r) (values 1 2 3)) (length r))", "2", ALL},
    {"(vector-ref (let ((v (vector 1 2 3))) (vector-copy! v 0 #(9 8)) v) 1)", "8", ALL},
    // variadic comparison chains + min/max inexactness contagion
    {"(char<? #\\a #\\b #\\c)", "#t", ALL}, {"(char<? #\\a #\\c #\\b)", "#f", ALL},
    {"(string<? \"a\" \"b\" \"c\")", "#t", ALL},
    {"(boolean=? #t #t #t)", "#t", ALL},
    {"(exact? (min 1 2.0))", "#f", ALL}, {"(exact? (min 1 2 3))", "#t", ALL},
    // inexact reals print with a decimal point; expt is exact for exact args
    {"(number->string 2.0)", "2.0", ALL}, {"1.0", "1.0", ALL},
    {"(expt 2 10)", "1024", ALL}, {"(expt 2 -2)", "1/4", ALL},
    {"(exact? (expt 2 10))", "#t", ALL}, {"(exact? (expt 2.0 3))", "#f", ALL},
    // case with => (receiver applied to key); hash-table-update!
    {"(case 5 ((5) => (lambda (x) (* x x))) (else 0))", "25", ALL},
    {"(case 9 ((1 2) 'low) (else => (lambda (x) (+ x 1))))", "10", ALL},
    {"(let ((h (make-hash-table))) (hash-table-update!/default h 'x (lambda (v) (+ v 5)) 0) (hash-table-ref/default h 'x -1))", "5", ALL},
    // radix / exactness number prefixes
    {"#xff", "255", ALL}, {"#b101", "5", ALL}, {"#o17", "15", ALL},
    {"#e1.5", "3/2", ALL}, {"(exact? #i1/2)", "#f", ALL}, {"(+ #xff #b1)", "256", ALL},
    // with-output-to-string / call-with-output-string
    {"(with-output-to-string (lambda () (display \"ab\") (display 12)))", "ab12", ALL},
    {"(call-with-output-string (lambda (p) (display 99 p)))", "99", ALL},
    // named character literals (reader) + receive (Expander) — shared, all 4
    {"(char->integer #\\null)", "0", ALL}, {"(char->integer #\\escape)", "27", ALL},
    {"(char->integer #\\x41)", "65", ALL},
    {"(receive (a b) (values 1 2) (+ a b))", "3", ALL},
    {"(receive (a . r) (values 1 2 3) (length r))", "2", ALL},
    // inf/nan literals, negative-radix number->string, string mutation ops
    {"(nan? +nan.0)", "#t", ALL}, {"(infinite? +inf.0)", "#t", ALL},
    {"(number->string -255 16)", "-ff", ALL}, {"(number->string -10 2)", "-1010", ALL},
    {"(let ((s (make-string 4 #\\-))) (string-copy! s 1 \"ab\") s)", "-ab-", ALL},
    {"(let ((s (make-string 4 #\\-))) (string-fill! s #\\z 1 3) s)", "-zz-", ALL},
    // numeric literal syntax: exponent, leading-dot, signs (reader) — all 4
    {"1e3", "1000.0", ALL}, {"1.5e-2", "0.015", ALL}, {".5", "0.5", ALL},
    {"(exact .5)", "1/2", ALL}, {"(cons 1 .5)", "(1 . 0.5)", ALL},
    // list? terminates and returns #f on circular lists
    {"(let ((x (list 1 2))) (set-cdr! (cdr x) x) (list? x))", "#f", ALL},
    {"(list? '(1 2 . 3))", "#f", ALL}, {"(list? '(1 2 3))", "#t", ALL},
    // equal? terminates on circular structure (used to crash)
    {"(let ((x (list 1 2)) (y (list 1 2))) (set-cdr! (cdr x) x) (set-cdr! (cdr y) y) (equal? x y))", "#t", ALL},
    {"(let ((x (list 1 2))) (set-cdr! (cdr x) x) (equal? x '(1 2 1 2)))", "#f", ALL},
    // eval + interaction-environment
    {"(eval '(+ 1 2 3) (interaction-environment))", "6", ALL},
    {"((eval '(lambda (x) (* x x)) (interaction-environment)) 7)", "49", ALL},
    // syntax-rules hygiene: introduced binders don't capture user identifiers
    {"(begin (define-syntax myor (syntax-rules () ((_ a b) (let ((t a)) (if t t b))))) (define t 99) (myor #f t))", "99", ALL},
    {"(begin (define-syntax m (syntax-rules () ((_ e) (let ((x 10)) e)))) (define x 5) (m x))", "5", ALL},
    {"(begin (define-syntax sw (syntax-rules () ((_ a b) (let ((tmp a)) (set! a b) (set! b tmp))))) (define tmp 1) (define y 2) (sw tmp y) (list tmp y))", "(2 1)", ALL},
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
    // case-lambda — desugared in the shared Expander (arity dispatch), so every
    // evaluator runs the same core-form expansion (was tree-walk-only before).
    {"((case-lambda (() 0) ((x) x) ((x y) (+ x y))) 3 4)", "7", ALL},
    {"((case-lambda ((x) (list 'one x)) ((x y) (list 'two x y))) 9)", "(one 9)", ALL},
    {"((case-lambda ((a b) 'two) ((a b . r) (list 'rest r))) 1 2 3 4)", "(rest (3 4))", ALL},
    {"((case-lambda ((x) x) (args (length args))) 1 2 3)", "3", ALL},
    // Escape continuations — supported by all three (the tree-walk's only mode).
    {"(+ 1 (call/cc (lambda (k) (k 10))))", "11", ALL},
    {"(* 2 (call/cc (lambda (k) (+ 1 (k 5)))))", "10", ALL},
    {"(+ 100 (begin (call/cc (lambda (k) (k 5))) 999))", "1099", ALL},
    // begin (and other complex forms) in operand position — exercises the CoreIR
    // terminal-atom fix (a chain ending in Return(literal) must bind correctly).
    {"(+ 100 (begin 1 999))", "1099", ALL},
    {"(* 2 (begin 10 20))", "40", ALL},
    {"(list 1 (begin 2 'sym))", "(1 sym)", ALL},
    // `if` in operand position — CoreIR threads the call's continuation into both
    // branches of a tail `if`; the other three handle it natively. Cross-check.
    {"(+ 1 (if #f 10 20))", "21", ALL},
    {"(+ (if (> 3 1) 100 0) (if (< 3 1) 5 7))", "107", ALL},
    {"(list (if #t 1 2) (if #f 3 4))", "(1 4)", ALL},
    {"((lambda (x) (* 2 x)) (if #f 1 7))", "14", ALL},
    {"(cons (if #t 'a 'b) (if #f '() (list 9)))", "(a 9)", ALL},
    {"(begin (+ 1 2) (call/cc (lambda (k) (k 42))) (+ 3 4))", "7", ALL},
    {"((lambda (ls) (call/cc (lambda (break) (let loop ((l ls) (acc 1)) (cond ((null? l) acc) ((= (car l) 0) (break 0)) (else (loop (cdr l) (* acc (car l))))))))) '(1 2 0 4))", "0", ALL},
};

// Re-entrant continuations (re-invoking a stored k to resume a computation) are
// supported only by the two real VMs; the tree-walk is escape-only and errors on
// re-invocation, so these cannot use it as the oracle — compare bytecode vs toy
// VM against a known value, and to each other.
const Case kReentrantCorpus[] = {
    // counter driven by re-invoking a stored continuation
    {"(begin (define k0 #f) (define count 0) (define start (call/cc (lambda (k) (set! k0 k) 0))) (set! count (+ count 1)) (if (< start 3) (k0 (+ start 1)) count))", "4", B | V},
    {"(begin (define ks '()) (define i (call/cc (lambda (k) (set! ks k) 1))) (if (<= i 3) (ks (+ i 1)) i))", "4", B | V},
    // sum 1..5 by re-invoking the captured continuation
    {"(begin (define k #f) (define i 0) (define acc 0) (define j (call/cc (lambda (c) (set! k c) 1))) (set! acc (+ acc j)) (if (< j 5) (k (+ j 1)) acc))", "15", B | V},
    // re-entry back INTO a dynamic-wind extent runs before (entering) then after
    // (leaving) again -> (before after before after). R7RS 6.10; both VMs agree.
    {"(begin (define log '()) (define k0 #f) (define n 0) (dynamic-wind (lambda () (set! log (cons 'before log))) (lambda () (call/cc (lambda (k) (set! k0 k)))) (lambda () (set! log (cons 'after log)))) (set! n (+ n 1)) (if (< n 2) (k0 #f) (reverse log)))", "(before after before after)", B | V},
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

TEST_F(DifferentialTest, ReentrantContinuations) {
    // Each masked (B/V) evaluator must produce the expected value — so they also
    // agree with each other. The tree-walk is excluded (escape-only; it errors on
    // re-invocation), which is itself verified below.
    for (const auto& c : kReentrantCorpus) {
        for (const auto& ev : kEvs) {
            if (!(c.mask & ev.bit)) continue;
            std::string got;
            try {
                got = run_in(ev.type, c.code);
            } catch (const std::exception& e) {
                ADD_FAILURE() << ev.name << " threw on  " << c.code << " : " << e.what();
                continue;
            }
            EXPECT_EQ(got, c.expect) << ev.name << " diverged on: " << c.code;
        }
        // The tree-walk only supports escape continuations: re-invoking a stored
        // continuation is rejected (eval_string reports the error and returns
        // null -> "<null>"), so it must NOT produce the re-entrant result.
        EXPECT_NE(run_in(EvaluatorType::LISP, c.code), c.expect)
            << "tree-walk unexpectedly produced the re-entrant result: " << c.code;
    }
}
