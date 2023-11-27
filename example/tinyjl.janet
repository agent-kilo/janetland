(use janetland/wl)
(use janetland/wlr)
(use janetland/util)


(defn remove-element [arr e]
  (for idx 0 (length arr)
    (when (= (in arr idx) e)
      (array/remove arr idx)
      (break))))


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
  (wlr-log :debug "#### handle-backend-new-input #### data = %p"
           (get-abstract-listener-data data 'wlr/wlr-input-device)))


(defn handle-xdg-surface-map [view listener data]
  (wlr-log :debug "#### handle-xdg-surface-map ####")
  (array/push ((view :server) :views) view)
  (wlr-log :debug "#### (length ((view :server) :views)) = %v" (length ((view :server) :views)))
  # TODO
  #(focus-view view (((view :xdg-toplevel) :base) :surface))
  )


(defn handle-xdg-surface-unmap [view listener data]
  (wlr-log :debug "#### handle-xdg-surface-unmap ####")
  (remove-element ((view :server) :views) view)
  # TODO: grabbed cursor
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


(defn handle-xdg-toplevel-request-move [server listener data]
  (wlr-log :debug "#### handle-xdg-toplevel-request-move ####")
  )


(defn handle-xdg-toplevel-request-resize [server listener data]
  (wlr-log :debug "#### handle-xdg-toplevel-request-resize ####")
  )


(defn handle-xdg-toplevel-request-maximize [server listener data]
  (wlr-log :debug "#### handle-xdg-toplevel-request-maximize ####")
  )


(defn handle-xdg-toplevel-request-fullscreen [server listener data]
  (wlr-log :debug "#### handle-xdg-toplevel-request-fullscreen ####")
  )


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
  # XXX: not working yet
  #(set (((view :scene-tree) :node) :data) view)
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
                      (handle-xdg-toplevel-request-move server listener data))))
  (put view :xdg-toplevel-request-resize-listener
     (wl-signal-add (toplevel :events.request_resize)
                    (fn [listener data]
                      (handle-xdg-toplevel-request-resize server listener data))))
  (put view :xdg-toplevel-request-maximize-listener
     (wl-signal-add (toplevel :events.request_maximize)
                    (fn [listener data]
                      (handle-xdg-toplevel-request-maximize server listener data))))
  (put view :xdg-toplevel-request-fullscreen-listener
     (wl-signal-add (toplevel :events.request_fullscreen)
                    (fn [listener data]
                      (handle-xdg-toplevel-request-fullscreen server listener data)))))


(defn main [&]
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

  (put server :backend-new-input-listener
     (wl-signal-add ((server :backend) :events.new_input)
                    (fn [listener data]
                      (handle-backend-new-input server listener data))))

  (put server :seat (wlr-seat-create (server :display) "seat0"))

  (put server :socket (wl-display-add-socket-auto (server :display)))

  (when (not (wlr-backend-start (server :backend)))
    (wlr-log :debug "#### failed to start backend")
    (wlr-backend-destroy (server :backend))
    (wl-display-destroy (server :display))
    (break))

  (os/setenv "WAYLAND_DISPLAY" (server :socket))

  (wlr-log :info "#### running on WAYLAND_DISPLAY=%s" (server :socket))
  (wl-display-run (server :display))

  (wl-display-destroy-clients (server :display))
  (wl-display-destroy (server :display)))