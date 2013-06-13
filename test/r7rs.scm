;;;
;;; R7RS
;;;

(import (foment base))

;;
;; ---- expressions ----
;;

;; quote

(must-equal a (quote a))
(must-equal #(a b c) (quote #(a b c)))
(must-equal (+ 1 2) (quote (+ 1 2)))

(must-equal a 'a)
(must-equal #(a b c) '#(a b c))
(must-equal () '())
(must-equal (+ 1 2) '(+ 1 2))
(must-equal (quote a) '(quote a))
(must-equal (quote a) ''a)

(must-equal 145932 '145932)
(must-equal 145932 145932)
(must-equal "abc" '"abc")
(must-equal "abc" "abc")
;(must-equal # '#)
;(must-equal # #)
(must-equal #(a 10) '#(a 10))
(must-equal #(a 10) #(a 10))
(must-equal #u8(64 65) '#u8(64 65))
(must-equal #u8(64 65) #u8(64 65))
(must-equal #t '#t)
(must-equal #t #t)
;(must-equal #t #true)
;(must-equal #f #false

(must-equal #(a 10) ((lambda () '#(a 10))))
(must-equal #(a 10) ((lambda () #(a 10))))

(must-raise (syntax-violation quote) (quote))
(must-raise (syntax-violation quote) (quote . a))
(must-raise (syntax-violation quote) (quote  a b))
(must-raise (syntax-violation quote) (quote a . b))

;; procedure call

(must-equal 7 (+ 3 4))
(must-equal 12 ((if #f + *) 3 4))

(must-equal 0 (+))
(must-equal 12 (+ 12))
(must-equal 19 (+ 12 7))
(must-equal 23 (+ 12 7 4))
(must-raise (syntax-violation procedure-call) (+ 12 . 7))

;; lambda

(must-equal 8 ((lambda (x) (+ x x)) 4))

(define reverse-subtract (lambda (x y) (- y x)))
(must-equal 3 (reverse-subtract 7 10))

(define add4 (let ((x 4)) (lambda (y) (+ x y))))
(must-equal 10 (add4 6))

(must-equal (3 4 5 6) ((lambda x x) 3 4 5 6))
(must-equal (5 6) ((lambda (x y . z) z) 3 4 5 6))

(must-equal 4 ((lambda () 4)))

(must-raise (syntax-violation lambda) (lambda (x x) x))
(must-raise (syntax-violation lambda) (lambda (x y z x) x))
(must-raise (syntax-violation lambda) (lambda (z x y . x) x))
(must-raise (syntax-violation lambda) (lambda (x 13 x) x))
(must-raise (syntax-violation lambda) (lambda (x)))
(must-raise (syntax-violation lambda) (lambda (x) . x))

(must-equal 12 ((lambda (a b)
    (define (x c) (+ c b))
    (define (y b)
        (define (z e) (+ (x e) a b))
        (z (+ b b)))
    (y 3)) 1 2))

(define (q w)
    (define (d v) (+ v 1000))
    (define (a x)
        (define (b y)
            (define (c z)
                (d (+ z w)))
            (c (+ y 10)))
        (b (+ x 100)))
    (a 0))
(must-equal 11110 (q 10000))

(define (q u)
    (define (a v)
        (define (b w)
            (define (c x)
                (define (d y)
                    (define (e z)
                        (f z))
                    (e y))
                (d x))
            (c w))
        (b v))
    (define (f t) (* t 2))
    (a u))
(must-equal 4 (q 2))

(define (l x y) x)
(must-raise (assertion-violation l) (l 3))
(must-raise (assertion-violation l) (l 3 4 5))

(define (make-counter n)
    (lambda () n))
(define counter (make-counter 10))
(must-equal 10 (counter))

(must-equal 200
    (letrec ((recur (lambda (x max) (if (= x max) (+ x max) (recur (+ x 1) max))))) (recur 1 100)))

;; if

(must-equal yes (if (> 3 2) 'yes 'no))
(must-equal no (if (> 2 3) 'yes 'no))
(must-equal 1 (if (> 3 2) (- 3 2) (+ 3 2)))

(must-equal true (if #t 'true 'false))
(must-equal false (if #f 'true 'false))
(must-equal 11 (+ 1 (if #t 10 20)))
(must-equal 21 (+ 1 (if #f 10 20)))
(must-equal 11 (+ 1 (if #t 10)))
(must-raise (assertion-violation +) (+ 1 (if #f 10)))

;; set!

(define x 2)
(must-equal 3 (+ x 1))
(set! x 4)
(must-equal 5 (+ x 1))

(define (make-counter n)
    (lambda () (set! n (+ n 1)) n))
(define c1 (make-counter 0))
(must-equal 1 (c1))
(must-equal 2 (c1))
(must-equal 3 (c1))
(define c2 (make-counter 100))
(must-equal 101 (c2))
(must-equal 102 (c2))
(must-equal 103 (c2))
(must-equal 4 (c1))

(define (q x)
    (set! x 10) x)
(must-equal 10 (q 123))

(define (q x)
    (define (r y)
        (set! x y))
    (r 10)
    x)
(must-equal 10 (q 123))

;; include

(must-equal (10 20) (begin (include "..\\test\\include.scm") (list INCLUDE-A include-b)))
(must-raise (assertion-violation) (begin (include "..\\test\\include3.scm") include-c))

(must-equal 10 (let ((a 0) (B 0)) (set! a 1) (set! B 1) (include "..\\test\\include2.scm") a))
(must-equal 20 (let ((a 0) (B 0)) (set! a 1) (set! B 1) (include "..\\test\\include2.scm") B))

;; include-ci

(must-raise (assertion-violation) (begin (include-ci "..\\test\\include4.scm") INCLUDE-E))
(must-equal (10 20) (begin (include-ci "..\\test\\include5.scm") (list include-g include-h)))

(must-equal 10 (let ((a 0) (b 0)) (set! a 1) (set! b 1) (include-ci "..\\test\\include2.scm") a))
(must-equal 20 (let ((a 0) (b 0)) (set! a 1) (set! b 1) (include-ci "..\\test\\include2.scm") b))

;; cond

(define (cadr obj) (car (cdr obj)))

(must-equal greater (cond ((> 3 2) 'greater) ((< 3 2) 'less)))
(must-equal equal (cond ((> 3 3) 'greater) ((< 3 3) 'less) (else 'equal)))
(must-equal 2 (cond ('(1 2 3) => cadr) (else #f)))
;(must-equal 2 (cond ((assv 'b '((a 1) (b 2))) => cadr) (else #f)))

;; case

(must-equal composite (case (* 2 3) ((2 3 5 7) 'prime) ((1 4 6 8 9) 'composite)))
(must-equal c (case (car '(c d)) ((a) 'a) ((b) 'b)))
(must-equal vowel
    (case (car '(i d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant)))
(must-equal semivowel
    (case (car '(w d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant)))
(must-equal consonant
    (case (car '(c d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant)))

(must-equal c (case (car '(c d)) ((a e i o u) 'vowel) ((w y) 'semivowel)
        (else => (lambda (x) x))))
(must-equal (vowel o) (case (car '(o d)) ((a e i o u) => (lambda (x) (list 'vowel x)))
        ((w y) 'semivowel)
        (else => (lambda (x) x))))
(must-equal (semivowel y) (case (car '(y d)) ((a e i o u) 'vowel)
        ((w y) => (lambda (x) (list 'semivowel x)))
        (else => (lambda (x) x))))

(must-equal composite
    (let ((ret (case (* 2 3) ((2 3 5 7) 'prime) ((1 4 6 8 9) 'composite)))) ret))
(must-equal c (let ((ret (case (car '(c d)) ((a) 'a) ((b) 'b)))) ret))
(must-equal vowel
    (let ((ret (case (car '(i d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant))))
        ret))
(must-equal semivowel
    (let ((ret (case (car '(w d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant))))
        ret))
(must-equal consonant
    (let ((ret (case (car '(c d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant))))
        ret))

(must-equal c (let ((ret (case (car '(c d)) ((a e i o u) 'vowel) ((w y) 'semivowel)
        (else => (lambda (x) x))))) ret))
(must-equal (vowel o) (let ((ret (case (car '(o d)) ((a e i o u) => (lambda (x) (list 'vowel x)))
        ((w y) 'semivowel)
        (else => (lambda (x) x))))) ret))
(must-equal (semivowel y) (let ((ret (case (car '(y d)) ((a e i o u) 'vowel)
        ((w y) => (lambda (x) (list 'semivowel x)))
        (else => (lambda (x) x))))) ret))

(must-equal a
    (let ((ret 'nothing)) (case 'a ((a b c d) => (lambda (x) (set! ret x) x))
        ((e f g h) (set! ret 'efgh) ret) (else (set! ret 'else))) ret))
(must-equal efgh
    (let ((ret 'nothing)) (case 'f ((a b c d) => (lambda (x) (set! ret x) x))
        ((e f g h) (set! ret 'efgh) ret) (else (set! ret 'else))) ret))
(must-equal else
    (let ((ret 'nothing)) (case 'z ((a b c d) => (lambda (x) (set! ret x) x))
        ((e f g h) (set! ret 'efgh) ret) (else (set! ret 'else))) ret))

;; and

(must-equal #t (and (= 2 2) (> 2 1)))
(must-equal #f (and (= 2 2) (< 2 1)))
(must-equal (f g) (and 1 2 'c '(f g)))
(must-equal #t (and))

;; or

(must-equal #t (or (= 2 2) (> 2 1)))
(must-equal #t (or (= 2 2) (< 2 1)))
(must-equal #f (or #f #f #f))
(must-equal (b c) (or '(b c) (/ 3 0)))

(must-equal #t (let ((x (or (= 2 2) (> 2 1)))) x))
(must-equal #t (let ((x (or (= 2 2) (< 2 1)))) x))
(must-equal #f (let ((x (or #f #f #f))) x))
(must-equal (b c) (let ((x (or '(b c) (/ 3 0)))) x))

(must-equal 1 (let ((x 0)) (or (begin (set! x 1) #t) (begin (set! x 2) #t)) x))
(must-equal 2 (let ((x 0)) (or (begin (set! x 1) #f) (begin (set! x 2) #t) (/ 3 0)) x))

;; when

(must-equal greater (when (> 3 2) 'greater))

;; unless

(must-equal less (unless (< 3 2) 'less))

;; cond-expand

(must-equal 1
    (cond-expand (no-features 0) (r7rs 1) (i386 2)))
(must-equal 2
    (cond-expand (no-features 0) (no-r7rs 1) (i386 2)))
(must-equal 0
    (cond-expand ((not no-features) 0) (no-r7rs 1) (i386 2)))

(must-equal 1
    (cond-expand ((and r7rs no-features) 0) ((and r7rs i386) 1) (r7rs 2)))
(must-equal 0
    (cond-expand ((or no-features r7rs) 0) ((and r7rs i386) 1) (r7rs 2)))

(must-equal 1
    (cond-expand ((and r7rs no-features) 0) (else 1)))

(must-raise (syntax-violation cond-expand)
    (cond-expand ((and r7rs no-features) 0) (no-features 1)))

(must-equal 1 (cond-expand ((library (scheme base)) 1) (else 2)))
(must-equal 2 (cond-expand ((library (not a library)) 1) (else 2)))
(must-equal 1 (cond-expand ((library (lib ce1)) 1) (else 2)))

(must-equal 1
    (let ((x 0) (y 1) (z 2)) (cond-expand (no-features x) (r7rs y) (i386 z))))
(must-equal 2
    (let ((x 0) (y 1) (z 2)) (cond-expand (no-features x) (no-r7rs y) (i386 z))))
(must-equal 0
    (let ((x 0) (y 1) (z 2)) (cond-expand ((not no-features) x) (no-r7rs y) (i386 z))))

(must-equal 1
    (let ((x 0) (y 1) (z 2))
        (cond-expand ((and r7rs no-features) x) ((and r7rs i386) y) (r7rs z))))
(must-equal 0
    (let ((x 0) (y 1) (z 2)) (cond-expand ((or no-features r7rs) x) ((and r7rs i386) y) (r7rs z))))

(must-equal 1
    (let ((x 0) (y 1)) (cond-expand ((and r7rs no-features) 0) (else 1))))

(must-raise (syntax-violation cond-expand)
    (let ((x 0) (y 1)) (cond-expand ((and r7rs no-features) x) (no-features y))))

(must-equal 1
    (let ((x 1) (y 2)) (cond-expand ((library (scheme base)) x) (else y))))
(must-equal 2
    (let ((x 1) (y 2)) (cond-expand ((library (not a library)) x) (else y))))

(must-equal 1
    (let ((x 1) (y 2)) (cond-expand ((library (lib ce2)) x) (else y))))

;; let

(must-equal 6 (let ((x 2) (y 3)) (* x y)))
(must-equal 35 (let ((x 2) (y 3)) (let ((x 7) (z (+ x y))) (* z x))))

(must-equal 2 (let ((a 2) (b 7)) a))
(must-equal 7 (let ((a 2) (b 7)) b))
(must-equal 2 (let ((a 2) (b 7)) (let ((a a) (b a)) a)))
(must-equal 2 (let ((a 2) (b 7)) (let ((a a) (b a)) b)))

(must-raise (syntax-violation let) (let))
(must-raise (syntax-violation let) (let (x 2) x))
(must-raise (syntax-violation let) (let x x))
(must-raise (syntax-violation let) (let ((x)) x))
(must-raise (syntax-violation let) (let ((x) 2) x))
(must-raise (syntax-violation let) (let ((x 2) y) x))
(must-raise (syntax-violation let) (let ((x 2) . y) x))
(must-raise (syntax-violation let) (let ((x 2) (x 3)) x))
(must-raise (syntax-violation let) (let ((x 2) (y 1) (x 3)) x))
(must-raise (syntax-violation let) (let ((x 2))))
(must-raise (syntax-violation let) (let ((x 2)) . x))
(must-raise (syntax-violation let) (let ((x 2)) y . x))
(must-raise (syntax-violation let) (let (((x y z) 2)) y x))
(must-raise (syntax-violation let) (let ((x 2) ("y" 3)) y))

;; let*

(must-equal 70 (let ((x 2) (y 3)) (let* ((x 7) (z (+ x y))) (* z x))))

(must-equal 2 (let* ((a 2) (b 7)) a))
(must-equal 7 (let* ((a 2) (b 7)) b))
(must-equal 4 (let ((a 2) (b 7)) (let* ((a (+ a a)) (b a)) b)))

(must-raise (syntax-violation let*) (let*))
(must-raise (syntax-violation let*) (let* (x 2) x))
(must-raise (syntax-violation let*) (let* x x))
(must-raise (syntax-violation let*) (let* ((x)) x))
(must-raise (syntax-violation let*) (let* ((x) 2) x))
(must-raise (syntax-violation let*) (let* ((x 2) y) x))
(must-raise (syntax-violation let*) (let* ((x 2) . y) x))
(must-equal 3 (let* ((x 2) (x 3)) x))
(must-equal 3 (let* ((x 2) (y 1) (x 3)) x))
(must-raise (syntax-violation let*) (let* ((x 2))))
(must-raise (syntax-violation let*) (let* ((x 2)) . x))
(must-raise (syntax-violation let*) (let* ((x 2)) y . x))
(must-raise (syntax-violation let*) (let* (((x y z) 2)) y x))
(must-raise (syntax-violation let*) (let* ((x 2) ("y" 3)) y))

;; letrec

(must-equal #t (letrec ((even? (lambda (n) (if (zero? n) #t (odd? (- n 1)))))
                        (odd? (lambda (n) (if (zero? n) #f (even? (- n 1))))))
                    (even? 88)))

(must-raise (syntax-violation letrec) (letrec))
(must-raise (syntax-violation letrec) (letrec (x 2) x))
(must-raise (syntax-violation letrec) (letrec x x))
(must-raise (syntax-violation letrec) (letrec ((x)) x))
(must-raise (syntax-violation letrec) (letrec ((x) 2) x))
(must-raise (syntax-violation letrec) (letrec ((x 2) y) x))
(must-raise (syntax-violation letrec) (letrec ((x 2) . y) x))
(must-raise (syntax-violation letrec) (letrec ((x 2) (x 3)) x))
(must-raise (syntax-violation letrec) (letrec ((x 2) (y 1) (x 3)) x))
(must-raise (syntax-violation letrec) (letrec ((x 2))))
(must-raise (syntax-violation letrec) (letrec ((x 2)) . x))
(must-raise (syntax-violation letrec) (letrec ((x 2)) y . x))
(must-raise (syntax-violation letrec) (letrec (((x y z) 2)) y x))
(must-raise (syntax-violation letrec) (letrec ((x 2) ("y" 3)) y))

;; letrec*

(must-equal 5 (letrec* ((p (lambda (x) (+ 1 (q (- x 1)))))
                        (q (lambda (y) (if (zero? y) 0 (+ 1 (p (- y 1))))))
                    (x (p 5))
                    (y x))
                    y))

(must-equal 45 (let ((x 5))
                (letrec* ((foo (lambda (y) (bar x y)))
                            (bar (lambda (a b) (+ (* a b) a))))
                    (foo (+ x 3)))))

(must-raise (syntax-violation letrec*) (letrec*))
(must-raise (syntax-violation letrec*) (letrec* (x 2) x))
(must-raise (syntax-violation letrec*) (letrec* x x))
(must-raise (syntax-violation letrec*) (letrec* ((x)) x))
(must-raise (syntax-violation letrec*) (letrec* ((x) 2) x))
(must-raise (syntax-violation letrec*) (letrec* ((x 2) y) x))
(must-raise (syntax-violation letrec*) (letrec* ((x 2) . y) x))
(must-raise (syntax-violation letrec*) (letrec* ((x 2) (x 3)) x))
(must-raise (syntax-violation letrec*) (letrec* ((x 2) (y 1) (x 3)) x))
(must-raise (syntax-violation letrec*) (letrec* ((x 2))))
(must-raise (syntax-violation letrec*) (letrec* ((x 2)) . x))
(must-raise (syntax-violation letrec*) (letrec* ((x 2)) y . x))
(must-raise (syntax-violation letrec*) (letrec* (((x y z) 2)) y x))
(must-raise (syntax-violation letrec*) (letrec* ((x 2) ("y" 3)) y))

;; let-values

(must-equal (1 2 3 4) (let-values (((a b) (values 1 2))
                                    ((c d) (values 3 4)))
                                (list a b c d)))
(must-equal (1 2 (3 4)) (let-values (((a b . c) (values 1 2 3 4)))
                                (list a b c)))
(must-equal (x y a b) (let ((a 'a) (b 'b) (x 'x) (y 'y))
                            (let-values (((a b) (values x y))
                                           ((x y) (values a b)))
                                    (list a b x y))))
(must-equal (1 2 3) (let-values ((x (values 1 2 3))) x))

(define (v)
    (define (v1) (v3) (v2) (v2))
    (define (v2) (v3) (v3))
    (define (v3) (values 1 2 3 4))
    (v1))
(must-equal (4 3 2 1) (let-values (((w x y z) (v))) (list z y x w)))

(must-raise (syntax-violation let-values) (let-values))
(must-raise (syntax-violation let-values) (let-values (x 2) x))
(must-raise (syntax-violation let-values) (let-values x x))
(must-raise (syntax-violation let-values) (let-values ((x)) x))
(must-raise (syntax-violation let-values) (let-values ((x) 2) x))
(must-raise (syntax-violation let-values) (let-values ((x 2) y) x))
(must-raise (syntax-violation let-values) (let-values ((x 2) . y) x))
(must-raise (syntax-violation let-values) (let-values ((x 2) (x 3)) x))
(must-raise (syntax-violation let-values) (let-values ((x 2) (y 1) (x 3)) x))
(must-raise (syntax-violation let-values) (let-values ((x 2))))
(must-raise (syntax-violation let-values) (let-values ((x 2)) . x))
(must-raise (syntax-violation let-values) (let-values ((x 2)) y . x))
(must-raise (syntax-violation let-values) (let-values (((x 2 y z) 2)) y x))
(must-raise (syntax-violation let-values) (let-values ((x 2) ("y" 3)) y))

;; let*-values

(must-equal (x y x y) (let ((a 'a) (b 'b) (x 'x) (y 'y))
                            (let*-values (((a b) (values x y))
                                            ((x y) (values a b)))
                                (list a b x y))))
(must-equal ((1 2 3) 4 5) (let*-values ((x (values 1 2 3)) (x (values x 4 5))) x))

(must-raise (syntax-violation let*-values) (let*-values))
(must-raise (syntax-violation let*-values) (let*-values (x 2) x))
(must-raise (syntax-violation let*-values) (let*-values x x))
(must-raise (syntax-violation let*-values) (let*-values ((x)) x))
(must-raise (syntax-violation let*-values) (let*-values ((x) 2) x))
(must-raise (syntax-violation let*-values) (let*-values ((x 2) y) x))
(must-raise (syntax-violation let*-values) (let*-values ((x 2) . y) x))
(must-raise (syntax-violation let*-values) (let*-values ((x 2))))
(must-raise (syntax-violation let*-values) (let*-values ((x 2)) . x))
(must-raise (syntax-violation let*-values) (let*-values ((x 2)) y . x))
(must-raise (syntax-violation let*-values) (let*-values (((x 2 y z) 2)) y x))
(must-raise (syntax-violation let*-values) (let*-values ((x 2) ("y" 3)) y))

;; begin

;; do

(define (range b e)
    (do ((r '() (cons e r))
        (e (- e 1) (- e 1)))
        ((< e b) r)))

(must-equal (3 4) (range 3 5))

(must-equal #(0 1 2 3 4)
    (do ((vec (make-vector 5))
        (i 0 (+ i 1)))
        ((= i 5) vec)
        (vector-set! vec i i)))

(must-equal 25
    (let ((x '(1 3 5 7 9)))
        (do ((x x (cdr x))
            (sum 0 (+ sum (car x))))
            ((null? x) sum))))

;; named let

(must-equal ((6 1 3) (-5 -2))
    (let loop ((numbers '(3 -2 1 6 -5))
            (nonneg '())
            (neg '()))
        (cond ((null? numbers) (list nonneg neg))
            ((>= (car numbers) 0)
                (loop (cdr numbers) (cons (car numbers) nonneg) neg))
            ((< (car numbers) 0)
                (loop (cdr numbers) nonneg (cons (car numbers) neg))))))

;; case-lambda

(define range
    (case-lambda
        ((e) (range 0 e))
        ((b e) (do ((r '() (cons e r))
                    (e (- e 1) (- e 1)))
                    ((< e b) r)))))

(must-equal (0 1 2) (range 3))
(must-equal (3 4) (range 3 5))
(must-raise (assertion-violation case-lambda) (range 1 2 3))

(define cl-test
    (case-lambda
        (() 'none)
        ((a) 'one)
        ((b c) 'two)
        ((b . d) 'rest)))

(must-equal none (cl-test))
(must-equal one (cl-test 0))
(must-equal two (cl-test 0 0))
(must-equal rest (cl-test 0 0 0))

(define cl-test2
    (case-lambda
        ((a b) 'two)
        ((c d e . f) 'rest)))

(must-raise (assertion-violation case-lambda) (cl-test2 0))
(must-equal two (cl-test2 0 0))
(must-equal rest (cl-test2 0 0 0))
(must-equal rest (cl-test2 0 0 0 0))

(must-raise (assertion-violation case-lambda)
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl)))
(must-equal one
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl 0)))
(must-equal two
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl 0 0)))
(must-equal rest
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl 0 0 0)))
(must-equal rest
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl 0 0 0 0)))

(must-equal one (let ((cl (case-lambda ((a) 'one) ((a b) 'two)))) (eq? cl cl) (cl 1)))

;; quasiquote

(must-equal (list 3 4) `(list ,(+ 1 2) 4))
(must-equal (list a (quote a)) (let ((name 'a)) `(list ,name ',name)))

(must-equal (a 3 4 5 6 b) `(a ,(+ 1 2) ,@(map abs '(4 -5 6)) b))
(must-equal ((foo 7) . cons) `((foo ,(- 10 3)) ,@(cdr '(c)) . ,(car '(cons))))
(must-equal #(10 5 2 4 3 8) `#(10 5 ,(sqrt 4) ,@(map sqrt '(16 9)) 8))
(must-equal (list foo bar baz) (let ((foo '(foo bar)) (@baz 'baz)) `(list ,@foo , @baz)))

(must-equal (a `(b ,(+ 1 2) ,(foo 4 d) e) f) `(a `(b ,(+ 1 2) ,(foo ,(+ 1 3) d) e) f))
(must-equal (a `(b ,x ,'y d) e) (let ((name1 'x) (name2 'y)) `(a `(b ,,name1 ,',name2 d) e)))

(must-equal (list 3 4) (quasiquote (list (unquote (+ 1 2)) 4)))
(must-equal `(list ,(+ 1 2) 4) '(quasiquote (list (unquote (+ 1 2)) 4)))

;; define

(define add3 (lambda (x) (+ x 3)))
(must-equal 6 (add3 3))

(define first car)
(must-equal 1 (first '(1 2)))

(must-equal 45 (let ((x 5))
                    (define foo (lambda (y) (bar x y)))
                    (define bar (lambda (a b) (+ (* a b) a)))
                (foo (+ x 3))))

;; define-syntax

;; defines-values

;;
;; ---- macros ----
;;

;; syntax-rules

(define-syntax be-like-begin
    (syntax-rules ()
        ((be-like-begin name)
            (define-syntax name
                (syntax-rules ()
                    ((name expr (... ...)) (begin expr (... ...))))))))
(be-like-begin sequence)
(must-equal 4 (sequence 1 2 3 4))
(must-equal 4 ((lambda () (be-like-begin sequence) (sequence 1 2 3 4))))

(must-equal ok (let ((=> #f)) (cond (#t => 'ok))))

;; let-syntax

(must-equal now (let-syntax
                    ((when (syntax-rules ()
                        ((when test stmt1 stmt2 ...)
                            (if test
                                (begin stmt1 stmt2 ...))))))
                    (let ((if #t))
                        (when if (set! if 'now))
                        if)))

(must-equal outer (let ((x 'outer))
                      (let-syntax ((m (syntax-rules () ((m) x))))
                            (let ((x 'inner))
                                (m)))))

;; letrec-syntax

(must-equal 7 (letrec-syntax
                    ((my-or (syntax-rules ()
                    ((my-or) #f)
                    ((my-or e) e)
                    ((my-or e1 e2 ...)
                        (let ((temp e1))
                            (if temp temp (my-or e2 ...)))))))
                (let ((x #f)
                        (y 7)
                        (temp 8)
                        (let odd?)
                        (if even?))
                    (my-or x (let temp) (if y) y))))

;; syntax-error

;;
;; ---- program structure ----
;;

;; define-library

(must-equal 100 (begin (import (lib a b c)) lib-a-b-c))
(must-equal 10 (begin (import (lib t1)) lib-t1-a))
(must-raise (assertion-violation) lib-t1-b)
(must-equal 20 b-lib-t1)
(must-raise (assertion-violation) lib-t1-c)

(must-equal 10 (begin (import (lib t2)) (lib-t2-a)))
(must-equal 20 (lib-t2-b))
(must-raise (syntax-violation) (import (lib t3)))
(must-raise (syntax-violation) (import (lib t4)))

(must-equal 1000 (begin (import (lib t5)) (lib-t5-b)))
(must-equal 1000 lib-t5-a)

(must-equal 1000 (begin (import (lib t6)) (lib-t6-b)))
(must-equal 1000 (lib-t6-a))

(must-equal 1000 (begin (import (lib t7)) (lib-t7-b)))
(must-equal 1000 lib-t7-a)

(must-equal 1 (begin (import (only (lib t8) lib-t8-a lib-t8-c)) lib-t8-a))
(must-raise (assertion-violation) lib-t8-b)
(must-equal 3 lib-t8-c)
(must-raise (assertion-violation) lib-t8-d)

(must-equal 1 (begin (import (except (lib t9) lib-t9-b lib-t9-d)) lib-t9-a))
(must-raise (assertion-violation) lib-t9-b)
(must-equal 3 lib-t9-c)
(must-raise (assertion-violation) lib-t9-d)

(must-equal 1 (begin (import (prefix (lib t10) x)) xlib-t10-a))
(must-raise (assertion-violation) lib-t10-b)
(must-equal 3 xlib-t10-c)
(must-raise (assertion-violation) lib-t10-d)

(must-equal 1 (begin (import (rename (lib t11) (lib-t11-b b-lib-t11) (lib-t11-d d-lib-t11)))
    lib-t11-a))
(must-raise (assertion-violation) lib-t11-b)
(must-equal 2 b-lib-t11)
(must-equal 3 lib-t11-c)
(must-raise (assertion-violation) lib-t11-d)
(must-equal 4 d-lib-t11)

(must-raise (syntax-violation) (import bad "bad library" name))
(must-raise (syntax-violation)
    (define-library (no ("good") "library") (import (scheme base)) (export +)))

(must-raise (syntax-violation) (import (lib t12)))
(must-raise (syntax-violation) (import (lib t13)))

(must-equal 10 (begin (import (lib t14)) (lib-t14-a 10 20)))
(must-equal 10 (lib-t14-b 10 20))

(must-equal 10 (begin (import (lib t15)) (lib-t15-a 10 20)))
(must-equal 10 (lib-t15-b 10 20))

;;
;; ---- pairs ----
;;

;; pair?

(must-equal #t (pair? '(a . b)))
(must-equal #t (pair? '(a b c)))
(must-equal #f (pair? '()))
(must-equal #f (pair? '#(a b)))

;; cons

(must-equal (a) (cons 'a '()))
(must-equal ((a) b c d) (cons '(a) '(b c d)))
(must-equal ("a" b c) (cons "a" '(b c)))
(must-equal (a . 3) (cons 'a 3))
(must-equal ((a b) . c) (cons '(a b) 'c))

(must-equal a (car '(a b c)))
(must-equal (a) (car '((a) b c d)))
(must-equal 1 (car '(1 . 2)))
(must-raise (assertion-violation car) (car '()))

(must-equal (b c d) (cdr '((a) b c d)))
(must-equal 2 (cdr '(1 . 2)))
(must-raise (assertion-violation cdr) (cdr '()))

;; append

(must-equal (x y) (append '(x) '(y)))
(must-equal (a b c d) (append '(a) '(b c d)))
(must-equal (a (b) (c)) (append '(a (b)) '((c))))
(must-equal (a b c . d) (append '(a b) '(c . d)))
(must-equal a (append '() 'a))

;; reverse

(must-equal (c b a) (reverse '(a b c)))
(must-equal ((e (f)) d (b c) a) (reverse '(a (b c) d (e (f)))))

;;
;; ---- control features ----
;;

;; procedure?

;; apply

(must-equal 7 (apply + (list 3 4)))

(define compose
    (lambda (f g)
        (lambda args
            (f (apply g args)))))
(must-equal 30 ((compose sqrt *) 12 75))

(must-equal 15 (apply + '(1 2 3 4 5)))
(must-equal 15 (apply + 1 '(2 3 4 5)))
(must-equal 15 (apply + 1 2 '(3 4 5)))
(must-equal 15 (apply + 1 2 3 4 5 '()))

(must-raise (assertion-violation apply) (apply +))
(must-raise (assertion-violation apply) (apply + '(1 2 . 3)))
(must-raise (assertion-violation apply) (apply + 1))

;; map

(define (cadr obj) (car (cdr obj)))
(must-equal (b e h) (map cadr '((a b) (d e) (g h))))
(must-equal (1 4 27 256 3125) (map (lambda (n) (expt n n)) '(1 2 3 4 5)))
(must-equal (5 7 9) (map + '(1 2 3) '(4 5 6 7)))

;; string-map

;; vector-map

;; for-each

(must-equal #(0 1 4 9 16)
    (let ((v (make-vector 5)))
        (for-each (lambda (i) (vector-set! v i (* i i))) '(0 1 2 3 4))
        v))

;; string-for-each

;; vector-for-each

;; call-with-current-continuation
;; call/cc

;; values

(define (v0) (values))
(define (v1) (values 1))
(define (v2) (values 1 2))

(must-equal 1 (+ (v1) 0))
(must-raise (assertion-violation values) (+ (v0) 0))
(must-raise (assertion-violation values) (+ (v2) 0))
(must-equal 1 (begin (v0) 1))
(must-equal 1 (begin (v1) 1))
(must-equal 1 (begin (v2) 1))

;; call-with-values

(must-equal 5 (call-with-values (lambda () (values 4 5)) (lambda (a b) b)))
(must-equal 4 (call-with-values (lambda () (values 4 5)) (lambda (a b) a)))
(must-equal -1 (call-with-values * -))

;; dynamic-wind


;;
;; ---- input and output ----
;;

;; write

(define (cddr o) (cdr (cdr o)))
(must-equal "#0=(a b c . #0#)"
    (let ((p (open-output-string)))
        (let ((x (list 'a 'b 'c)))
            (set-cdr! (cddr x) x)
            (write x p)
            (get-output-string p))))

(must-equal "#0=(val1 . #0#)"
    (let ((p (open-output-string)))
        (let ((a (cons 'val1 'val2)))
           (set-cdr! a a)
           (write a p)
           (get-output-string p))))

(must-equal "((a b c) a b c)"
    (let ((p (open-output-string)))
        (let ((x (list 'a 'b 'c)))
            (write (cons x x) p)
            (get-output-string p))))

;; write-shared

(must-equal "(#0=(a b c) . #0#)"
    (let ((p (open-output-string)))
        (let ((x (list 'a 'b 'c)))
            (write-shared (cons x x) p)
            (get-output-string p))))