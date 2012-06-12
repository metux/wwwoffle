;;
;; Invoke WWWOFFLE as the program to browse a URL in emacs.
;;

(defvar browse-url-wwwoffle-program "wwwoffle"
  "*The name for invoking WWWOFFLE.")

(defvar browse-url-wwwoffle-arguments nil
  "*A list of strings to pass to WWWOFFLE as arguments.")

(defun browse-url-wwwoffle (url)
  "Ask WWWOFFLE to load a URL.
Default to the URL around or before point."
  (interactive (browse-url-interactive-arg "WWWOFFLE URL: "))
  (message "Running WWWOFFLE...")
  (apply 'start-process "wwwoffle" nil browse-url-wwwoffle-program
         (append browse-url-wwwoffle-arguments (list url)))
  (message "Running WWWOFFLE...done")
  )

(setq browse-url-browser-function 'browse-url-wwwoffle)
