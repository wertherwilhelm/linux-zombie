;;; cases.scm - syntax-rules hygiene: a template's introduced binders must not
;;; capture user identifiers passed in as arguments (R7RS 4.3.2). Batch 16.

(define corpus-cases
  (list
    ;; introduced `t` in myor must not capture a user variable named t
    (list "myor-capture"
          '(begin (define-syntax myor (syntax-rules () ((_ a b) (let ((t a)) (if t t b)))))
                  (define t 99) (myor #f t))
          99)
    ;; introduced `x` in a let template must not shadow the argument's x
    (list "let-introduced"
          '(begin (define-syntax m (syntax-rules () ((_ e) (let ((x 10)) e))))
                  (define x 5) (m x))
          5)
    ;; swap! works even when the user variable is literally named tmp
    (list "swap!-tmp-collision"
          '(begin (define-syntax swap! (syntax-rules ()
                     ((_ a b) (let ((tmp a)) (set! a b) (set! b tmp)))))
                  (define tmp 1) (define y 2) (swap! tmp y) (list tmp y))
          '(2 1))
    ;; swap! with non-colliding names (regression)
    (list "swap!-normal"
          '(begin (define-syntax sw (syntax-rules ()
                     ((_ a b) (let ((tmp a)) (set! a b) (set! b tmp)))))
                  (define p 1) (define q 2) (sw p q) (list p q))
          '(2 1))
    ;; lambda-introduced binder must not capture argument
    (list "lambda-introduced"
          '(begin (define-syntax m2 (syntax-rules () ((_ e) ((lambda (tmp) (+ tmp e)) 100))))
                  (define tmp 7) (m2 tmp))
          107)
    ;; letrec-introduced binder
    (list "letrec-introduced"
          '(begin (define-syntax m3 (syntax-rules ()
                     ((_ e) (letrec ((helper (lambda () e))) (helper)))))
                  (define helper 42) (m3 helper))
          42)
    ;; named-let loop name introduced by the template
    (list "named-let-introduced"
          '(begin (define-syntax countup (syntax-rules ()
                     ((_ n) (let loop ((i 0) (acc 0))
                              (if (= i n) acc (loop (+ i 1) (+ acc i)))))))
                  (define loop 'user) (countup 4))
          6)
    ;; hygiene must not break ordinary ellipsis macros (regression)
    (list "ellipsis-regression"
          '(begin (define-syntax mylist (syntax-rules () ((_ x ...) (list x ...))))
                  (mylist 1 2 3 4))
          '(1 2 3 4))))
