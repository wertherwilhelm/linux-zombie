;; Zombie Scheme Startup File
;; ============================================================================
;; This file is loaded at REPL startup to provide convenient defaults
;; Users can customize by editing this file or creating ~/.zombie-scheme-init
;; ============================================================================

;; Display welcome message
(display "Loading Zombie Scheme standard library...\n")

;; ============================================================================
;; Utility Functions (inline definitions)
;; ============================================================================

;; Quick helpers available immediately without imports

(define (identity x) x)

(define (square x) (* x x))

(define (cube x) (* x x x))

(define (average a b) (/ (+ a b) 2))

(define (sum lst)
  (if (null? lst)
      0
      (+ (car lst) (sum (cdr lst)))))

(define (product lst)
  (if (null? lst)
      1
      (* (car lst) (product (cdr lst)))))

(define (range start end)
  "Generate list from start to end-1"
  (if (>= start end)
      '()
      (cons start (range (+ start 1) end))))

(define (iota n)
  "Generate list of numbers from 0 to n-1"
  (range 0 n))

(define (take lst n)
  "Take first n elements of list"
  (if (or (null? lst) (<= n 0))
      '()
      (cons (car lst) (take (cdr lst) (- n 1)))))

(define (drop lst n)
  "Drop first n elements of list"
  (if (or (null? lst) (<= n 0))
      lst
      (drop (cdr lst) (- n 1))))

(define (filter pred lst)
  "Keep only elements satisfying predicate"
  (cond
    ((null? lst) '())
    ((pred (car lst)) (cons (car lst) (filter pred (cdr lst))))
    (else (filter pred (cdr lst)))))

(define (foldl proc init lst)
  "Left fold"
  (if (null? lst)
      init
      (foldl proc (proc (car lst) init) (cdr lst))))

(define (foldr proc init lst)
  "Right fold"
  (if (null? lst)
      init
      (proc (car lst) (foldr proc init (cdr lst)))))

(define (any pred lst)
  "Check if any element satisfies predicate"
  (cond
    ((null? lst) #f)
    ((pred (car lst)) #t)
    (else (any pred (cdr lst)))))

(define (all pred lst)
  "Check if all elements satisfy predicate"
  (cond
    ((null? lst) #t)
    ((pred (car lst)) (all pred (cdr lst)))
    (else #f)))

(define (zip lst1 lst2)
  "Zip two lists into list of pairs"
  (if (or (null? lst1) (null? lst2))
      '()
      (cons (list (car lst1) (car lst2))
            (zip (cdr lst1) (cdr lst2)))))

;; ============================================================================
;; Debugging Helpers
;; ============================================================================

(define (pp obj)
  "Pretty print object"
  (write obj)
  (newline)
  obj)

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

(define (time-it expr-thunk)
  "Time execution of thunk (requires current-second)"
  ;; Note: Requires (current-second) which may not be implemented yet
  (display "Timing not yet implemented\n")
  (expr-thunk))

;; ============================================================================
;; Math Utilities
;; ============================================================================

(define pi 3.141592653589793)

(define (even? n) (= (modulo n 2) 0))

(define (odd? n) (not (even? n)))

(define (abs x)
  (if (< x 0) (- x) x))

(define (max-list lst)
  "Maximum element in list"
  (foldl (lambda (x acc) (if (> x acc) x acc))
         (car lst)
         (cdr lst)))

(define (min-list lst)
  "Minimum element in list"
  (foldl (lambda (x acc) (if (< x acc) x acc))
         (car lst)
         (cdr lst)))

;; ============================================================================
;; String Utilities
;; ============================================================================

(define (string-join strings sep)
  "Join list of strings with separator"
  (cond
    ((null? strings) "")
    ((null? (cdr strings)) (car strings))
    (else
      (string-append (car strings) sep (string-join (cdr strings) sep)))))

;; ============================================================================
;; List Utilities
;; ============================================================================

(define (last lst)
  "Get last element of list"
  (if (null? (cdr lst))
      (car lst)
      (last (cdr lst))))

(define (flatten lst)
  "Flatten nested list structure"
  (cond
    ((null? lst) '())
    ((pair? (car lst))
     (append (flatten (car lst)) (flatten (cdr lst))))
    (else
     (cons (car lst) (flatten (cdr lst))))))

;; ============================================================================
;; Function Composition
;; ============================================================================

(define (compose f g)
  "Compose two functions: (compose f g) => λx. f(g(x))"
  (lambda (x) (f (g x))))

(define (curry f arg)
  "Curry a function with one argument"
  (lambda rest (apply f arg rest)))

;; ============================================================================
;; Done
;; ============================================================================

(display "Standard library loaded!\n")
(display "Type (help) for available functions\n")
(newline)

;; Help function
(define (help)
  (display "Zombie Scheme - Quick Reference\n")
  (display "================================\n\n")
  (display "Math: square, cube, average, sum, product, abs\n")
  (display "      even?, odd?, max-list, min-list\n\n")
  (display "Lists: range, iota, take, drop, filter, last, flatten\n")
  (display "       foldl, foldr, any, all, zip\n\n")
  (display "Strings: string-join\n\n")
  (display "Debug: pp, debug\n\n")
  (display "Functional: compose, curry, identity\n\n")
  (display "R7RS Libraries:\n")
  (display "  (import (scheme base))    - Core language\n")
  (display "  (import (scheme read))    - Reading\n")
  (display "  (import (scheme write))   - Writing\n")
  (display "  (import (scheme file))    - File I/O\n")
  (newline))
