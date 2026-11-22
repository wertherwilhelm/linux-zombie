;;; Compute digits of PI using a straightforward implementation of
;;; the Chudnovsky brothers algorithm; see
;;; http://www.craig-wood.com/nick/articles/pi-chudnovsky/

(import (scheme base) (scheme cxr) (scheme read) (scheme write) (scheme time))

(define ch-A 13591409)
(define ch-B 545140134)
(define ch-C 640320)
(define ch-C^3 (expt 640320 3))
(define ch-D 12)

(define (ch-split a b)
  (if (= 1 (- b a))
      (let ((g (* (- (* 6 b) 5) (- (* 2 b) 1) (- (* 6 b) 1))))
        (list g
              (quotient (* ch-C^3 (expt b 3)) 24)
              (* (expt -1 b) g (+ (* b ch-B) ch-A))))
      (let* ((mid (quotient (+ a b) 2))
             (gpq1 (ch-split a mid))
             (gpq2 (ch-split mid b))
             (g1 (car gpq1)) (p1 (cadr gpq1)) (q1 (caddr gpq1))
             (g2 (car gpq2)) (p2 (cadr gpq2)) (q2 (caddr gpq2)))
        (list (* g1 g2)
              (* p1 p2)
              (+ (* q1 p2) (* q2 g1))))))

(define (integer-sqrt x)
  (call-with-values
   (lambda ()
     (exact-integer-sqrt x))
   (lambda (q r)
     q)))

(define (pi digits)
  (let* ((num-terms (exact (floor (+ 2 (/ digits 14.181647462)))))
         (sqrt-C (integer-sqrt (* ch-C (expt 100 digits)))))
    (let* ((gpq (ch-split 0 num-terms))
           (g (car gpq)) (p (cadr gpq)) (q (caddr gpq)))
      (quotient (* p ch-C sqrt-C) (* ch-D (+ q (* p ch-A)))))))

(define (pies n m s)
  (if (< m n)
      '()
      (cons (pi n)
            (pies (+ n s) m s))))

(define (run-benchmark)
  (let* ((count (read))
         (input1 (read))
         (input2 (read))
         (input3 (read))
         (output (read))
         (s4 (number->string count))
         (s3 (number->string input3))
         (s2 (number->string input2))
         (s1 (number->string input1))
         (name "chudnovsky"))
    (run-r7rs-benchmark
     (string-append name ":" s1 ":" s2 ":" s3 ":" s4)
     count
     (lambda ()
       (pies (hide count input1)
             (hide count input2)
             (hide count input3)))
     (lambda (result) (equal? result output)))))


;;; The following code is appended to all benchmarks.

;;; Given an integer and an object, returns the object
;;; without making it too easy for compilers to tell
;;; the object will be returned.

(define (hide r x)
  (call-with-values
   (lambda ()
     (values (vector values (lambda (x) x))
             (if (< r 100) 0 1)))
   (lambda (v i)
     ((vector-ref v i) x))))

;;; Given the name of a benchmark,
;;; the number of times it should be executed,
;;; a thunk that runs the benchmark once,
;;; and a unary predicate that is true of the
;;; correct results the thunk may return,
;;; runs the benchmark for the number of specified iterations.

(define (run-r7rs-benchmark name count thunk ok?)

  ;; Rounds to thousandths.
  (define (rounded x)
    (/ (round (* 1000 x)) 1000))

  (display "Running ")
  (display name)
  (newline)
  (flush-output-port (current-output-port))
  (let* ((j/s (jiffies-per-second))
         (t0 (current-second))
         (j0 (current-jiffy)))
    (let loop ((i 0)
               (result #f))
      (cond ((< i count)
             (loop (+ i 1) (thunk)))
            ((ok? result)
             (let* ((t1 (current-second))
                    (j1 (current-jiffy))
                    (jifs (- j1 j0))
                    (secs (inexact (/ jifs j/s)))
                    (secs2 (rounded (- t1 t0))))
               (display "Elapsed time: ")
               (write secs)
               (display " seconds (")
               (write secs2)
               (display ") for ")
               (display name)
               (newline)
               (display "+!CSVLINE!+")
               (display (this-scheme-implementation-name))
               (display ",")
               (display name)
               (display ",")
               (display secs)
               (newline)
               (flush-output-port (current-output-port)))
             0)
            (else
             (display "ERROR: returned incorrect result: ")
             (write result)
             (newline)
             (display "+!CSVLINE!+")
             (display (this-scheme-implementation-name))
             (display ",")
             (display name)
             (display ",")
             (display "INCORRECT")
             (newline)
             (flush-output-port (current-output-port))
             0)))))

(define (this-scheme-implementation-name) "ZombieScheme-lisp")

(run-benchmark)
