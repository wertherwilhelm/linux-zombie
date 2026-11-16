;;; cases.scm - external representation of inexact reals, and expt exactness
;;; (batch 9).

(define corpus-cases
  (list
    ;; --- inexact reals print with a decimal point (R7RS 6.2.6 / 7.1.1) ---
    (list "write-1.0"      '1.0                              1.0)
    (list "n->s-2.0"       '(number->string 2.0)            "2.0")
    (list "n->s-2"         '(number->string 2)              "2")
    (list "n->s-1/2"       '(number->string 1/2)            "1/2")
    (list "n->s-2.5"       '(number->string 2.5)            "2.5")
    (list "ei->str"        '(number->string (exact->inexact 2)) "2.0")
    (list "n->s-radix"     '(number->string 255 16)         "ff")
    ;; infinities and NaN
    (list "pos-inf"        '(number->string (/ 1.0 0.0))    "+inf.0")
    (list "neg-inf"        '(number->string (/ -1.0 0.0))   "-inf.0")
    (list "nan"            '(number->string (/ 0.0 0.0))    "+nan.0")

    ;; --- expt preserves exactness (R7RS 6.2.6) ---
    (list "expt-exact"     '(expt 2 10)                      1024)
    (list "expt-exact?"    '(exact? (expt 2 10))             #t)
    (list "expt-neg"       '(expt 2 -2)                      1/4)
    (list "expt-zero"      '(expt 5 0)                        1)
    (list "expt-inexact-b" '(exact? (expt 2.0 3))            #f)
    (list "expt-inexact-e" '(exact? (expt 2 3.0))            #f)
    (list "expt-value"     '(expt 3 4)                        81)))
