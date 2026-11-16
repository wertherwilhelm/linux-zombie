;;; cases.scm - broad R7RS feature probes (no source.scm; builtins only).
;;; Purpose: validate conformance AND discover missing/under-implemented areas.
;;; Each is a (label form expected) triple; a form that errors is reported.

(define corpus-cases
  (list
    ;; higher-order list procedures
    (list "map"         '(map (lambda (x) (* x x)) '(1 2 3))        '(1 4 9))
    (list "map-2lists"  '(map + '(1 2 3) '(10 20 30))              '(11 22 33))
    (list "for-each"    '(let ((s 0)) (for-each (lambda (x) (set! s (+ s x))) '(1 2 3 4)) s) 10)
    (list "filter"      '(filter odd? '(1 2 3 4 5 6))              '(1 3 5))
    (list "fold-left"   '(fold-left + 0 '(1 2 3 4 5))              15)
    (list "fold-right"  '(fold-right cons '() '(1 2 3))           '(1 2 3))
    (list "reduce"      '(reduce + 0 '(1 2 3 4))                   10  'gap)
    (list "assoc"       '(assoc 'b '((a . 1) (b . 2)))            '(b . 2))
    (list "assq"        '(assq 'c '((a . 1) (c . 3)))             '(c . 3))
    (list "member"      '(member 3 '(1 2 3 4))                    '(3 4))
    (list "list-sort"   '(list-sort < '(3 1 2))                   '(1 2 3) 'gap)
    (list "sort"        '(sort '(3 1 2) <)                        '(1 2 3) 'gap)

    ;; vectors
    (list "vector-map"  '(vector-map (lambda (x) (* x 2)) (vector 1 2 3)) #(2 4 6))
    (list "vector-for-each" '(let ((s 0)) (vector-for-each (lambda (x) (set! s (+ s x))) (vector 1 2 3)) s) 6)
    (list "vector-fill" '(let ((v (make-vector 3 0))) (vector-fill! v 7) v) #(7 7 7))
    (list "vector->list" '(vector->list (vector 1 2 3))          '(1 2 3))

    ;; strings & chars
    (list "string-map"  '(string-map char-upcase "abc")          "ABC")
    (list "string-for-each" '(let ((n 0)) (string-for-each (lambda (c) (set! n (+ n 1))) "hi") n) 2)
    (list "string-split" '(string-split "a,b,c" #\,)            '("a" "b" "c") 'gap)
    (list "string-upcase" '(string-upcase "hello")              "HELLO")
    (list "string-contains" '(string-contains "hello world" "wor") 6 'gap)
    (list "string->list" '(string->list "abc")                  '(#\a #\b #\c))
    (list "char-upcase" '(char-upcase #\a)                       #\A)

    ;; control / binding forms
    (list "named-let"   '(let loop ((i 0) (s 0)) (if (< i 5) (loop (+ i 1) (+ s i)) s)) 10)
    (list "do-loop"     '(do ((i 0 (+ i 1)) (s 0 (+ s i))) ((= i 5) s)) 10)
    (list "case"        '(case 2 ((1) 'one) ((2) 'two) (else 'x))  'two)
    (list "when"        '(when (> 3 2) 'yes)                       'yes)
    (list "let-values"  '(let-values (((a b) (values 1 2))) (+ a b)) 3)
    (list "call/cc"     '(+ 1 (call/cc (lambda (k) (k 10))))       11)
    (list "dynamic-wind" '(let ((o '())) (dynamic-wind (lambda () (set! o (cons 'b o))) (lambda () (set! o (cons 'd o))) (lambda () (set! o (cons 'a o)))) (reverse o)) '(b d a))
    (list "apply"       '(apply + 1 2 '(3 4))                     10)
    (list "values-list" '(call-with-values (lambda () (values 1 2 3)) list) '(1 2 3))

    ;; quasiquote
    (list "quasiquote"  '`(1 ,(+ 1 1) ,@(list 3 4) 5)            '(1 2 3 4 5))

    ;; numeric tower
    (list "rational"    '(+ 1/2 1/3)                              5/6)
    (list "exact->inexact" '(exact->inexact 1/4)                 0.25)
    (list "expt"        '(expt 2 10)                              1024)
    (list "gcd-lcm"     '(list (gcd 12 18) (lcm 4 6))           '(6 12))
    ;; floor/ and exact-integer-sqrt should return TWO values; currently floor/
    ;; yields only the quotient and exact-integer-sqrt a single list — gaps.
    (list "floor/"      '(call-with-values (lambda () (floor/ 7 2)) list) '(3 1) 'gap)
    (list "number->string-radix" '(number->string 255 16)        "ff")
    (list "exact-sqrt"  '(call-with-values (lambda () (exact-integer-sqrt 17)) list) '(4 1) 'gap)

    ;; association / hash tables (R7RS/SRFI-69 - likely a gap)
    (list "make-hash"   '(let ((h (make-hash-table))) (hash-table-set! h 'k 42) (hash-table-ref h 'k #f)) 42 'gap)

    ;; tail recursion depth
    (list "deep-tail"   '(let loop ((i 0)) (if (< i 100000) (loop (+ i 1)) i)) 100000)))
