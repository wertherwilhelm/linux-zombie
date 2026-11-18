;;; cases.scm - R7RS case-lambda (4.2.9). Desugared in the shared Expander to a
;;; single variadic lambda that dispatches on argument count, so all four
;;; evaluators share one implementation (see expander.cpp expand_case_lambda).

(define corpus-cases
  (list
    ;; fixed-arity dispatch
    (list "fixed-arity"
          '(let ((f (case-lambda (() 'none) ((x) (list 'one x)) ((x y) (list 'two x y)))))
             (list (f) (f 1) (f 1 2)))
          '(none (one 1) (two 1 2)))
    ;; rest clause (dotted formals) matches 2+ args via >=
    (list "rest-clause"
          '(let ((f (case-lambda ((a) (list 'solo a)) ((a b . rest) (list 'many a b rest)))))
             (list (f 9) (f 1 2) (f 1 2 3 4)))
          '((solo 9) (many 1 2 ()) (many 1 2 (3 4))))
    ;; bare-symbol (fully variadic) clause collects everything
    (list "variadic-clause"
          '(let ((f (case-lambda ((x) x) (args (length args)))))
             (list (f 'a) (f) (f 1 2 3)))
          '(a 0 3))
    ;; a classic: plus with 0/1/2/n arities
    (list "plus-all-arities"
          '(let ((plus (case-lambda (() 0) ((x) x) ((x y) (+ x y)) ((x y . z) (apply + x y z)))))
             (list (plus) (plus 5) (plus 3 4) (plus 1 2 3 4)))
          '(0 5 7 10))
    ;; clauses are tried in order; first match wins
    (list "first-match-wins"
          '(let ((f (case-lambda ((x) 'unary) ((x . rest) 'variadic))))
             (list (f 1) (f 1 2)))
          '(unary variadic))
    ;; case-lambda value is a first-class procedure usable with map
    (list "as-higher-order"
          '(map (case-lambda ((x) (* x x))) '(1 2 3 4))
          '(1 4 9 16))))
