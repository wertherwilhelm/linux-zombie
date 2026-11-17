;;; cases.scm - numeric literal syntax: exponents, leading-dot, signs (batch 13,
;;; R7RS 7.1.1). All in the shared reader, so all evaluators.

(define corpus-cases
  (list
    ;; --- exponent notation (inexact) ---
    (list "exp-plain"    '1e3           1000.0)
    (list "exp-frac"     '1.5e2         150.0)
    (list "exp-neg"      '1e-3          0.001)
    (list "exp-cap-sign" '3E+2          300.0)
    (list "exp-avogadro" '(> 6.02e23 6e23) #t)
    (list "exp-inexact?" '(exact? 1e3)  #f)

    ;; --- leading-dot decimals ---
    (list "dot-5"        '.5            0.5)
    (list "dot-neg"      '-.25        -0.25)
    (list "dot-pos"      '+.5           0.5)
    (list "dot-exact"    '(exact .5)    1/2)
    (list "dot-in-list"  '(list .5 .25) '(0.5 0.25))

    ;; --- trailing dot / leading sign ---
    (list "trailing-dot" '3.            3.0)
    (list "plus-int"     '+5            5)

    ;; --- dotted-pair notation is NOT confused by numeric dots ---
    (list "dotted-pair"  '(quote (1 2 . 3))   '(1 2 . 3))
    (list "cons-dot-num" '(cons 1 .5)         '(1 . 0.5))

    ;; --- symbols with dots/arrows still parse as symbols ---
    (list "arrow-sym"    '(begin (define -> 7) ->)   7)
    (list "ellipsis-ok"  '(begin (define-syntax m (syntax-rules () ((_ x ...) (list x ...)))) (m 1 2 3)) '(1 2 3))))
