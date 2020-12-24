(do 
    (defun repeat (x count) (map (lambda (_) x) (range 0 count)))
    (defun len (l) (reduce (lambda (acc _) (+ acc 1)) 0 l))

    (defun index (l n) (head
        (reduce
            (lambda (acc arg) (tail acc))
            l
            (range 0 n)
        )))

    (print (len (range 0 1000)))

    "success")