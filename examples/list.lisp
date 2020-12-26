(defun repeat (x count) (map (lambda (_) x) (range 0 count)))
(defun len (l) (reduce (lambda (acc _) (+ acc 1)) 0 l))

; get the nth item in a list
(defun index (l n)
    ; get the head of the result
    (head
        ; get the nth tail of the list `l`
        (reduce
            (lambda (acc arg) (tail acc))
            l
            (range 0 n)))
    )

(defun min (a b) (if (< a b) a b))
(defun max (a b) (if (< a b) b a))

(defun second (l) (index l 1))

(defun random-list (l h size)
    (map
        (lambda (_) (random l h))
        (range 0 size)
    ))


; quicksort a list
(defun qs (l)
    (if (<= (len l) 1)
        l
        (do
            (define pivot (first l))
            (+
                (qs (filter (lambda (n) (> pivot n)) l))
                (list pivot)
                (qs (tail (filter (lambda (n) (<= pivot n)) l)))
            ))
    ))


(defun split-list (l) (do
    (define a '())
    (define b '())
    (define count 0)
    (for x l
        (if (< count (/ (len l) 2))
            (define a (push a x))
            (define b (push b x)))
        (define count (+ count 1))
        )
    (list a b)
))

(defun is-empty (l) (= (len l) 0))

(defun merge-sort (l)
    (if (<= (len l) 1)
        l
        (if (= (len l) 2)
            (list (min (first l) (second l)) (max (first l) (second l)))
            (do
                (define sub-list (split-list l))
                (define a (merge-sort (first sub-list)))
                (define b (merge-sort (second sub-list)))
                (define result '())
                (while (< (len result) (len l))
                    (if (is-empty a)
                        (do
                            (define result (push result (first b)))
                            (define b (tail b))
                        )
                        (if (is-empty b)
                            (do
                                (define result (push result (first a)))
                                (define a (tail a))
                            )
                            (if (<= (first a) (first b))
                                (do
                                    (define result (push result (first a)))
                                    (define a (tail a))
                                )
                                (do
                                    (define result (push result (first b)))
                                    (define b (tail b))
                                )
                            )
                        )
                    )
                )
                result
            )
        )
    ))
; this is too!
"success"