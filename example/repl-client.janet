(import spork/netrepl)

(defn main [& args]
  (when (< (length args) 2)
    (error "no socket path provided"))
  (def unix-socket-path (in args 1))
  (netrepl/client :unix unix-socket-path "example-client"))
