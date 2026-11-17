;;; cases.scm - list? must handle circular lists (R7RS 6.4): return #f and
;;; terminate. Previously a cyclic list hung the interpreter (batch 14).

(define corpus-cases
  (list
    (list "list?-proper"  '(list? '(1 2 3))        #t)
    (list "list?-nil"     '(list? '())             #t)
    (list "list?-dotted"  '(list? '(1 2 . 3))      #f)
    (list "list?-atom"    '(list? 5)               #f)
    (list "list?-pair"    '(list? (cons 1 2))      #f)
    ;; circular lists: must return #f without looping forever
    (list "list?-cyclic-1"
          '(let ((x (list 1))) (set-cdr! x x) (list? x))
          #f)
    (list "list?-cyclic-3"
          '(let ((x (list 1 2 3))) (set-cdr! (cddr x) x) (list? x))
          #f)
    (list "list?-cyclic-tail"
          '(let ((tailp (list 3 4)))
             (set-cdr! (cdr tailp) tailp)
             (list? (cons 1 (cons 2 tailp))))
          #f)

    ;; equal? must terminate on circular structure too (it used to crash with a
    ;; stack overflow). Co-inductive result: equal cyclic structures are equal.
    (list "equal?-cyclic-same"
          '(let ((x (list 1 2)) (y (list 1 2)))
             (set-cdr! (cdr x) x) (set-cdr! (cdr y) y) (equal? x y))
          #t)
    (list "equal?-cyclic-diff"
          '(let ((x (list 1 2)) (y (list 1 9)))
             (set-cdr! (cdr x) x) (set-cdr! (cdr y) y) (equal? x y))
          #f)
    (list "equal?-cyclic-vs-finite"
          '(let ((x (list 1 2))) (set-cdr! (cdr x) x) (equal? x '(1 2 1 2)))
          #f)
    (list "equal?-finite-still" '(equal? '(1 (2 3) #(4 5)) '(1 (2 3) #(4 5))) #t)))
