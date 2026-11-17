;;; cases.scm - inf/nan literals, number->string negative radix, and the
;;; string mutation ops string-copy! / string-fill! ranges (batch 12).

(define corpus-cases
  (list
    ;; --- infinity / NaN external representation (reader + string->number) ---
    (list "read-+inf"    '(+ 0 +inf.0)               +inf.0)
    (list "read--inf"    '(- 0 +inf.0)               -inf.0)
    (list "nan-literal"  '(nan? +nan.0)              #t)
    (list "inf-pred"     '(infinite? +inf.0)         #t)
    (list "s->n-inf"     '(string->number "+inf.0")  +inf.0)
    (list "inf-roundtrip" '(string->number (number->string (/ 1.0 0.0))) +inf.0)

    ;; --- number->string in non-decimal radix handles negatives (R7RS) ---
    (list "n->s-neg-hex" '(number->string -255 16)   "-ff")
    (list "n->s-neg-bin" '(number->string -10 2)     "-1010")
    (list "n->s-pos-hex" '(number->string 255 16)    "ff")
    (list "n->s-zero"    '(number->string 0 16)      "0")
    (list "n->s-neg-oct" '(number->string -8 8)      "-10")

    ;; --- string-copy! (R7RS 6.7) ---
    (list "scopy!-basic" '(let ((s (make-string 5 #\-))) (string-copy! s 1 "abc") s) "-abc-")
    (list "scopy!-range" '(let ((s (make-string 5 #\-))) (string-copy! s 0 "abcde" 1 3) s) "bc---")
    (list "scopy!-overlap" '(let ((s (string-copy "abcde"))) (string-copy! s 1 s 0 3) s) "aabce")

    ;; --- string-fill! with optional range (R7RS 6.7) ---
    (list "sfill-range"  '(let ((s (make-string 5 #\-))) (string-fill! s #\z 1 3) s) "-zz--")
    (list "sfill-full"   '(let ((s (make-string 3 #\-))) (string-fill! s #\z) s) "zzz")))
