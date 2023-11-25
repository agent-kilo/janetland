(declare-project :name "janetland")


(def proto-files
  ["xdg-shell"
   "viewporter"
   "presentation-time"])


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

(defn file-exists? [name]
  (def stat (os/stat name))
  (and (not (nil? stat)) (= (stat :mode) :file)))

(defn dir-exists? [name]
  (def stat (os/stat name))
  (and (not (nil? stat)) (= (stat :mode) :directory)))

(defn ensure-dir [name]
  (when (not (dir-exists? name))
    (when (not (os/mkdir name))
      (error (string/format "failed to create directory %s" name)))))


(def generated-headers-dir "generated_headers")

(def wlr-cflags
  (let [arr @[]]
    (array/concat
     arr
     @["-DWLR_USE_UNSTABLE" (string "-I" generated-headers-dir)]
     (string/split " " (pkg-config "--cflags" "--libs" "wlroots"))
     (string/split " " (pkg-config "--cflags" "--libs" "wayland-server"))
     (string/split " " (pkg-config "--cflags" "--libs" "xkbcommon")))))

(def common-cflags ["-g" "-Wall" "-Wextra"])


(declare-native :name (project-module "wlr")
                :source ["wlr.c"]
                :header ["jl.h"
                         "types.h"
                         "wlr_abs_types.h"
                         (string generated-headers-dir "/xdg-shell-protocol.h")]
                :cflags [;common-cflags ;wlr-cflags])

(declare-native :name (project-module "wl")
                :source ["wl.c"]
                :header ["jl.h" "types.h" "wl_abs_types.h"]
                :cflags [;common-cflags ;wlr-cflags])

(declare-native :name (project-module "util")
                :source ["util.c"]
                :header ["jl.h"]
                :cflags [;common-cflags ;wlr-cflags])


(task "proto-headers" []
  (def wl-proto-dir (pkg-config "--variable=pkgdatadir" "wayland-protocols"))
  (def wl-scanner (pkg-config "--variable=wayland_scanner" "wayland-scanner"))
  (ensure-dir generated-headers-dir)
  (map (fn [pfile]
         (def out-file (string generated-headers-dir "/" pfile "-protocol.h"))
         (when (file-exists? out-file)
           (printf "%s exists, skipping" out-file)
           (break))
         (def scanner-out
           (spawn-and-wait wl-scanner
                           "server-header"
                           (string wl-proto-dir "/stable/" pfile "/" pfile ".xml")
                           out-file))
         (print scanner-out)
         (printf "generated %s" out-file))
       proto-files))
