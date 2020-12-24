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

    
; this is too!
"success"