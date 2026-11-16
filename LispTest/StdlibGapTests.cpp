/**
 * @file StdlibGapTests.cpp
 * @brief Regression tests for procedures added to close gaps the corpus found:
 *   reduce, sort, list-sort, string-split, string-contains, hash tables, and
 *   the multi-value floor/ / truncate/ / exact-integer-sqrt.
 */
#include "gtest/gtest.h"
#include "zombie_runtime_types.h"
#include "TestFixture.h"

class StdlibGapTest : public ZombieSchemeTest {};

TEST_F(StdlibGapTest, Reduce) {
    expectInt("(reduce + 0 '(1 2 3 4))", 10);
    expectInt("(reduce + 0 '())", 0);       // empty -> ridentity
    expectInt("(reduce max 0 '(3 1 4 1 5 9 2 6))", 9);
}

TEST_F(StdlibGapTest, SortAndListSort) {
    expectString("(symbol->string (car (sort '(c a b) (lambda (x y) (string<? (symbol->string x) (symbol->string y))))))", "a");
    eval("(define s (sort '(3 1 2 5 4) <))");
    expectString("(number->string (car s))", "1");
    expectString("(number->string (list-ref s 4))", "5");
    eval("(define t (list-sort < '(9 7 8)))");
    expectInt("(car t)", 7);
    expectInt("(caddr t)", 9);
    // stable + does not mutate input
    eval("(define orig '(3 1 2))");
    eval("(define sorted (sort orig <))");
    expectString("(number->string (car orig))", "3");  // original unchanged
}

TEST_F(StdlibGapTest, StringSplitAndContains) {
    expectInt("(length (string-split \"a,b,c\" #\\,))", 3);
    expectString("(car (string-split \"a,b,c\" #\\,))", "a");
    expectInt("(length (string-split \"nodelim\" #\\,))", 1);
    expectInt("(string-contains \"hello world\" \"wor\")", 6);
    expectBool("(string-contains \"abc\" \"xyz\")", false);
    expectInt("(string-contains \"abcabc\" \"abc\")", 0);
}

TEST_F(StdlibGapTest, MultiValueDivision) {
    expectInt("(call-with-values (lambda () (floor/ 7 2)) (lambda (q r) q))", 3);
    expectInt("(call-with-values (lambda () (floor/ 7 2)) (lambda (q r) r))", 1);
    expectInt("(call-with-values (lambda () (floor/ -7 2)) (lambda (q r) q))", -4);
    expectInt("(call-with-values (lambda () (truncate/ -7 2)) (lambda (q r) q))", -3);
    expectInt("(call-with-values (lambda () (exact-integer-sqrt 17)) (lambda (s r) s))", 4);
    expectInt("(call-with-values (lambda () (exact-integer-sqrt 17)) (lambda (s r) r))", 1);
}

// Hash tables (new HashTable type) — includes a GC stress to exercise tracing.
TEST_F(StdlibGapTest, HashTables) {
    expectBool("(hash-table? (make-hash-table))", true);
    expectBool("(hash-table? '(1 2 3))", false);

    eval("(define h (make-hash-table))");
    eval("(hash-table-set! h 'alpha 1)");
    eval("(hash-table-set! h 'beta 2)");
    expectInt("(hash-table-ref h 'alpha #f)", 1);
    expectInt("(hash-table-ref/default h 'beta 0)", 2);
    expectBool("(hash-table-ref/default h 'missing #f)", false);
    expectBool("(hash-table-contains? h 'beta)", true);
    expectInt("(hash-table-size h)", 2);

    // update existing key: value changes, size stays
    eval("(hash-table-set! h 'alpha 100)");
    expectInt("(hash-table-ref h 'alpha #f)", 100);
    expectInt("(hash-table-size h)", 2);

    // delete
    eval("(hash-table-delete! h 'alpha)");
    expectBool("(hash-table-contains? h 'alpha)", false);
    expectInt("(hash-table-size h)", 1);

    // equal?-keyed (string / list keys)
    eval("(define h2 (make-hash-table))");
    eval("(hash-table-set! h2 \"key\" 'sv)");
    eval("(hash-table-set! h2 '(1 2) 'lv)");
    expectSymbol("(hash-table-ref h2 \"key\" #f)", "sv");
    expectSymbol("(hash-table-ref h2 (list 1 2) #f)", "lv");  // equal?, not eq?

    // thunk default (SRFI-69 style)
    expectInt("(hash-table-ref h2 'nope (lambda () 77))", 77);
}

TEST_F(StdlibGapTest, HashTableSurvivesGC) {
    eval("(define hg (make-hash-table))");
    // insert many entries, allocating heavily to trigger collections
    eval("(let loop ((i 0)) (if (< i 200) (begin (hash-table-set! hg i (* i i)) (loop (+ i 1))) 'done))");
    expectInt("(hash-table-size hg)", 200);
    expectInt("(hash-table-ref hg 150 #f)", 22500);
}
