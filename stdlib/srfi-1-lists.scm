;; SRFI-1 style list library
;; Common list utilities not in (scheme base)

(define-library (srfi 1)
  (import (scheme base))

  (export
    ;; Constructors
    iota circular-list
    ;; Predicates
    proper-list? circular-list? dotted-list?
    ;; Selectors
    first second third fourth fifth
    take drop take-right drop-right
    last last-pair
    ;; Miscellaneous
    concatenate zip unzip1 unzip2
    count
    ;; Folding
    fold fold-right
    reduce reduce-right
    ;; Filtering
    filter partition remove
    ;; Searching
    find find-tail any every
    ;; Association lists
    alist-cons alist-delete)

  (begin
    ;; ========================================================================
    ;; Constructors
    ;; ========================================================================

    (define (iota count . rest)
      "Generate list of numbers: (iota 5) => (0 1 2 3 4)"
      (let ((start (if (null? rest) 0 (car rest)))
            (step (if (or (null? rest) (null? (cdr rest))) 1 (cadr rest))))
        (let loop ((n 0) (result '()))
          (if (>= n count)
              (reverse result)
              (loop (+ n 1) (cons (+ start (* n step)) result))))))

    ;; ========================================================================
    ;; Predicates
    ;; ========================================================================

    (define (proper-list? x)
      "Test if x is a proper list (null-terminated)"
      (cond
        ((null? x) #t)
        ((pair? x) (proper-list? (cdr x)))
        (else #f)))

    (define (circular-list? x)
      "Test if x is a circular list (uses tortoise-hare algorithm)"
      (let loop ((slow x) (fast x))
        (cond
          ((null? fast) #f)
          ((not (pair? fast)) #f)
          ((null? (cdr fast)) #f)
          ((not (pair? (cdr fast))) #f)
          ((eq? slow fast) #t)
          ((eq? slow (cdr fast)) #t)
          (else (loop (cdr slow) (cddr fast))))))

    (define (dotted-list? x)
      "Test if x is a dotted list (improper list)"
      (and (pair? x)
           (not (proper-list? x))
           (not (circular-list? x))))

    ;; ========================================================================
    ;; Selectors
    ;; ========================================================================

    (define first car)
    (define second cadr)
    (define third caddr)
    (define fourth cadddr)

    (define (fifth x)
      (car (cddddr x)))

    (define (take lst k)
      "Return first k elements of lst"
      (let loop ((lst lst) (k k) (result '()))
        (if (or (zero? k) (null? lst))
            (reverse result)
            (loop (cdr lst) (- k 1) (cons (car lst) result)))))

    (define (drop lst k)
      "Return lst with first k elements removed"
      (if (zero? k)
          lst
          (drop (cdr lst) (- k 1))))

    (define (take-right lst k)
      "Return last k elements"
      (drop lst (- (length lst) k)))

    (define (drop-right lst k)
      "Return all but last k elements"
      (take lst (- (length lst) k)))

    (define (last lst)
      "Return last element of list"
      (if (null? (cdr lst))
          (car lst)
          (last (cdr lst))))

    (define (last-pair lst)
      "Return last pair of list"
      (if (null? (cdr lst))
          lst
          (last-pair (cdr lst))))

    ;; ========================================================================
    ;; Miscellaneous
    ;; ========================================================================

    (define (concatenate lists)
      "Concatenate a list of lists"
      (apply append lists))

    (define (zip list1 list2 . more)
      "Zip lists together: (zip '(1 2 3) '(a b c)) => ((1 a) (2 b) (3 c))"
      (if (or (null? list1) (null? list2))
          '()
          (cons (list (car list1) (car list2))
                (apply zip (cdr list1) (cdr list2) more))))

    (define (unzip1 lst)
      "Extract first elements"
      (map car lst))

    (define (unzip2 lst)
      "Extract second elements"
      (map cadr lst))

    (define (count pred lst)
      "Count elements satisfying predicate"
      (fold (lambda (x acc) (if (pred x) (+ acc 1) acc)) 0 lst))

    ;; ========================================================================
    ;; Folding
    ;; ========================================================================

    (define (fold proc init lst)
      "Left fold: (fold + 0 '(1 2 3)) => 6"
      (if (null? lst)
          init
          (fold proc (proc (car lst) init) (cdr lst))))

    (define (fold-right proc init lst)
      "Right fold"
      (if (null? lst)
          init
          (proc (car lst) (fold-right proc init (cdr lst)))))

    (define (reduce proc init lst)
      "Reduce with initial value (alias for fold)"
      (fold proc init lst))

    (define (reduce-right proc init lst)
      "Right reduce (alias for fold-right)"
      (fold-right proc init lst))

    ;; ========================================================================
    ;; Filtering
    ;; ========================================================================

    (define (filter pred lst)
      "Keep only elements satisfying predicate"
      (fold-right (lambda (x acc) (if (pred x) (cons x acc) acc)) '() lst))

    (define (partition pred lst)
      "Split list into (matching non-matching)"
      (let loop ((lst lst) (yes '()) (no '()))
        (cond
          ((null? lst) (list (reverse yes) (reverse no)))
          ((pred (car lst)) (loop (cdr lst) (cons (car lst) yes) no))
          (else (loop (cdr lst) yes (cons (car lst) no))))))

    (define (remove pred lst)
      "Remove elements satisfying predicate"
      (filter (lambda (x) (not (pred x))) lst))

    ;; ========================================================================
    ;; Searching
    ;; ========================================================================

    (define (find pred lst)
      "Find first element satisfying predicate"
      (cond
        ((null? lst) #f)
        ((pred (car lst)) (car lst))
        (else (find pred (cdr lst)))))

    (define (find-tail pred lst)
      "Find tail of list starting at first matching element"
      (cond
        ((null? lst) #f)
        ((pred (car lst)) lst)
        (else (find-tail pred (cdr lst)))))

    (define (any pred lst)
      "Test if any element satisfies predicate"
      (cond
        ((null? lst) #f)
        ((pred (car lst)) #t)
        (else (any pred (cdr lst)))))

    (define (every pred lst)
      "Test if all elements satisfy predicate"
      (cond
        ((null? lst) #t)
        ((pred (car lst)) (every pred (cdr lst)))
        (else #f)))

    ;; ========================================================================
    ;; Association lists
    ;; ========================================================================

    (define (alist-cons key value alist)
      "Add key-value pair to association list"
      (cons (cons key value) alist))

    (define (alist-delete key alist)
      "Remove key from association list"
      (filter (lambda (pair) (not (equal? (car pair) key))) alist))))
