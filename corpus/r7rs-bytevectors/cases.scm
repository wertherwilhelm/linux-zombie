;;; cases.scm - bytevector literal + operation probes (batch 5).

(define corpus-cases
  (list
    ;; #u8(...) literals are self-evaluating
    (list "bv-literal"      '#u8(1 2 255)                                  #u8(1 2 255))
    (list "bv-literal-pred" '(bytevector? #u8(1 2 3))                      #t)
    (list "bv-empty"        '(bytevector-length #u8())                     0)
    (list "bv-length"       '(bytevector-length #u8(10 20 30))            3)
    (list "bv-u8-ref"       '(bytevector-u8-ref #u8(10 20 30) 1)          20)
    (list "bv-byte-max"     '(bytevector-u8-ref #u8(0 255) 1)            255)
    ;; equal? compares bytevectors element-wise
    (list "bv-equal-ctor"   '(equal? #u8(1 2 3) (bytevector 1 2 3))        #t)
    (list "bv-equal-diff"   '(equal? #u8(1 2 3) #u8(1 2 4))               #f)
    (list "bv-equal-len"    '(equal? #u8(1 2) #u8(1 2 3))                 #f)
    ;; literals coexist with mutation of freshly constructed bytevectors
    (list "bv-mutate"       '(let ((b (bytevector 1 2 3)))
                              (bytevector-u8-set! b 0 9)
                              (list (bytevector-u8-ref b 0) (equal? b #u8(9 2 3))))
          '(9 #t))
    ;; nested inside a list, read back out
    (list "bv-in-list"      '(bytevector-length (car (list #u8(5 6 7) 'x))) 3)))
