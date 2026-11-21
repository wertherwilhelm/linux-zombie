;;; cases.scm - R7RS promises / lazy evaluation (4.2.5). delay / delay-force are
;;; desugared in the shared Expander to %make-promise; force / make-promise /
;;; promise? live in the prelude (memoizing, with delay-force trampolining).
;;; All four evaluators share one implementation.

(define corpus-cases
  (list
    ;; (force (delay expr)) evaluates expr
    (list "force-delay" '(force (delay (+ 1 2))) 3)
    ;; make-promise wraps an already-computed value
    (list "make-promise" '(force (make-promise 10)) 10)
    ;; promise? predicate
    (list "promise-predicate"
          '(list (promise? (delay 1)) (promise? 5) (promise? (make-promise 1)))
          '(#t #f #t))
    ;; forcing a non-promise returns it unchanged (R7RS)
    (list "force-non-promise" '(force 42) 42)
    ;; a delayed expression runs at most once; re-forcing is memoized
    (list "memoized-once"
          '(let ((n 0))
             (let ((p (delay (begin (set! n (+ n 1)) n))))
               (list (force p) (force p) n)))
          '(1 1 1))
    ;; lazy infinite stream: only the forced prefix is computed
    (list "lazy-stream"
          '(begin
             (define (ints-from k) (delay (cons k (ints-from (+ k 1)))))
             (define (stream-head s n)
               (if (= n 0) '()
                   (let ((pr (force s)))
                     (cons (car pr) (stream-head (cdr pr) (- n 1))))))
             (stream-head (ints-from 1) 5))
          '(1 2 3 4 5))
    ;; delay-force gives proper space: a long tail-recursive force terminates
    (list "delay-force-tail-loop"
          '(begin
             (define (loop n)
               (delay-force (if (= n 0) (delay 'done) (loop (- n 1)))))
             (force (loop 2000)))
          'done)))
