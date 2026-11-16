;;; cases.scm - broader R7RS / SRFI feature probes (discovery batch 2).
;;; New-to-the-corpus areas; failures here reveal the next round of gaps.

(define corpus-cases
  (list
    ;; --- SRFI-1 style list ops ---
    (list "iota"            '(iota 5)                                 '(0 1 2 3 4))
    (list "iota-start"      '(iota 3 10)                              '(10 11 12))
    (list "take"            '(take '(1 2 3 4 5) 2)                    '(1 2))
    (list "drop"            '(drop '(1 2 3 4 5) 2)                    '(3 4 5))
    (list "last"            '(last '(1 2 3))                          3)
    (list "count"           '(count odd? '(1 2 3 4 5))               3)
    (list "delete"          '(delete 3 '(1 2 3 4 3))                 '(1 2 4))
    (list "remove"          '(remove odd? '(1 2 3 4 5))              '(2 4))
    (list "partition"       '(call-with-values (lambda () (partition odd? '(1 2 3 4))) list) '((1 3) (2 4)))
    (list "delete-dups"     '(delete-duplicates '(1 2 1 3 2))       '(1 2 3))
    (list "find"            '(find even? '(1 3 4 5))                  4)
    (list "any"             '(any even? '(1 3 5 6))                   #t)
    (list "every"           '(every odd? '(1 3 5))                    #t)
    (list "append-map"      '(append-map (lambda (x) (list x x)) '(1 2)) '(1 1 2 2))
    (list "list-index"      '(list-index even? '(1 3 5 6))           3)
    (list "list-tabulate"   '(list-tabulate 4 (lambda (i) (* i i))) '(0 1 4 9))

    ;; --- strings ---
    (list "string-reverse"  '(string-reverse "abc")                  "cba")
    (list "string-join"     '(string-join '("a" "b" "c") ",")        "a,b,c")
    (list "string-index"    '(string-index "hello" #\l)              2)
    (list "string-prefix?"  '(string-prefix? "he" "hello")           #t)
    (list "string-suffix?"  '(string-suffix? "lo" "hello")           #t)
    (list "string-trim"     '(string-trim "  hi  ")                  "hi")
    (list "string-pad-left" '(string-pad "42" 5)                     "   42")

    ;; --- chars ---
    (list "char-whitespace" '(char-whitespace? #\space)              #t)
    (list "char-alphabetic" '(char-alphabetic? #\A)                  #t)
    (list "char-numeric"    '(char-numeric? #\7)                     #t)
    (list "digit-value"     '(digit-value #\7)                       7)
    (list "char-ci=?"       '(char-ci=? #\A #\a)                     #t)

    ;; --- vectors ---
    (list "vector-copy"     '(vector-copy #(1 2 3))                  #(1 2 3))
    (list "vector-copy-range" '(vector-copy #(1 2 3 4 5) 1 3)        #(2 3))
    (list "vector-append"   '(vector-append #(1 2) #(3 4))           #(1 2 3 4))
    (list "vector-sort"     '(vector-sort < #(3 1 2))                #(1 2 3))

    ;; --- numbers ---
    (list "square"          '(square 7)                              49)
    (list "exact"           '(exact 3.0)                             3)
    (list "exact-integer?"  '(exact-integer? 5)                      #t)
    (list "numerator"       '(numerator 3/4)                         3)
    (list "denominator"     '(denominator 3/4)                       4)
    (list "nan?"            '(nan? (/ 0.0 0.0))                      #t)
    (list "finite?"         '(finite? 1.0)                           #t)
    (list "rationalize"     '(rationalize 1/3 1/100)                 1/3 'gap)

    ;; --- control / exceptions ---
    (list "delay-force"     '(force (delay (+ 1 2)))                 3)
    (list "guard-catch"     '(guard (e (#t 'caught)) (raise 'oops))  'caught)
    (list "error-object"    '(guard (e ((error-object? e) (error-object-message e))) (error "boom")) "boom")
    (list "with-exc"        '(call/cc (lambda (k) (with-exception-handler (lambda (e) (k 'handled)) (lambda () (raise 'x))))) 'handled)
    (list "parameterize"    '(let ((p (make-parameter 10))) (list (p) (parameterize ((p 20)) (p)) (p))) '(10 20 10))
    (list "dynamic-wind-val" '(dynamic-wind (lambda () #f) (lambda () 42) (lambda () #f)) 42)

    ;; --- assoc / misc ---
    (list "assv-num"        '(assv 2 '((1 . a) (2 . b)))             '(2 . b))
    (list "list-copy"       '(list-copy '(1 2 3))                    '(1 2 3))
    (list "boolean=?"       '(boolean=? #t #t)                       #t)))
