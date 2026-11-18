;;; cases.scm - R7RS guard (4.2.7). Desugared in the shared Expander to
;;; (%guard (lambda (var) (cond clause ... [else (raise var)])) (lambda () body))
;;; on the %guard runtime primitive, which owns the C++ RAII (GuardHandler
;;; install, payload recovery incl. C++ primitive errors + nested re-raise). All
;;; four evaluators share it (see expander.cpp expand_guard +
;;; primitive_exceptions.cpp GUARDIMPL).

(define corpus-cases
  (list
    ;; catch-all clause binds the raised condition
    (list "catch-all"
          '(guard (e (#t (list 'caught e))) (raise 'boom))
          '(caught boom))
    ;; clause selection by predicate, with else
    (list "predicate-then-else"
          '(list (guard (e ((symbol? e) 'sym) (else 'other)) (raise 'x))
                 (guard (e ((string? e) 'str) (else 'other)) (raise 'x)))
          '(sym other))
    ;; body completes normally -> its value, handler untouched
    (list "no-exception"
          '(guard (e (#t 'caught)) (+ 1 2))
          3)
    ;; guard usable in operand position
    (list "in-operand-position"
          '(+ 1 (guard (e (#t 100)) (raise 'x)))
          101)
    ;; no matching clause + no else -> re-raise to the outer guard
    (list "reraise-to-outer"
          '(guard (e ((eq? e 'a) 'outer-a))
             (guard (e2 ((eq? e2 'b) 'inner-b)) (raise 'a)))
          'outer-a)
    ;; outer guard matches the re-raised number
    (list "reraise-number-to-outer"
          '(guard (e ((number? e) (+ e 100)))
             (guard (e2 ((string? e2) 'inner)) (raise 7)))
          107)
    ;; a C++ primitive error (divide by zero) is a catchable condition
    (list "catches-primitive-error"
          '(guard (e (else 'caught-div)) (/ 10 0))
          'caught-div)
    ;; error object: message + irritants are accessible in the handler
    (list "error-object-accessors"
          '(guard (e (#t (list (error-object? e)
                               (error-object-message e)
                               (error-object-irritants e))))
             (error "bad" 1 2))
          '(#t "bad" (1 2)))))
