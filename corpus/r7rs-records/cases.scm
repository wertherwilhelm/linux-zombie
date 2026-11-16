;;; cases.scm - define-record-type probes (batch 4).

(define corpus-cases
  (list
    ;; basic construction, predicate, accessors
    (list "record-basic"
          '(begin (define-record-type point (make-point x y) point?
                    (x point-x) (y point-y))
                  (let ((p (make-point 3 4)))
                    (list (point? p) (point-x p) (point-y p))))
          '(#t 3 4))
    ;; mutation via modifier
    (list "record-mutate"
          '(begin (define-record-type cell (make-cell v) cell? (v cell-v set-cell-v!))
                  (let ((c (make-cell 1))) (set-cell-v! c 99) (cell-v c)))
          99)
    ;; predicate rejects non-records and foreign vectors
    (list "record-pred-reject"
          '(begin (define-record-type box (make-box a) box? (a box-a))
                  (list (box? 5) (box? "x") (box? '(1 2)) (box? (vector 'box 1))))
          '(#f #f #f #f))
    ;; two distinct record types are not confused by the predicate
    (list "record-distinct-types"
          '(begin (define-record-type ta (mk-a x) a? (x a-x))
                  (define-record-type tb (mk-b y) b? (y b-y))
                  (list (a? (mk-a 1)) (a? (mk-b 2)) (b? (mk-b 2)) (b? (mk-a 1))))
          '(#t #f #t #f))
    ;; constructor need not list every field (unset slots default to #f)
    (list "record-partial-ctor"
          '(begin (define-record-type rec (make-rec a) rec? (a rec-a) (b rec-b set-rec-b!))
                  (let ((r (make-rec 10)))
                    (list (rec-b r) (begin (set-rec-b! r 20) (rec-b r)) (rec-a r))))
          '(#f 20 10))
    ;; records survive being stored/retrieved through other structures
    (list "record-in-list"
          '(begin (define-record-type kv (make-kv k v) kv? (k kv-k) (v kv-v))
                  (let ((items (list (make-kv 'a 1) (make-kv 'b 2))))
                    (map kv-v items)))
          '(1 2))))
