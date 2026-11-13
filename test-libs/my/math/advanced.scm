(define-library (my math advanced)
  (import (scheme base) (my math))
  (export power sum-of-squares hypotenuse)
  (begin
    (define (power base exp)
      (if (= exp 0)
          1
          (* base (power base (- exp 1)))))

    (define (sum-of-squares a b)
      (+ (square a) (square b)))

    (define (hypotenuse a b)
      (sqrt (sum-of-squares a b)))))
