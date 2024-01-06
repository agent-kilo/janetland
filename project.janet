(declare-project
 :name "janetland"
 :dependencies [{:url "https://github.com/janet-lang/spork.git"
                 :tag "d644da0fd05612a2d5a3c97277bf7b9bb96dcf6b"}])


(def proto-files
  ["stable/xdg-shell/xdg-shell.xml"
   "staging/ext-session-lock/ext-session-lock-v1.xml"])


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
(def generated-tables-dir "generated_tables")

(def wlr-cflags
  (let [arr @[]]
    (array/concat
     arr
     @["-DWLR_USE_UNSTABLE" (string "-I" generated-headers-dir)]
     (string/split " " (pkg-config "--cflags" "--libs" "wlroots"))
     (string/split " " (pkg-config "--cflags" "--libs" "wayland-server"))
     (string/split " " (pkg-config "--cflags" "--libs" "xkbcommon")))))

(def common-cflags ["-g" "-Wall" "-Wextra"])

(def protocol-file-peg
  (peg/compile
   ~{:main (sequence (opt :sep) (any :dir) (capture :file-name) :ext)
     :sep (some "/")
     :dir (sequence :file-name :sep)
     :file-name (some (choice "_" "-" :w))
     :ext ".xml"}))

(def include-path-peg
  (peg/compile
   ~{:main (sequence :i-flag :path -1)
     :i-flag (sequence (any :s) "-I")
     :path (sequence (any :s) (capture (some 1)) (any :s))}))

(def keysym-define-peg
  (peg/compile
   ~{:main (sequence :define :name :value (opt :comment) -1)
     :define (sequence (any :s) "#define" (some :s))
     :name (sequence "XKB_KEY_" (capture (some (choice "_" :w))) (some :s))
     :value (sequence (number (sequence "0x" :h+)) (any :s))
     :comment (choice (sequence "//" (any :s) (capture (some 1)))
                      (sequence :ml-comment-begin  (capture (to :ml-comment-end)) :ml-comment-end))
     :ml-comment-begin (sequence "/*" (any :s))
     :ml-comment-end (sequence (any :s) "*/")}))


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

(declare-native :name (project-module "xkb")
                :source ["xkb.c"]
                :header ["jl.h" "types.h"]
                :cflags [;common-cflags ;wlr-cflags])

(declare-native :name (project-module "xcb")
                :source ["xcb.c"]
                :header ["jl.h" "types.h"]
                :cflags [;common-cflags ;wlr-cflags])

(declare-native :name (project-module "util")
                :source ["util.c"]
                :header ["jl.h"]
                :cflags [;common-cflags ;wlr-cflags])

(declare-source :source [(string generated-tables-dir "/keysyms.janet")]
                :prefix ((dyn :project) :name))


(task "proto-headers" []
  (def wl-proto-dir (pkg-config "--variable=pkgdatadir" "wayland-protocols"))
  (def wl-scanner (pkg-config "--variable=wayland_scanner" "wayland-scanner"))
  (ensure-dir generated-headers-dir)
  (map (fn [pfile-path]
         (def matched (peg/match protocol-file-peg pfile-path))
         (when (nil? matched)
           (printf "failed to match file, skipping: %s" pfile-path)
           (break))
         (def pfile (in matched 0))
         (def out-file (string generated-headers-dir "/" pfile "-protocol.h"))
         (when (file-exists? out-file)
           (printf "%s exists, skipping" out-file)
           (break))
         (def scanner-out
           (spawn-and-wait wl-scanner
                           "server-header"
                           (string wl-proto-dir "/" pfile-path)
                           out-file))
         (print scanner-out)
         (printf "generated %s" out-file))
       proto-files))


(task "keysym-table" []
  (def xkbcommon-cflags (pkg-config "--cflags" "xkbcommon"))
  (def matched-arr (peg/match include-path-peg xkbcommon-cflags))
  (when (or (nil? matched-arr) (empty? matched-arr))
    (error (string/format "unrecognised flags from pkg-config: %s" xkbcommon-cflags)))

  (def include-path (in matched-arr 0))
  (def keysym-file-path (string include-path "/xkbcommon/xkbcommon-keysyms.h"))
  (def keysym-file (file/open keysym-file-path :rn))
  (ensure-dir generated-tables-dir)
  (def keysym-table-file-path (string generated-tables-dir "/keysyms.janet"))
  (def keysym-table-file (file/open keysym-table-file-path :wn))

  (file/write keysym-table-file
              "(def xkb-key {\n")

  (each line (file/lines keysym-file)
    (def matched (peg/match keysym-define-peg (string/trim line)))
    (when (not (nil? matched))
      (def [name value comment] matched)
      (file/write keysym-table-file
                  (string/format "  :%s    (int/u64 %d)%s\n"
                                 name
                                 value
                                 (if (nil? comment)
                                   ""
                                   (string/format "    # %s" comment))))))

  (file/write keysym-table-file
              "})\n")

  (file/close keysym-table-file)
  (printf "generated %s" keysym-table-file-path))
