;;; cases.scm - eval + interaction-environment (R7RS 6.12), batch 15.

(define corpus-cases
  (list
    (list "eval-arith"   '(eval '(+ 1 2 3) (interaction-environment))  6)
    (list "eval-nested"  '(eval '(* (+ 1 2) (- 10 6)) (interaction-environment)) 12)
    (list "eval-quote"   '(eval '(list 'a 'b) (interaction-environment)) '(a b))
    (list "eval-define"  '(begin (eval '(define corpus-zz 42) (interaction-environment))
                                  corpus-zz) 42)
    (list "eval-lambda"  '((eval '(lambda (x) (* x x)) (interaction-environment)) 7) 49)
    (list "eval-uses-global" '(begin (define corpus-base 100)
                                     (eval '(+ corpus-base 1) (interaction-environment))) 101)))
