

(define (hypo a b)
  (define (square x) [* x x])
  (define #;(add x y) (+ x y))

  (sqrt (add {square a}
             (. (square b)))))
