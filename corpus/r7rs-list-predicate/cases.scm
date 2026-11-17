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
          #f)))
