(define-library (my math)
  (import (scheme base))
  (export square cube factorial add-two)
  (begin
    (define (square x)
      (* x x))

    (define (cube x)
      (* x x x))

    (define (factorial n)
      (if (= n 0)
          1
          (* n (factorial (- n 1)))))

    (define (add-two x y)
      (+ x y))))
