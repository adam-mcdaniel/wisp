(defun dec (n) (- n 1))
(defun inc (n) (+ n 1))
(defun not (x) (if x 0 1))

(defun neg (n) (- 0 n))

(defun is-pos (n) (> n 0))
(defun is-neg (n) (< n 0))

(defun const (n) (lambda (_) n))

(defun pow (n exp)
    (if (= exp 0)
        1
        (if (< exp 0)
            (pow (/ 1 n) (neg exp))
            (reduce
                (lambda (acc x) (* acc x))
                n
                (map (const n) (range 1 exp))
            ))
        )
    )

(defun is-odd (n) (% n 2))
(defun is-even (n) (not (is-odd n)))

(defun fact (n) (reduce * 1.0 (range 2 (inc n))))

"success"