#
# To run this example, you need spork (https://github.com/janet-lang/spork.git)
# `jpm -l install spork` and then `jpm -l janet path/to/repl-client.janet`
#

(import spork/netrepl)

(defn main [& args]
  (def unix-socket-path
    (if (< (length args) 2)
      (do
        (def display-name
          (or (os/getenv "WAYLAND_DISPLAY")
              (error "no socket path provided and no WAYLAND_DISPLAY found")))
        (string "/tmp/" display-name "-repl.socket"))
      (in args 1)))
  (netrepl/client :unix unix-socket-path))
