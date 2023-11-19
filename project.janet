(declare-project :name "janetland")

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
  (string/trim (spawn-and-wait "pkg-config" ;args)))

(defn project-module [name]
  (string ((dyn :project) :name) "/" name))

(def wlr-cflags
  (let [arr @[]]
    (array/concat
     arr
     @["-DWLR_USE_UNSTABLE"]
     (string/split " " (pkg-config "--cflags" "--libs" "wlroots"))
     (string/split " " (pkg-config "--cflags" "--libs" "wayland-server"))
     (string/split " " (pkg-config "--cflags" "--libs" "xkbcommon")))))

(declare-native :name (project-module "wlr")
                :source ["wlr.c"]
                :cflags ["-g" "-Werror" ;wlr-cflags])
