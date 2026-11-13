;; Example: Complete application using library system
;; Demonstrates library creation, importing, and usage

;; ============================================================================
;; Step 1: Define utility library
;; ============================================================================

(define-library (myapp utils)
  (import (scheme base))
  (export repeat-string indent-lines format-list)

  (begin
    (define (repeat-string str n)
      "Repeat string n times"
      (if (<= n 0)
          ""
          (string-append str (repeat-string str (- n 1)))))

    (define (indent-lines text indent)
      "Indent each line by indent spaces"
      (string-append (repeat-string " " indent) text))

    (define (format-list items)
      "Format list as bulleted list"
      (if (null? items)
          ""
          (string-append
            "  * " (car items) "\n"
            (format-list (cdr items)))))))

;; ============================================================================
;; Step 2: Define data processing library
;; ============================================================================

(define-library (myapp data)
  (import (scheme base))
  (export process-numbers analyze-list summarize)

  (begin
    (define (process-numbers nums)
      "Calculate statistics on list of numbers"
      (let ((total (apply + nums))
            (count (length nums)))
        (list
          (cons 'total total)
          (cons 'count count)
          (cons 'average (/ total count))
          (cons 'min (apply min nums))
          (cons 'max (apply max nums)))))

    (define (analyze-list lst)
      "Analyze list composition"
      (let ((numbers (filter number? lst))
            (strings (filter string? lst))
            (symbols (filter symbol? lst)))
        (list
          (cons 'numbers (length numbers))
          (cons 'strings (length strings))
          (cons 'symbols (length symbols)))))

    (define (summarize data)
      "Create text summary of data"
      (define (format-pair p)
        (string-append
          "  "
          (symbol->string (car p))
          ": "
          (number->string (cdr p))
          "\n"))
      (apply string-append (map format-pair data)))))

;; ============================================================================
;; Step 3: Define main application
;; ============================================================================

(define-library (myapp main)
  (import (scheme base)
          (myapp utils)
          (myapp data))
  (export run-demo)

  (begin
    (define (run-demo)
      "Run demonstration of library features"

      (display "\n=== Zombie Scheme Library Demo ===\n\n")

      ;; Demo 1: String utilities
      (display "Demo 1: String Utilities\n")
      (display (repeat-string "=" 30))
      (newline)

      (display (indent-lines "This is indented text" 4))
      (newline)

      (display (format-list '("Apples" "Bananas" "Cherries")))
      (newline)

      ;; Demo 2: Number processing
      (display "\nDemo 2: Number Processing\n")
      (display (repeat-string "=" 30))
      (newline)

      (define test-numbers '(10 20 30 40 50))
      (display "Input: ")
      (write test-numbers)
      (newline)
      (display "Statistics:\n")
      (display (summarize (process-numbers test-numbers)))
      (newline)

      ;; Demo 3: List analysis
      (display "\nDemo 3: List Analysis\n")
      (display (repeat-string "=" 30))
      (newline)

      (define test-list '(1 2 "hello" world 3.14 "world" foo))
      (display "Input: ")
      (write test-list)
      (newline)
      (display "Composition:\n")
      (display (summarize (analyze-list test-list)))
      (newline)

      (display "\n=== Demo Complete ===\n"))))

;; ============================================================================
;; Step 4: Import and run
;; ============================================================================

(display "\nLoading application libraries...\n")

;; Import main application
(import (myapp main))

;; Run the demo
(run-demo)
