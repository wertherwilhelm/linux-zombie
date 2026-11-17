;;; cases.scm - named chars, receive, and SRFI-1/SRFI-69 prelude helpers
;;; (batch 11). Reader + Expander parts work on all evaluators; the SRFI
;;; helpers come from the Scheme prelude.

(define corpus-cases
  (list
    ;; --- named character literals (reader, R7RS 7.1.1) ---
    (list "char-null"   '(char->integer #\null)       0)
    (list "char-alarm"  '(char->integer #\alarm)      7)
    (list "char-bs"     '(char->integer #\backspace)  8)
    (list "char-delete" '(char->integer #\delete)   127)
    (list "char-escape" '(char->integer #\escape)    27)
    (list "char-hex"    '(char->integer #\x41)        65)
    (list "char-plain"  '(char->integer #\a)          97)

    ;; --- receive (SRFI-8, Expander) ---
    (list "receive-2"   '(receive (a b) (values 1 2) (+ a b))        3)
    (list "receive-rest" '(receive (a . r) (values 1 2 3) (list a r)) '(1 (2 3)))

    ;; --- SRFI-1 (prelude) ---
    (list "fold"        '(fold cons '() '(1 2 3))               '(3 2 1))
    (list "filter-map"  '(filter-map (lambda (x) (if (even? x) (* x x) #f)) '(1 2 3 4)) '(4 16))
    (list "concatenate" '(concatenate '((1 2) (3 4) (5)))      '(1 2 3 4 5))
    (list "take-while"  '(take-while even? '(2 4 1 3))         '(2 4))
    (list "drop-while"  '(drop-while even? '(2 4 1 3))         '(1 3))
    (list "unfold"      '(unfold (lambda (x) (> x 3)) (lambda (x) (* x x)) (lambda (x) (+ x 1)) 1) '(1 4 9))

    ;; --- SRFI-69 hash helpers (prelude) ---
    (list "ht-walk"     '(let ((h (make-hash-table)) (s 0))
                           (hash-table-set! h 'a 1) (hash-table-set! h 'b 2)
                           (hash-table-walk h (lambda (k v) (set! s (+ s v)))) s) 3)
    (list "ht-copy"     '(let ((h (make-hash-table)))
                           (hash-table-set! h 'a 1)
                           (hash-table-ref/default (hash-table-copy h) 'a #f)) 1)
    (list "alist->ht"   '(hash-table-ref/default (alist->hash-table '((a . 1) (b . 2))) 'b #f) 2)
    (list "ht-fold"     '(let ((h (make-hash-table)))
                           (hash-table-set! h 'a 10) (hash-table-set! h 'b 20)
                           (hash-table-fold h (lambda (k v acc) (+ v acc)) 0)) 30)))
