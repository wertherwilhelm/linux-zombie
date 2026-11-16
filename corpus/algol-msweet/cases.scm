;;; cases.scm - corpus cases for the ALGOL / M-expression reader.
;;;
;;; Format: corpus-cases is a list of (label form expected) triples.
;;;   - label:    a string naming the case (shown on failure)
;;;   - form:     a QUOTED Scheme expression; the runner evaluates it
;;;   - expected: a datum; the runner compares printed(result) to printed(expected)
;;; The runner loads source.scm first, then this file, then walks corpus-cases.
;;; A form that raises an error is reported as a failure (this is how the corpus
;;; discovers missing/under-implemented areas).

(define corpus-cases
  (list
    ;; --- parsing: M-expression + ALGOL sugar -> Scheme ---
    (list "parse-fn-def"
          '(car (algol-msweet-read "square[x] = {x * x}"))
          '(define (square x) (* x x)))

    (list "parse-infix-compare"
          '(car (algol-msweet-read "lt[a, b] = {a < b}"))
          '(define (lt a b) (< a b)))

    (list "parse-value-def"
          '(car (algol-msweet-read "pi = 3"))
          '(define pi 3))

    (list "parse-nested-mexpr-call"
          '(car (algol-msweet-read "g[n] = {fact[{n - 1}] * n}"))
          '(define (g n) (* (fact (- n 1)) n)))

    (list "parse-multi-arg"
          '(car (algol-msweet-read "add3[a, b, c] = {a + {b + c}}"))
          '(define (add3 a b c) (+ a (+ b c))))

    ;; --- end-to-end: parse, evaluate the forms, then call the result ---
    (list "eval-square"
          '(begin (for-each eval (algol-msweet-read "square[x] = {x * x}"))
                  (square 9))
          81)

    (list "eval-add"
          '(begin (for-each eval (algol-msweet-read "add[a, b] = {a + b}"))
                  (add 30 12))
          42)

    (list "eval-nested-call"
          '(begin (for-each eval (algol-msweet-read "dbl[x] = {x + x}"))
                  (for-each eval (algol-msweet-read "quad[x] = {dbl[x] + dbl[x]}"))
                  (quad 10))
          40)))
