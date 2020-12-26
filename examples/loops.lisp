(print "result:"
    (for x (range 0 10)
        (print (* 2 x))))

(defun yes_or_no (prompt)
    (if (= (input prompt) "y") 1 0))


(defun collatz (n) (do
    (print "performing collatz of" n)
    (define save n)
    (while (!= n 1)
        (print n)
        (if (% n 2)
            (define n (+ (* 3 n) 1))
            (define n (/ n 2))
        )
    )
    (print "collatz of" save "reaches 1")
))

(collatz (first (parse (input "Enter a number to perform collatz on: "))))