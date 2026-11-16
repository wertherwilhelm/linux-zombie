;;; cases.scm - numeric rounding, string->number radix, and the optional
;;; range/comparator arguments of string and list operations (batch 6).

(define corpus-cases
  (list
    ;; --- round: ties to even (R7RS 6.2.6) ---
    (list "round-0.5"   '(round 0.5)    0)
    (list "round-1.5"   '(round 1.5)    2)
    (list "round-2.5"   '(round 2.5)    2)
    (list "round-3.5"   '(round 3.5)    4)
    (list "round-neg"   '(round -2.5)  -2)
    (list "round-rat"   '(round 7/2)    4)   ; exact rational -> exact 4
    (list "round-rat2"  '(round 5/2)    2)
    (list "round-int"   '(round 42)    42)
    (list "round-exact" '(exact? (round 7/2)) #t)

    ;; --- string->number with explicit radix (R7RS 6.2.6) ---
    (list "s->n-dec"    '(string->number "42")       42)
    (list "s->n-r10"    '(string->number "42" 10)    42)
    (list "s->n-r16"    '(string->number "ff" 16)   255)
    (list "s->n-r2"     '(string->number "101" 2)     5)
    (list "s->n-r8"     '(string->number "17" 8)     15)
    (list "s->n-exact"  '(exact? (string->number "42")) #t)
    (list "s->n-float"  '(string->number "4.5")     4.5)
    (list "s->n-bad"    '(string->number "xyz")      #f)

    ;; --- string-copy / string->list with start/end (R7RS 6.7) ---
    (list "scopy-full"  '(string-copy "hello")           "hello")
    (list "scopy-range" '(string-copy "hello" 1 3)       "el")
    (list "scopy-start" '(string-copy "hello" 2)         "llo")
    (list "s->l-range"  '(string->list "hello" 1 3)      '(#\e #\l))
    (list "s->l-start"  '(string->list "hello" 2)        '(#\l #\l #\o))

    ;; --- assoc / member with a user comparator (R7RS 6.3/6.4) ---
    (list "assoc-cmp"   '(assoc 2.0 '((1 . a) (2 . b) (3 . c)) =)  '(2 . b))
    (list "assoc-miss"  '(assoc 9.0 '((1 . a) (2 . b)) =)          #f)
    (list "member-cmp"  '(member 2.0 '(1 2 3) =)                   '(2 3))
    (list "assoc-plain" '(assoc 2 '((1 . a) (2 . b)))             '(2 . b))))
