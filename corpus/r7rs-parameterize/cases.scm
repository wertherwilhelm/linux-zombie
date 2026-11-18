;;; cases.scm - R7RS parameterize / make-parameter (4.2.6). Desugared in the
;;; shared Expander to a (%parameterize (list p...) (list v...) thunk) call on
;;; the runtime primitive, so all four evaluators share one implementation
;;; (see expander.cpp expand_parameterize + primitive_parameters.cpp).

(define corpus-cases
  (list
    ;; basic dynamic binding + restore on normal exit
    (list "basic-bind-restore"
          '(let ((p (make-parameter 10)))
             (list (p) (parameterize ((p 20)) (p)) (p)))
          '(10 20 10))
    ;; converter runs on the new value (make-parameter's 2nd arg)
    (list "converter"
          '(let ((p (make-parameter 5 (lambda (x) (* x 2)))))
             (list (p) (parameterize ((p 7)) (p)) (p)))
          '(10 14 10))
    ;; multiple bindings in one parameterize
    (list "multiple-bindings"
          '(let ((a (make-parameter 1)) (b (make-parameter 2)))
             (parameterize ((a 10) (b 20)) (+ (a) (b))))
          30)
    ;; nested parameterize sees inner value, restores outer
    (list "nested"
          '(let ((p (make-parameter 1)))
             (parameterize ((p 2))
               (list (p) (parameterize ((p 3)) (p)) (p))))
          '(2 3 2))
    ;; escaping via a continuation still restores the old value
    (list "escape-restores"
          '(let ((p (make-parameter 10)))
             (call/cc (lambda (k) (parameterize ((p 99)) (k 'out))))
             (p))
          10)
    ;; multi-expression body returns the last expression
    (list "multi-expr-body"
          '(let ((p (make-parameter 1)))
             (parameterize ((p 5)) 'ignored (* (p) (p))))
          25)))
