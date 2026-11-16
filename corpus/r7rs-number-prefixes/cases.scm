;;; cases.scm - radix and exactness number prefixes (batch 11, R7RS 7.1.1).

(define corpus-cases
  (list
    ;; --- radix prefixes ---
    (list "hex"        '#xff              255)
    (list "hex-upper"  '#xFF              255)
    (list "binary"     '#b101               5)
    (list "octal"      '#o17               15)
    (list "decimal"    '#d42               42)
    (list "hex-neg"    '#x-ff            -255)
    (list "hex-rat"    '#x10/2              8)

    ;; --- exactness prefixes ---
    (list "exact-dec"  '#e1.5             3/2)
    (list "exact?-e"   '(exact? #e1.5)     #t)
    (list "inexact-rat" '#i1/2            0.5)
    (list "exact?-i"   '(exact? #i1/2)     #f)
    (list "exact?-hex" '(exact? #xff)      #t)

    ;; --- combined prefixes (either order) ---
    (list "e-then-x"   '#e#xa              10)
    (list "x-then-e"   '#x#ea              10)
    (list "i-then-x"   '(exact? #i#xa)     #f)

    ;; --- used inside expressions ---
    (list "in-plus"    '(+ #xff #b1)      256)
    (list "in-list"    '(list #o10 #xa #b10) '(8 10 2))))
