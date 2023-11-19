#(declare-project :name "janetland")

(defn spawn-and-wait [& args]
  (def os-env (os/environ))
  (put os-env :out :pipe)
  (def proc (os/spawn args :ep os-env))
  (os/proc-wait proc)
  (def out (in proc :out))
  (def ret (in proc :return-code))
  (when (not (= ret 0))
    (error (string/format "subprocess exited abnormally: %d" ret)))
  (:read out :all))

(defn pkg-config [& args]
  (spawn-and-wait "pkg-config" ;args))

#(declare-native :name "wlr"
#                :source ["wlr.c"])
