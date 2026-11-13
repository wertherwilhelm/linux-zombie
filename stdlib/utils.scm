;; Common utility functions
;; Useful helpers for everyday programming

(define-library (zombie utils)
  (import (scheme base))

  (export
    ;; Debugging
    debug pp
    ;; Functional programming
    identity const compose curry partial
    ;; Conditionals
    when-let if-let
    ;; Math
    square cube clamp between?
    ;; Lists
    range repeat cycle
    ;; Higher order
    times do-times
    ;; Strings
    string-join string-split
    ;; Misc
    void noop)

  (begin
    ;; ========================================================================
    ;; Debugging
    ;; ========================================================================

    (define (debug msg . values)
      "Print debug message with values"
      (display "DEBUG: ")
      (display msg)
      (for-each (lambda (v)
                  (display " ")
                  (write v))
                values)
      (newline)
      (if (null? values) msg (car values)))

    (define (pp obj)
      "Pretty print an object and return it"
      (write obj)
      (newline)
      obj)

    ;; ========================================================================
    ;; Functional Programming
    ;; ========================================================================

    (define (identity x)
      "Return argument unchanged"
      x)

    (define (const x)
      "Return a function that always returns x"
      (lambda args x))

    (define (compose . fns)
      "Compose functions: ((compose f g h) x) => (f (g (h x)))"
      (if (null? fns)
          identity
          (let ((fn (car fns))
                (rest (apply compose (cdr fns))))
            (lambda (x) (fn (rest x))))))

    (define (curry f . args1)
      "Partially apply function with some arguments"
      (lambda args2
        (apply f (append args1 args2))))

    (define partial curry)

    ;; ========================================================================
    ;; Conditionals
    ;; ========================================================================

    ;; Note: These would need macro support for proper implementation
    ;; Providing function versions for now

    ;; ========================================================================
    ;; Math
    ;; ========================================================================

    (define (square x)
      "Return x squared"
      (* x x))

    (define (cube x)
      "Return x cubed"
      (* x x x))

    (define (clamp x min-val max-val)
      "Clamp x to range [min-val, max-val]"
      (cond
        ((< x min-val) min-val)
        ((> x max-val) max-val)
        (else x)))

    (define (between? x min-val max-val)
      "Test if x is in range [min-val, max-val]"
      (and (>= x min-val) (<= x max-val)))

    ;; ========================================================================
    ;; Lists
    ;; ========================================================================

    (define (range start end . rest)
      "Generate range of numbers: (range 0 5) => (0 1 2 3 4)"
      (let ((step (if (null? rest) 1 (car rest))))
        (let loop ((i start) (result '()))
          (if (>= i end)
              (reverse result)
              (loop (+ i step) (cons i result))))))

    (define (repeat n x)
      "Create list of n copies of x"
      (let loop ((n n) (result '()))
        (if (<= n 0)
            result
            (loop (- n 1) (cons x result)))))

    (define (cycle n lst)
      "Repeat list n times"
      (if (<= n 0)
          '()
          (append lst (cycle (- n 1) lst))))

    ;; ========================================================================
    ;; Higher Order
    ;; ========================================================================

    (define (times n proc)
      "Call proc n times, return list of results"
      (let loop ((i 0) (result '()))
        (if (>= i n)
            (reverse result)
            (loop (+ i 1) (cons (proc i) result)))))

    (define (do-times n proc)
      "Call proc n times for side effects"
      (let loop ((i 0))
        (when (< i n)
          (proc i)
          (loop (+ i 1)))))

    ;; ========================================================================
    ;; Strings
    ;; ========================================================================

    (define (string-join strings separator)
      "Join list of strings with separator"
      (cond
        ((null? strings) "")
        ((null? (cdr strings)) (car strings))
        (else
          (string-append (car strings)
                        separator
                        (string-join (cdr strings) separator)))))

    (define (string-split str delimiter)
      "Split string by delimiter (simple version - single char)"
      ;; Note: This is a simplified version
      ;; A full implementation would need more sophisticated parsing
      (let ((chars (string->list str))
            (delim-char (if (char? delimiter)
                           delimiter
                           (string-ref delimiter 0))))
        (let loop ((chars chars) (current '()) (result '()))
          (cond
            ((null? chars)
             (reverse (cons (list->string (reverse current)) result)))
            ((char=? (car chars) delim-char)
             (loop (cdr chars)
                   '()
                   (cons (list->string (reverse current)) result)))
            (else
             (loop (cdr chars)
                   (cons (car chars) current)
                   result))))))

    ;; ========================================================================
    ;; Miscellaneous
    ;; ========================================================================

    (define (void . args)
      "Return unspecified value (for side effects)"
      (if #f #f))

    (define noop void)))
