;;; cases.scm - macros, derived syntax, and control-form probes (batch 3).

(define corpus-cases
  (list
    ;; --- syntax-rules macros ---
    (list "define-syntax-simple"
          '(begin (define-syntax my-if
                    (syntax-rules () ((_ c t e) (cond (c t) (else e)))))
                  (my-if #t 'yes 'no))
          'yes)
    (list "syntax-rules-ellipsis"
          '(begin (define-syntax mylist (syntax-rules () ((_ x ...) (list x ...))))
                  (mylist 1 2 3 4))
          '(1 2 3 4))
    (list "syntax-rules-swap"
          '(begin (define-syntax swap!
                    (syntax-rules () ((_ a b) (let ((tmp a)) (set! a b) (set! b tmp)))))
                  (define x 1) (define y 2) (swap! x y) (list x y))
          '(2 1))
    (list "let-syntax"
          '(let-syntax ((dbl (syntax-rules () ((_ x) (* 2 x))))) (dbl 21))
          42)
    (list "letrec-syntax"
          '(letrec-syntax ((dbl (syntax-rules () ((_ x) (* 2 x))))) (dbl 50))
          100)

    ;; --- case-lambda ---
    (list "case-lambda"
          '((case-lambda ((x) x) ((x y) (+ x y))) 3 4)
          7)
    (list "case-lambda-1"
          '((case-lambda ((x) (* x x)) ((x y) (+ x y))) 6)
          36)

    ;; --- derived control forms ---
    (list "do-sum"      '(do ((i 0 (+ i 1)) (s 0 (+ s i))) ((= i 5) s))   10)
    (list "case-else"   '(case 9 ((1 2) 'low) (else 'high))               'high)
    (list "cond-arrow"  '(cond ((assv 2 '((1 . a) (2 . b))) => cdr) (else 'none)) 'b)
    (list "when-list"   '(when #t 1 2 3)                                   3)
    (list "unless-list" '(unless #f 'ran)                                  'ran)
    (list "and-value"   '(and 1 2 3)                                       3)
    (list "or-value"    '(or #f 7 8)                                       7)
    (list "named-let"   '(let loop ((i 0) (acc '())) (if (= i 3) (reverse acc) (loop (+ i 1) (cons i acc)))) '(0 1 2))
    (list "let*-seq"    '(let* ((a 1) (b (+ a 1)) (c (+ b 1))) (list a b c)) '(1 2 3))
    (list "letrec-mut"  '(letrec ((e? (lambda (n) (if (= n 0) #t (o? (- n 1))))) (o? (lambda (n) (if (= n 0) #f (e? (- n 1)))))) (e? 10)) #t)

    ;; --- quasiquote ---
    (list "qq-unquote"  '`(a ,(+ 1 2) c)                                   '(a 3 c))
    (list "qq-splice"   '`(1 ,@(list 2 3) 4)                               '(1 2 3 4))
    ;; quasiquote does not yet descend into vector literals (unquote left verbatim).
    (list "qq-vector"   '`#(1 ,(+ 1 1) 3)                                  #(1 2 3) 'gap)
    ;; nested quasiquote mishandles depth: inner unquote evaluated at wrong level.
    (list "qq-nested"   '`(1 `(2 ,(+ 1 2)))                                '(1 (quasiquote (2 (unquote (+ 1 2))))) 'gap)))
