;;; cases.scm - floor/ceiling/truncate exactness, symbol=?, define-values,
;;; and optional-range vector operations (batch 7).

(define corpus-cases
  (list
    ;; --- floor/ceiling/truncate preserve exactness (R7RS 6.2.6) ---
    (list "floor-rat"      '(floor 7/2)              3)
    (list "floor-exact"    '(exact? (floor 7/2))     #t)
    (list "ceiling-rat"    '(ceiling 7/2)            4)
    (list "truncate-rat"   '(truncate -7/2)         -3)
    (list "floor-inexact"  '(exact? (floor 2.5))     #f)
    (list "floor-int"      '(floor 5)                5)

    ;; --- symbol=? (R7RS 6.5) ---
    (list "symbol=?-2"     '(symbol=? 'a 'a)         #t)
    (list "symbol=?-3"     '(symbol=? 'a 'a 'a)      #t)
    (list "symbol=?-no"    '(symbol=? 'a 'b)         #f)

    ;; --- define-values (R7RS 5.3.3) ---
    (list "dv-fixed"       '(begin (define-values (a b) (values 1 2)) (list a b)) '(1 2))
    (list "dv-rest"        '(begin (define-values (a . r) (values 1 2 3)) (list a r)) '(1 (2 3)))
    (list "dv-all"         '(begin (define-values x (values 1 2 3)) x)              '(1 2 3))
    (list "dv-single"      '(begin (define-values (only) (values 42)) only)          42)

    ;; --- vector->list / vector-fill! ranges, vector-copy! (R7RS 6.8) ---
    (list "v->l-range"     '(vector->list #(1 2 3 4 5) 1 3)                 '(2 3))
    (list "v->l-start"     '(vector->list #(1 2 3 4 5) 2)                   '(3 4 5))
    (list "vfill-range"    '(let ((v (vector 1 2 3 4))) (vector-fill! v 0 1 3) v) #(1 0 0 4))
    (list "vcopy!-basic"   '(let ((v (vector 1 2 3 4 5))) (vector-copy! v 1 #(9 8 7)) v) #(1 9 8 7 5))
    (list "vcopy!-range"   '(let ((v (vector 1 2 3 4 5))) (vector-copy! v 0 #(9 8 7) 1 3) v) #(8 7 3 4 5))
    (list "vcopy!-overlap" '(let ((v (vector 1 2 3 4 5))) (vector-copy! v 2 v 0 3) v) #(1 2 1 2 3))))
