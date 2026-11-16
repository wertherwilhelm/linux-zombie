;;; cases.scm - case with => and hash-table-update! (batch 10).

(define corpus-cases
  (list
    ;; --- case with => applies the receiver to the key (R7RS 4.2.1) ---
    (list "case-arrow"      '(case 5 ((5) => (lambda (x) (* x x))) (else 'no))  25)
    (list "case-arrow-else" '(case 9 ((1 2) 'low) (else => (lambda (x) (+ x 1)))) 10)
    (list "case-arrow-miss" '(case 3 ((5) => (lambda (x) x)) (else 'other))    'other)
    (list "case-plain"      '(case 2 ((1 2 3) 'yes) (else 'no))                'yes)
    (list "case-multi-data" '(case 7 ((1 2) 'a) ((6 7 8) => (lambda (k) (* k 2))) (else 'z)) 14)

    ;; --- hash-table-update! / hash-table-update!/default (SRFI-69) ---
    (list "ht-update-exist"
          '(let ((h (make-hash-table)))
             (hash-table-set! h 'a 1)
             (hash-table-update! h 'a (lambda (v) (+ v 10)) (lambda () 0))
             (hash-table-ref/default h 'a #f))
          11)
    (list "ht-update-absent"
          '(let ((h (make-hash-table)))
             (hash-table-update! h 'c (lambda (v) (+ v 1)) (lambda () 100))
             (hash-table-ref/default h 'c #f))
          101)
    (list "ht-update-default"
          '(let ((h (make-hash-table)))
             (hash-table-update!/default h 'x (lambda (v) (+ v 5)) 0)
             (hash-table-update!/default h 'x (lambda (v) (+ v 5)) 0)
             (hash-table-ref/default h 'x -1))
          10)))
