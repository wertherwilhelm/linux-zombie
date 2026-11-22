;;; cases.scm - primitives CoreIR lowers as Prim nodes and the native JIT now
;;; emits directly (integer division, list ops, multiple values). Runs on the
;;; tree-walk reference and, via CorpusTest.NativeMatchesExpected, on --native.

(define corpus-cases
  (list
    (list "modulo-pos"      '(modulo 17 5)                  2)
    (list "modulo-neg"      '(modulo -17 5)                 3)
    (list "quotient"        '(quotient 17 5)                3)
    (list "remainder-neg"   '(remainder -17 5)             -2)
    (list "length"          '(length '(a b c d))            4)
    (list "length-empty"    '(length '())                   0)
    (list "list-ref"        '(list-ref '(10 20 30) 1)      20)
    (list "list?-proper"    '(list? '(1 2 3))              #t)
    (list "list?-improper"  '(list? (cons 1 2))           #f)
    (list "list?-atom"      '(list? 7)                     #f)
    (list "values-many"     '(call-with-values
                               (lambda () (values 1 2 3))
                               (lambda (a b c) (+ a b c)))  6)
    (list "values-one"      '(+ 1 (values 41))            42)
    (list "nested-arith"    '(modulo (+ (* 6 7) 1) 10)      3)))
