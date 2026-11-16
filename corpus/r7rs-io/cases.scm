;;; cases.scm - ports and I/O probes (batch 3).

(define corpus-cases
  (list
    ;; --- string input ports: read datums / chars / lines ---
    (list "read-datum-list" '(read (open-input-string "(1 2 3)"))        '(1 2 3))
    (list "read-datum-num"  '(read (open-input-string "42"))             42)
    (list "read-datum-sym"  '(read (open-input-string "hello"))          'hello)
    (list "read-char-seq"   '(let ((p (open-input-string "ab")))
                               (list (read-char p) (read-char p) (eof-object? (read-char p))))
          '(#\a #\b #t))
    (list "peek-char"       '(let ((p (open-input-string "xy")))
                               (let ((a (peek-char p))) (list a (read-char p))))
          '(#\x #\x))
    (list "read-line-one"   '(read-line (open-input-string "first\nsecond")) "first")
    (list "read-line-eof"   '(eof-object? (read-line (open-input-string ""))) #t)
    (list "read-string"     '(read-string 3 (open-input-string "abcdef")) "abc")

    ;; --- string output ports ---
    (list "out-string-write" '(let ((p (open-output-string)))
                                (write '(1 "x" a) p) (get-output-string p))
          "(1 \"x\" a)")
    (list "out-string-display" '(let ((p (open-output-string)))
                                  (display "hi") (display 42 p) (get-output-string p))
          "42")
    ;; with-output-to-string needs current-output-port parameterization (not yet wired).
    (list "with-output-to-string"
          '(with-output-to-string (lambda () (display "abc") (display 123)))
          "abc123" 'gap)

    ;; --- write vs display (strings/chars quoting) ---
    (list "write-string-quoted" '(let ((p (open-output-string))) (write "hi" p) (get-output-string p)) "\"hi\"")
    (list "display-string-raw"  '(let ((p (open-output-string))) (display "hi" p) (get-output-string p)) "hi")
    (list "write-char-literal"  '(let ((p (open-output-string))) (write #\a p) (get-output-string p)) "#\\a")

    ;; --- round trips ---
    (list "read-write-roundtrip"
          '(read (open-input-string
                   (let ((p (open-output-string))) (write '(1 (2 . 3) #(4 5)) p) (get-output-string p))))
          '(1 (2 . 3) #(4 5)))
    (list "number-string-roundtrip" '(string->number (number->string 12345)) 12345)

    ;; --- port predicates ---
    (list "input-port?"  '(input-port? (open-input-string "x"))   #t)
    (list "output-port?" '(output-port? (open-output-string))     #t)
    (list "port-not"     '(input-port? 42)                        #f)))
