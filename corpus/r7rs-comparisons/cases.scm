;;; cases.scm - variadic comparison chains and min/max contagion (batch 8).

(define corpus-cases
  (list
    ;; --- char comparison chains (R7RS 6.6) ---
    (list "char<-chain"   '(char<? #\a #\b #\c)         #t)
    (list "char<-break"   '(char<? #\a #\c #\b)         #f)
    (list "char=-chain"   '(char=? #\a #\a #\a)         #t)
    (list "char>=-chain"  '(char>=? #\c #\b #\b)        #t)
    (list "char-ci=chain" '(char-ci=? #\A #\a #\A)      #t)

    ;; --- string comparison chains (R7RS 6.7) ---
    (list "str<-chain"    '(string<? "a" "b" "c")       #t)
    (list "str<-break"    '(string<? "a" "c" "b")       #f)
    (list "str=-chain"    '(string=? "x" "x" "x")       #t)
    (list "str-ci=chain"  '(string-ci=? "AB" "ab" "Ab") #t)

    ;; --- boolean=? variadic (R7RS 6.3) ---
    (list "bool=-chain"   '(boolean=? #t #t #t)         #t)
    (list "bool=-break"   '(boolean=? #t #t #f)         #f)

    ;; --- min/max inexactness contagion (R7RS 6.2.6) ---
    (list "min-exact"     '(exact? (min 1 2 3))          #t)
    (list "min-contag"    '(exact? (min 1 2.0))          #f)
    (list "max-contag"    '(exact? (max 1.0 2 3))        #f)
    (list "min-value"     '(min 3 1 2)                    1)
    (list "max-value"     '(max 3 1 2)                    3)))
