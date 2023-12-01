(use janetland/wl)
(use janetland/wlr)
(use janetland/xkb)
(use janetland/util)


(defn remove-element [arr e]
  (for idx 0 (length arr)
    (when (= (in arr idx) e)
      (array/remove arr idx)
      (break))))


(defn contains? [arr e]
  (any? (map (fn [ee] (= e ee)) arr)))


(defn reset-cursor-mode [server]
  (put server :cursor-mode :passthrough)
  (put server :grabbed-view nil))


(defn desktop-view-at [server x y]
  (def [node sx sy] (wlr-scene-node-at (((server :scene) :tree) :node) x y))
  (when (or (nil? node) (not (= (node :type) :buffer)))
    (break [nil nil 0 0]))

  (def scene-buffer (wlr-scene-buffer-from-node node))
  (def scene-surface (wlr-scene-surface-from-buffer scene-buffer))
  (when (nil? scene-surface)
    (break [nil nil 0 0]))

  (def surface (scene-surface :surface))
  (var tree (node :parent))
  (while (and (not (nil? tree)) (nil? ((tree :node) :data)))
    (set tree ((tree :node) :parent)))
  (def view (pointer-to-table ((tree :node) :data)))

  (wlr-log :debug "#### desktop-view-at #### view = %p" view)
  [view surface sx sy])


(defn focus-view [view surface]
  (when (nil? view) (break))

  (def server (view :server))
  (def seat (server :seat))
  (def prev-surface ((seat :keyboard-state) :focused-surface))

  (when (= prev-surface surface) (break))

  (when (not (nil? prev-surface))
    (def previous (wlr-xdg-surface-from-wlr-surface prev-surface))
    (wlr-log :debug "(previous :role) = %p" (previous :role))
    (wlr-xdg-toplevel-set-activated (previous :toplevel) false))

  (def keyboard (wlr-seat-get-keyboard seat))
  (wlr-scene-node-raise-to-top ((view :scene-tree) :node))

  # Move the view to the front of the list
  (remove-element (server :views) view)
  (array/push (server :views) view)
  (wlr-log :debug "(length (server :views)) = %p" (length (server :views)))

  (wlr-xdg-toplevel-set-activated (view :xdg-toplevel) true)

  (when (not (nil? keyboard))
    (wlr-seat-keyboard-notify-enter seat (((view :xdg-toplevel) :base) :surface)
                                    (keyboard :keycodes) (keyboard :modifiers))))


(defn begin-interactive [view mode edges]
  # TODO
  )


(defn process-cursor-move [server time]
  #TODO
  )


(defn process-cursor-resize [server time]
  #TODO
  )


(defn process-cursor-motion [server time]
  (when (= (server :cursor-mode) :move)
    (process-cursor-move server time)
    (break))
  (when (= (server :cursor-mode) :resize)
    (process-cursor-resize server time)
    (break))

  (def [view surface sx sy]
    (desktop-view-at server ((server :cursor) :x) ((server :cursor) :y)))

  (when (nil? view)
    (wlr-xcursor-manager-set-cursor-image (server :xcursor-manager) "left_ptr" (server :cursor)))

  (if (nil? surface)
    (wlr-seat-pointer-clear-focus (server :seat))
    (do
      (wlr-seat-pointer-notify-enter (server :seat) surface sx sy)
      (wlr-seat-pointer-notify-motion (server :seat) time sx sy))))


(defn handle-wlr-keyboard-modifiers [keyboard listener data]
  (def seat ((keyboard :server) :seat))
  (def wlr-keyboard (keyboard :wlr-keyboard))

  (wlr-log :debug "#### handle-wlr-keyboard-modifiers ####")
  (wlr-log :debug "#### ((wlr-keyboard :modifiers) :depressed) = %p ####" ((wlr-keyboard :modifiers) :depressed))

  (wlr-seat-set-keyboard seat wlr-keyboard)
  (wlr-seat-keyboard-notify-modifiers seat (wlr-keyboard :modifiers)))


(defn handle-keybinding [server sym]
  (wlr-log :debug "#### handle-keybinding #### sym = %p" sym)
  (case sym
    (int/u64 0xff1b) # XKB_KEY_Escape
    (do
      (wl-display-terminate (server :display))
      true)

    (int/u64 0xffbe) # XKB_KEY_F1
    (do
      (when (> (length (server :views)) 1)
        (def next-view ((server :views) 0))
        (focus-view next-view (((next-view :xdg-toplevel) :base) :surface)))
      true)

    # default
    false))


(defn handle-wlr-keyboard-key [keyboard listener data]
  (def server (keyboard :server))
  (def seat (server :seat))
  (def event (get-abstract-listener-data data 'wlr/wlr-keyboard-key-event))

  (wlr-log :debug "#### handle-wlr-keyboard-key ####")
  (wlr-log :debug "#### (event :time-msec) = %p" (event :time-msec))
  (wlr-log :debug "#### (event :keycode) = %p" (event :keycode))
  (wlr-log :debug "#### (event :update-state) = %p" (event :update-state))
  (wlr-log :debug "#### (event :state) = %p" (event :state))

  (def keycode (+ (event :keycode) 8))
  (def syms (xkb-state-key-get-syms ((keyboard :wlr-keyboard) :xkb-state) keycode))
  (def modifiers (wlr-keyboard-get-modifiers (keyboard :wlr-keyboard)))

  (wlr-log :debug "#### syms = %p" syms)
  (wlr-log :debug "#### modifiers = %p" modifiers)

  (def handled-syms
    (if (and (contains? modifiers :alt) (= (event :state) :pressed))
      (map (fn [sym] (handle-keybinding server sym)) syms)
      (map (fn [_] false) syms)))

  (wlr-log :debug "#### handled-syms = %p" handled-syms)
  (wlr-log :debug "#### (not (any? handled-syms)) = %p" (not (any? handled-syms)))

  (when (not (any? handled-syms))
    (wlr-seat-set-keyboard seat (keyboard :wlr-keyboard))
    (wlr-seat-keyboard-notify-key seat (event :time-msec) (event :keycode) (event :state))))


(defn handle-wlr-input-device-destroy [keyboard listener data]
  (wl-signal-remove (keyboard :wlr-keyboard-modifiers-listener))
  (wl-signal-remove (keyboard :wlr-keyboard-key-listener))
  (wl-signal-remove (keyboard :wlr-input-device-destroy-listener))
  (remove-element ((keyboard :server) :keyboards) keyboard))


(defn server-new-keyboard [server device]
  (def wlr-keyboard (wlr-keyboard-from-input-device device))

  (wlr-log :debug "#### server-new-keyboard ####")

  (def keyboard @{})
  (put keyboard :server server)
  (put keyboard :wlr-keyboard wlr-keyboard)

  (def context (xkb-context-new :no-flags))
  (def keymap (xkb-keymap-new-from-names context nil :no-flags))

  (wlr-keyboard-set-keymap wlr-keyboard keymap)
  (xkb-keymap-unref keymap)
  (xkb-context-unref context)
  (wlr-keyboard-set-repeat-info wlr-keyboard 25 600)

  (wlr-log :debug "#### (wlr-keyboard :keymap-string) = %p" (wlr-keyboard :keymap-string))
  (wlr-log :debug "#### (wlr-keyboard :xkb-state) = %p" (wlr-keyboard :xkb-state))

  (put keyboard :wlr-keyboard-modifiers-listener
     (wl-signal-add (wlr-keyboard :events.modifiers)
                    (fn [listener data]
                      (handle-wlr-keyboard-modifiers keyboard listener data))))
  (put keyboard :wlr-keyboard-key-listener
     (wl-signal-add (wlr-keyboard :events.key)
                    (fn [listener data]
                      (handle-wlr-keyboard-key keyboard listener data))))
  (put keyboard :wlr-input-device-destroy-listener
     (wl-signal-add (device :events.destroy)
                    (fn [listener data]
                      (handle-wlr-input-device-destroy keyboard listener data))))

  (wlr-seat-set-keyboard (server :seat) wlr-keyboard)
  (array/push (server :keyboards) keyboard))


(defn server-new-pointer [server device]
  (wlr-cursor-attach-input-device (server :cursor) device))


(defn handle-wlr-output-frame [server wlr-output listener data]
  (wlr-log :debug "#### handle-wlr-output-frame ####")
  (def scene-output (wlr-scene-get-scene-output (server :scene) wlr-output))
  (wlr-scene-output-commit scene-output)
  (wlr-scene-output-send-frame-done scene-output (clock-gettime :monotonic)))


(defn handle-wlr-output-destroy [server output listener data]
  (wlr-log :debug "#### handle-wlr-output-destroy ####")
  (wl-signal-remove (output :wlr-output-frame-listener))
  (wl-signal-remove (output :wlr-output-destroy-listener))
  (remove-element (server :outputs) output))


(defn handle-backend-new-output [server listener data]
  "Fired when a new output is available."

  (def wlr-output (get-abstract-listener-data data 'wlr/wlr-output))

  (wlr-log :debug "#### handle-backend-new-output #### wlr-output = %p" wlr-output)

  (wlr-log :debug "#### (wlr-output-init-render wlr-output allocator renderer) = %p"
           (wlr-output-init-render wlr-output (server :allocator) (server :renderer)))

  (when (not (wl-list-empty (wlr-output :modes)))
    (def mode (wlr-output-preferred-mode wlr-output))
    (wlr-output-set-mode wlr-output mode)
    (wlr-output-enable wlr-output true)
    (when (not (wlr-output-commit wlr-output))
      (break)))

  (def output
    @{:wlr-output wlr-output
      :server server})

  (put output :wlr-output-frame-listener
     (wl-signal-add (wlr-output :events.frame)
                    (fn [listener data]
                      (handle-wlr-output-frame server wlr-output listener data))))

  (put output :wlr-output-destroy-listener
     (wl-signal-add (wlr-output :events.destroy)
                    (fn [listener data]
                      (handle-wlr-output-destroy server output listener data))))

  (array/push (server :outputs) output)
  (wlr-log :debug "(length (server :outputs)) = %v" (length (server :outputs)))

  (wlr-output-layout-add-auto (server :output-layout) wlr-output))


(defn handle-backend-new-input [server listener data]
  (def device (get-abstract-listener-data data 'wlr/wlr-input-device))

  (wlr-log :debug "#### handle-backend-new-input #### data = %p" device)
  (wlr-log :debug "#### (device :type) = %p" (device :type))

  (case (device :type)
    :keyboard (server-new-keyboard server device)
    :pointer (server-new-pointer server device))

  (def caps @[:pointer])
  (when (not (empty? (server :keyboards)))
    (array/push caps :keyboard))
  (wlr-seat-set-capabilities (server :seat) caps))


(defn handle-seat-request-set-cursor [server listener data]
  (def event (get-abstract-listener-data data 'wlr/wlr-seat-pointer-request-set-cursor-event))

  (wlr-log :debug "#### handle-seat-request-set-cursor #### data = %p" event)

  (def focused-client (((server :seat) :pointer-state) :focused-client))
  (when (= focused-client (event :seat-client))
    (wlr-log :debug "#### setting cursor surface")
    (wlr-cursor-set-surface (server :cursor) (event :surface) (event :hotspot-x) (event :hotspot-y))))


(defn handle-seat-request-set-selection [server listener data]
  (def event (get-abstract-listener-data data 'wlr/wlr-seat-request-set-selection-event))
  (wlr-seat-set-selection (server :seat) (event :source) (event :serial)))


(defn handle-xdg-surface-map [view listener data]
  (wlr-log :debug "#### handle-xdg-surface-map ####")
  (array/push ((view :server) :views) view)
  (wlr-log :debug "#### (length ((view :server) :views)) = %v" (length ((view :server) :views)))
  (focus-view view (((view :xdg-toplevel) :base) :surface)))


(defn handle-xdg-surface-unmap [view listener data]
  (wlr-log :debug "#### handle-xdg-surface-unmap ####")
  (when (= view ((view :server) :grabbed-view))
    (reset-cursor-mode (view :server)))
  (remove-element ((view :server) :views) view)
  (wlr-log :debug "#### (length ((view :server) :views)) = %v" (length ((view :server) :views))))


(defn handle-xdg-surface-destroy [view listener data]
  (wlr-log :debug "#### handle-xdg-surface-destroy ####")
  (wl-signal-remove (view :xdg-surface-map-listener))
  (wl-signal-remove (view :xdg-surface-unmap-listener))
  (wl-signal-remove (view :xdg-surface-destroy-listener))
  (wl-signal-remove (view :xdg-toplevel-request-move-listener))
  (wl-signal-remove (view :xdg-toplevel-request-resize-listener))
  (wl-signal-remove (view :xdg-toplevel-request-maximize-listener))
  (wl-signal-remove (view :xdg-toplevel-request-fullscreen-listener)))


(defn handle-xdg-toplevel-request-move [view listener data]
  (wlr-log :debug "#### handle-xdg-toplevel-request-move ####")
  (begin-interactive view :move 0))


(defn handle-xdg-toplevel-request-resize [view listener data]
  (wlr-log :debug "#### handle-xdg-toplevel-request-resize ####")
  (def event (get-abstract-listener-data data 'wlr/wlr-xdg-toplevel-resize-event))
  (begin-interactive view :resize (event :edges)))


(defn handle-xdg-toplevel-request-maximize [view listener data]
  (wlr-log :debug "#### handle-xdg-toplevel-request-maximize ####")
  (wlr-xdg-surface-schedule-configure ((view :xdg-toplevel) :base)))


(defn handle-xdg-toplevel-request-fullscreen [view listener data]
  (wlr-log :debug "#### handle-xdg-toplevel-request-fullscreen ####")
  (wlr-xdg-surface-schedule-configure ((view :xdg-toplevel) :base)))


(defn handle-xdg-shell-new-surface [server listener data]
  "Fired when a new surface appears."

  (def xdg-surface (get-abstract-listener-data data 'wlr/wlr-xdg-surface))

  (wlr-log :debug "#### handle-xdg-shell-new-surface #### data = %p" xdg-surface)
  (wlr-log :debug "#### (xdg-surface :role) = %p" (xdg-surface :role))

  (when (= (xdg-surface :role) :popup)
    (def parent (wlr-xdg-surface-from-wlr-surface ((xdg-surface :popup) :parent)))
    (def parent-tree (pointer-to-abstract-object (parent :data) 'wlr/wlr-scene-tree))
    (set (xdg-surface :data) (wlr-scene-xdg-surface-create parent-tree xdg-surface))
    (break))

  (def view @{:server server
              :xdg-toplevel (xdg-surface :toplevel)
              :scene-tree (wlr-scene-xdg-surface-create ((server :scene) :tree)
                                                        ((xdg-surface :toplevel) :base))})

  (wlr-log :debug "#### (view :scene-tree) = %p" (view :scene-tree))
  (set (((view :scene-tree) :node) :data) view)
  (set (xdg-surface :data) (view :scene-tree))

  (put view :xdg-surface-map-listener
     (wl-signal-add (xdg-surface :events.map)
                    (fn [listener data]
                      (handle-xdg-surface-map view listener data))))
  (put view :xdg-surface-unmap-listener
     (wl-signal-add (xdg-surface :events.unmap)
                    (fn [listener data]
                      (handle-xdg-surface-unmap view listener data))))
  (put view :xdg-surface-destroy-listener
     (wl-signal-add (xdg-surface :events.destroy)
                    (fn [listener data]
                      (handle-xdg-surface-destroy view listener data))))

  (def toplevel (xdg-surface :toplevel))

  (put view :xdg-toplevel-request-move-listener
     (wl-signal-add (toplevel :events.request_move)
                    (fn [listener data]
                      (handle-xdg-toplevel-request-move view listener data))))
  (put view :xdg-toplevel-request-resize-listener
     (wl-signal-add (toplevel :events.request_resize)
                    (fn [listener data]
                      (handle-xdg-toplevel-request-resize view listener data))))
  (put view :xdg-toplevel-request-maximize-listener
     (wl-signal-add (toplevel :events.request_maximize)
                    (fn [listener data]
                      (handle-xdg-toplevel-request-maximize view listener data))))
  (put view :xdg-toplevel-request-fullscreen-listener
     (wl-signal-add (toplevel :events.request_fullscreen)
                    (fn [listener data]
                      (handle-xdg-toplevel-request-fullscreen view listener data)))))


(defn handle-cursor-motion [server listener data]
  (def event (get-abstract-listener-data data 'wlr/wlr-pointer-motion-event))

  (wlr-log :debug "#### handle-cursor-motion #### data = %p" event)
  (wlr-log :debug "#### (event :time-msec) = %p" (event :time-msec))
  (wlr-log :debug "#### (event :delta-x) = %p" (event :delta-x))
  (wlr-log :debug "#### (event :delta-y) = %p" (event :delta-y))

  (wlr-cursor-move (server :cursor) ((event :pointer) :base) (event :delta-x) (event :delta-y))
  (process-cursor-motion server (event :time-msec)))


(defn handle-cursor-motion-absolute [server listener data]
  (def event (get-abstract-listener-data data 'wlr/wlr-pointer-motion-absolute-event))

  (wlr-log :debug "#### handle-cursor-motion-absolute #### data = %p" event)
  (wlr-log :debug "#### (event :time-msec) = %p" (event :time-msec))
  (wlr-log :debug "#### (event :x) = %p" (event :x))
  (wlr-log :debug "#### (event :y) = %p" (event :y))

  (wlr-cursor-warp-absolute (server :cursor) ((event :pointer) :base) (event :x) (event :y))
  (process-cursor-motion server (event :time-msec)))


(defn handle-cursor-button [server listener data]
  (def event (get-abstract-listener-data data 'wlr/wlr-pointer-button-event))

  (wlr-log :debug "#### handle-cursor-button #### data = %p" event)
  (wlr-log :debug "#### (event :time-msec) = %p" (event :time-msec))
  (wlr-log :debug "#### (event :button) = %p" (event :button))
  (wlr-log :debug "#### (event :state) = %p" (event :state))

  (wlr-seat-pointer-notify-button (server :seat)
                                  (event :time-msec)
                                  (event :button)
                                  (event :state))
  (if (= (event :state) :released)
    (reset-cursor-mode server)
    (let [[view surface _sx _sy] (desktop-view-at server ((server :cursor) :x) ((server :cursor) :y))]
      (focus-view view surface))))


(defn handle-cursor-axis [server listener data]
  (def event (get-abstract-listener-data data 'wlr/wlr-pointer-axis-event))

  (wlr-log :debug "#### handle-cursor-axis #### data = %p" event)
  (wlr-log :debug "#### (event :time-msec) = %p" (event :time-msec))
  (wlr-log :debug "#### (event :source) = %p" (event :source))
  (wlr-log :debug "#### (event :orientation) = %p" (event :orientation))
  (wlr-log :debug "#### (event :delta) = %p" (event :delta))
  (wlr-log :debug "#### (event :delta-discrete) = %p" (event :delta-discrete))

  (wlr-seat-pointer-notify-axis (server :seat)
                                (event :time-msec)
                                (event :orientation)
                                (event :delta)
                                (event :delta-discrete)
                                (event :source)))


(defn handle-cursor-frame [server listener data]
  (wlr-log :debug "#### handle-cursor-frame ####")
  (wlr-seat-pointer-notify-frame (server :seat)))


(defn main [& argv]
  (wlr-log-init :debug)

  (def server @{})

  (put server :display (wl-display-create))
  (put server :backend (wlr-backend-autocreate (server :display)))
  (put server :renderer (wlr-renderer-autocreate (server :backend)))

  (wlr-log :debug "#### (wlr-renderer-init-wl-display renderer display) = %p"
           (wlr-renderer-init-wl-display (server :renderer) (server :display)))

  (put server :allocator (wlr-allocator-autocreate (server :backend) (server :renderer)))
  (put server :compositor (wlr-compositor-create (server :display) (server :renderer)))
  (put server :subcompositor (wlr-subcompositor-create (server :display)))
  (put server :data-device-manager (wlr-data-device-manager-create (server :display)))
  (put server :output-layout (wlr-output-layout-create))

  (put server :outputs @[])
  (put server :backend-new-output-listener
     (wl-signal-add ((server :backend) :events.new_output)
                    (fn [listener data]
                      (handle-backend-new-output server listener data))))

  (put server :scene (wlr-scene-create))

  (wlr-log :debug "#### (wlr-scene-attach-output-layout scene output-layout) = %p"
           (wlr-scene-attach-output-layout (server :scene) (server :output-layout)))

  (put server :xdg-shell (wlr-xdg-shell-create (server :display) 3))

  (put server :views @[])
  (put server :xdg-shell-new-surface-listener
    (wl-signal-add ((server :xdg-shell) :events.new_surface)
                   (fn [listener data]
                     (handle-xdg-shell-new-surface server listener data))))

  (put server :cursor (wlr-cursor-create))

  (wlr-cursor-attach-output-layout (server :cursor) (server :output-layout))
 
  (put server :xcursor-manager (wlr-xcursor-manager-create nil 24))

  (wlr-log :debug "#### (wlr-xcursor-manager-load xcursor-manager 1) = %p"
           (wlr-xcursor-manager-load (server :xcursor-manager) 1))

  (put server :cursor-mode :passthrough)

  (put server :cursor-motion-listener
     (wl-signal-add ((server :cursor) :events.motion)
                    (fn [listener data]
                      (handle-cursor-motion server listener data))))
  (put server :cursor-motion-absolute-listener
     (wl-signal-add ((server :cursor) :events.motion_absolute)
                    (fn [listener data]
                      (handle-cursor-motion-absolute server listener data))))
  (put server :cursor-button-listener
     (wl-signal-add ((server :cursor) :events.button)
                    (fn [listener data]
                      (handle-cursor-button server listener data))))
  (put server :cursor-axis-listener
     (wl-signal-add ((server :cursor) :events.axis)
                    (fn [listener data]
                      (handle-cursor-axis server listener data))))
  (put server :cursor-frame-listener
     (wl-signal-add ((server :cursor) :events.frame)
                    (fn [listener data]
                      (handle-cursor-frame server listener data))))

  (put server :keyboards @[])
  (put server :backend-new-input-listener
     (wl-signal-add ((server :backend) :events.new_input)
                    (fn [listener data]
                      (handle-backend-new-input server listener data))))

  (put server :seat (wlr-seat-create (server :display) "seat0"))

  (put server :seat-request-set-cursor-listener
     (wl-signal-add ((server :seat) :events.request_set_cursor)
                    (fn [listener data]
                      (handle-seat-request-set-cursor server listener data))))
  (put server :seat-request-set-selection-listener
     (wl-signal-add ((server :seat) :events.request_set_selection)
                    (fn [listener data]
                      (handle-seat-request-set-selection server listener data))))

  (put server :socket (wl-display-add-socket-auto (server :display)))

  (when (not (wlr-backend-start (server :backend)))
    (wlr-log :debug "#### failed to start backend")
    (wlr-backend-destroy (server :backend))
    (wl-display-destroy (server :display))
    (break))

  (os/setenv "WAYLAND_DISPLAY" (server :socket))
  (when (> (length argv) 1)
    (wlr-log :debug "#### running command: %p" (slice argv 1))
    (os/spawn ["/bin/sh" "-c" ;(slice argv 1)]))

  (wlr-log :info "#### running on WAYLAND_DISPLAY=%s" (server :socket))
  (wl-display-run (server :display))

  (wl-display-destroy-clients (server :display))
  (wl-display-destroy (server :display)))
