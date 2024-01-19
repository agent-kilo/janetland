#ifndef __WLR_ABS_TYPES_H__
#define __WLR_ABS_TYPES_H__

#ifndef MOD_NAME
#define MOD_NAME WLR_MOD_NAME
#endif

#define JWLR_OFFSET_DEF(struct_type, member) \
    {#member, (uint64_t)&(((struct_type *)NULL)->member)}


static void method_wlr_abs_obj_tostring(void *p, JanetBuffer *buf);
static int method_wlr_abs_obj_compare(void *lhs, void *rhs);


static int method_box_get(void *p, Janet key, Janet *out);
static void method_box_put(void *p, Janet key, Janet value);
static const JanetAbstractType jwlr_at_box = {
    .name = MOD_NAME "/box",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_box_get,
    .put = method_box_put,
    JANET_ATEND_PUT
};


static const jl_offset_def_t wlr_backend_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_backend, events.destroy),
    JWLR_OFFSET_DEF(struct wlr_backend, events.new_input),
    JWLR_OFFSET_DEF(struct wlr_backend, events.new_output),
    {NULL, 0},
};

static int method_wlr_backend_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_backend = {
    .name = MOD_NAME "/wlr-backend",
    .gc = NULL, /* TODO: close the backend? */
    .gcmark = NULL,
    .get = method_wlr_backend_get,
    JANET_ATEND_GET
};


static const JanetAbstractType jwlr_at_wlr_renderer = {
    .name = MOD_NAME "/wlr-renderer",
    .gc = NULL, /* TODO: close the renderer? */
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static const JanetAbstractType jwlr_at_wlr_allocator = {
    .name = MOD_NAME "/wlr-allocator",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static const JanetAbstractType jwlr_at_wlr_compositor = {
    .name = MOD_NAME "/wlr-compositor",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static const JanetAbstractType jwlr_at_wlr_subcompositor = {
    .name = MOD_NAME "/wlr-subcompositor",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static const JanetAbstractType jwlr_at_wlr_data_device_manager = {
    .name = MOD_NAME "/wlr-data-device-manager",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static const jl_key_def_t wlr_direction_defs[] = {
    {"up", WLR_DIRECTION_UP},
    {"down", WLR_DIRECTION_DOWN},
    {"left", WLR_DIRECTION_LEFT},
    {"right", WLR_DIRECTION_RIGHT},
    {NULL, 0},
};

static const jl_offset_def_t wlr_output_layout_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_output_layout, events.add),
    JWLR_OFFSET_DEF(struct wlr_output_layout, events.change),
    JWLR_OFFSET_DEF(struct wlr_output_layout, events.destroy),
    {NULL, 0},
};

static const jl_offset_def_t wlr_output_layout_list_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_output_layout, outputs),
    {NULL, 0},
};

static int method_wlr_output_layout_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_output_layout = {
    .name = MOD_NAME "/wlr-output-layout",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_output_layout_get,
    JANET_ATEND_GET
};


static const JanetAbstractType jwlr_at_wlr_output_layout_output = {
    .name = MOD_NAME "/wlr-output-layout-output",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static const jl_offset_def_t wlr_scene_list_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_scene, outputs),
};

static int method_wlr_scene_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_scene = {
    .name = MOD_NAME "/wlr-scene",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_scene_get,
    JANET_ATEND_GET
};


static const jl_offset_def_t wlr_scene_output_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_scene_output, events.destroy),
    {NULL, 0},
};

static int method_wlr_scene_output_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_scene_output = {
    .name = MOD_NAME "/wlr-scene-output",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_scene_output_get,
    JANET_ATEND_GET
};


static const jl_offset_def_t wlr_xdg_shell_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_xdg_shell, events.new_surface),
    JWLR_OFFSET_DEF(struct wlr_xdg_shell, events.destroy),
    {NULL, 0},
};

static int method_wlr_xdg_shell_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xdg_shell = {
    .name = MOD_NAME "/wlr-xdg-shell",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xdg_shell_get,
    JANET_ATEND_GET
};


static const jl_offset_def_t wlr_surface_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_surface, events.client_commit),
    JWLR_OFFSET_DEF(struct wlr_surface, events.commit),
    JWLR_OFFSET_DEF(struct wlr_surface, events.new_subsurface),
    JWLR_OFFSET_DEF(struct wlr_surface, events.destroy),
    {NULL, 0},
};

static int method_wlr_surface_get(void *p, Janet key, Janet *out);
static void method_wlr_surface_put(void *p, Janet key, Janet value);
static const JanetAbstractType jwlr_at_wlr_surface = {
    .name = MOD_NAME "/wlr-surface",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_surface_get,
    .put = method_wlr_surface_put,
    .marshal = NULL,
    .unmarshal = NULL,
    .tostring = method_wlr_abs_obj_tostring,
    .compare = method_wlr_abs_obj_compare,
    JANET_ATEND_COMPARE
};


static const jl_key_def_t wlr_surface_state_field_defs[] = {
    {"buffer", WLR_SURFACE_STATE_BUFFER},
    {"surface-damage", WLR_SURFACE_STATE_SURFACE_DAMAGE},
    {"buffer-damage", WLR_SURFACE_STATE_BUFFER_DAMAGE},
    {"opaque-region", WLR_SURFACE_STATE_OPAQUE_REGION},
    {"input-region", WLR_SURFACE_STATE_INPUT_REGION},
    {"transform", WLR_SURFACE_STATE_TRANSFORM},
    {"scale", WLR_SURFACE_STATE_SCALE},
    {"frame-callback-list", WLR_SURFACE_STATE_FRAME_CALLBACK_LIST},
    {"viewport", WLR_SURFACE_STATE_VIEWPORT},
    {"offset", WLR_SURFACE_STATE_OFFSET},
    {NULL, 0},
};

static int method_wlr_surface_state_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_surface_state = {
    .name = MOD_NAME "/wlr-surface-state",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_surface_state_get,
    JANET_ATEND_GET
};


static const jl_offset_def_t wlr_xdg_popup_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_xdg_popup, events.reposition),
    {NULL, 0},
};

static int method_wlr_xdg_popup_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xdg_popup = {
    .name = MOD_NAME "/wlr-xdg-popup",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xdg_popup_get,
    JANET_ATEND_GET
};


static const jl_offset_def_t wlr_xdg_toplevel_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_xdg_toplevel, events.request_maximize),
    JWLR_OFFSET_DEF(struct wlr_xdg_toplevel, events.request_fullscreen),
    JWLR_OFFSET_DEF(struct wlr_xdg_toplevel, events.request_minimize),
    JWLR_OFFSET_DEF(struct wlr_xdg_toplevel, events.request_move),
    JWLR_OFFSET_DEF(struct wlr_xdg_toplevel, events.request_resize),
    JWLR_OFFSET_DEF(struct wlr_xdg_toplevel, events.request_show_window_menu),
    JWLR_OFFSET_DEF(struct wlr_xdg_toplevel, events.set_parent),
    JWLR_OFFSET_DEF(struct wlr_xdg_toplevel, events.set_title),
    JWLR_OFFSET_DEF(struct wlr_xdg_toplevel, events.set_app_id),
    {NULL, 0},
};

static int method_wlr_xdg_toplevel_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xdg_toplevel = {
    .name = MOD_NAME "/wlr-xdg-toplevel",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xdg_toplevel_get,
    JANET_ATEND_GET
};


static const jl_key_def_t wlr_edges_defs[] = {
    {"none", WLR_EDGE_NONE},
    {"top", WLR_EDGE_TOP},
    {"bottom", WLR_EDGE_BOTTOM},
    {"left", WLR_EDGE_LEFT},
    {"right", WLR_EDGE_RIGHT},
    {NULL, 0},
};

static int method_wlr_xdg_toplevel_resize_event_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xdg_toplevel_resize_event = {
    .name = MOD_NAME "/wlr-xdg-toplevel-resize-event",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xdg_toplevel_resize_event_get,
    JANET_ATEND_GET
};


static int method_wlr_xdg_toplevel_move_event_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xdg_toplevel_move_event = {
    .name = MOD_NAME "/wlr-xdg-toplevel-move-event",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xdg_toplevel_move_event_get,
    JANET_ATEND_GET
};


static const jl_offset_def_t wlr_xdg_surface_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_xdg_surface, events.destroy),
    JWLR_OFFSET_DEF(struct wlr_xdg_surface, events.ping_timeout),
    JWLR_OFFSET_DEF(struct wlr_xdg_surface, events.new_popup),
    JWLR_OFFSET_DEF(struct wlr_xdg_surface, events.map),
    JWLR_OFFSET_DEF(struct wlr_xdg_surface, events.unmap),
    JWLR_OFFSET_DEF(struct wlr_xdg_surface, events.configure),
    JWLR_OFFSET_DEF(struct wlr_xdg_surface, events.ack_configure),
    {NULL, 0},
};

static const jl_offset_def_t wlr_xdg_surface_list_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_xdg_surface, popups),
    JWLR_OFFSET_DEF(struct wlr_xdg_surface, configure_list),
};

static const jl_key_def_t wlr_xdg_surface_role_defs[] = {
    {"none", WLR_XDG_SURFACE_ROLE_NONE},
    {"toplevel", WLR_XDG_SURFACE_ROLE_TOPLEVEL},
    {"popup", WLR_XDG_SURFACE_ROLE_POPUP},
    {NULL, 0},
};
#define __WLR_XDG_SURFACE_ROLE_DEFS_COUNT JL_KEY_DEF_COUNT(wlr_xdg_surface_role_defs)

static int method_wlr_xdg_surface_get(void *p, Janet key, Janet *out);
static void method_wlr_xdg_surface_put(void *p, Janet key, Janet value);
static const JanetAbstractType jwlr_at_wlr_xdg_surface = {
    .name = MOD_NAME "/wlr-xdg-surface",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xdg_surface_get,
    .put = method_wlr_xdg_surface_put,
    .marshal = NULL,
    .unmarshal = NULL,
    .tostring = method_wlr_abs_obj_tostring,
    .compare = method_wlr_abs_obj_compare,
    JANET_ATEND_COMPARE
};


static const jl_offset_def_t wlr_cursor_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_cursor, events.motion),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.motion_absolute),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.button),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.axis),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.frame),

    JWLR_OFFSET_DEF(struct wlr_cursor, events.swipe_begin),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.swipe_update),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.swipe_end),

    JWLR_OFFSET_DEF(struct wlr_cursor, events.pinch_begin),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.pinch_update),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.pinch_end),

    JWLR_OFFSET_DEF(struct wlr_cursor, events.hold_begin),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.hold_end),

    JWLR_OFFSET_DEF(struct wlr_cursor, events.touch_up),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.touch_down),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.touch_motion),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.touch_cancel),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.touch_frame),

    JWLR_OFFSET_DEF(struct wlr_cursor, events.tablet_tool_axis),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.tablet_tool_proximity),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.tablet_tool_tip),
    JWLR_OFFSET_DEF(struct wlr_cursor, events.tablet_tool_button),

    {NULL, 0},
};

static int method_wlr_cursor_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_cursor = {
    .name = MOD_NAME "/wlr-cursor",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_cursor_get,
    JANET_ATEND_GET
};


static const JanetAbstractType jwlr_at_wlr_xcursor_manager = {
    .name = MOD_NAME "/wlr-xcursor-manager",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static int method_wlr_xcursor_image_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xcursor_image = {
    .name = MOD_NAME "/wlr-xcursor-image",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xcursor_image_get,
    JANET_ATEND_GET
};


static int method_wlr_xcursor_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xcursor = {
    .name = MOD_NAME "/wlr-xcursor",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xcursor_get,
    JANET_ATEND_GET
};


static const jl_key_def_t wl_seat_capability_defs[] = {
    {"pointer", WL_SEAT_CAPABILITY_POINTER},
    {"keyboard", WL_SEAT_CAPABILITY_KEYBOARD},
    {"touch", WL_SEAT_CAPABILITY_TOUCH},
    {NULL, 0},
};

static const jl_offset_def_t wlr_seat_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_seat, events.pointer_grab_begin),
    JWLR_OFFSET_DEF(struct wlr_seat, events.pointer_grab_end),

    JWLR_OFFSET_DEF(struct wlr_seat, events.keyboard_grab_begin),
    JWLR_OFFSET_DEF(struct wlr_seat, events.keyboard_grab_end),

    JWLR_OFFSET_DEF(struct wlr_seat, events.touch_grab_begin),
    JWLR_OFFSET_DEF(struct wlr_seat, events.touch_grab_end),

    JWLR_OFFSET_DEF(struct wlr_seat, events.request_set_cursor),

    JWLR_OFFSET_DEF(struct wlr_seat, events.request_set_selection),
    JWLR_OFFSET_DEF(struct wlr_seat, events.set_selection),

    JWLR_OFFSET_DEF(struct wlr_seat, events.request_set_primary_selection),
    JWLR_OFFSET_DEF(struct wlr_seat, events.set_primary_selection),

    JWLR_OFFSET_DEF(struct wlr_seat, events.request_start_drag),
    JWLR_OFFSET_DEF(struct wlr_seat, events.start_drag),

    JWLR_OFFSET_DEF(struct wlr_seat, events.destroy),

    {NULL, 0},
};

static int method_wlr_seat_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_seat = {
    .name = MOD_NAME "/wlr-seat",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_seat_get,
    JANET_ATEND_GET
};


static const JanetAbstractType jwlr_at_wlr_seat_client = {
    .name = MOD_NAME "/wlr-seat-client",
    .gc = NULL,
    .gcmark = NULL,
    .get = NULL,
    .put = NULL,
    .marshal = NULL,
    .unmarshal = NULL,
    .tostring = NULL,
    .compare = method_wlr_abs_obj_compare,
    JANET_ATEND_COMPARE
};


static const jl_offset_def_t wlr_output_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_output, events.frame),
    JWLR_OFFSET_DEF(struct wlr_output, events.damage),
    JWLR_OFFSET_DEF(struct wlr_output, events.needs_frame),
    JWLR_OFFSET_DEF(struct wlr_output, events.precommit),
    JWLR_OFFSET_DEF(struct wlr_output, events.commit),
    JWLR_OFFSET_DEF(struct wlr_output, events.present),
    JWLR_OFFSET_DEF(struct wlr_output, events.bind),
    JWLR_OFFSET_DEF(struct wlr_output, events.enable),
    JWLR_OFFSET_DEF(struct wlr_output, events.mode),
    JWLR_OFFSET_DEF(struct wlr_output, events.description),
    JWLR_OFFSET_DEF(struct wlr_output, events.destroy),
    {NULL, 0},
};

static const jl_offset_def_t wlr_output_list_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_output, resources),
    JWLR_OFFSET_DEF(struct wlr_output, modes),
    JWLR_OFFSET_DEF(struct wlr_output, cursors),
    {NULL, 0},
};

static int method_wlr_output_get(void *p, Janet key, Janet *out);
static void method_wlr_output_put(void *p, Janet key, Janet value);
static const JanetAbstractType jwlr_at_wlr_output = {
    .name = MOD_NAME "/wlr-output",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_output_get,
    .put = method_wlr_output_put,
    JANET_ATEND_PUT
};


static const jl_key_def_t wlr_output_mode_aspect_ratio_defs[] = {
    {"none", WLR_OUTPUT_MODE_ASPECT_RATIO_NONE},
    {"4:3", WLR_OUTPUT_MODE_ASPECT_RATIO_4_3},
    {"16:9", WLR_OUTPUT_MODE_ASPECT_RATIO_16_9},
    {"64:27", WLR_OUTPUT_MODE_ASPECT_RATIO_64_27},
    {"256:135", WLR_OUTPUT_MODE_ASPECT_RATIO_256_135},
    {NULL, 0},
};
#define __WLR_OUTPUT_MODE_ASPECT_RATIO_DEFS_COUNT JL_KEY_DEF_COUNT(wlr_output_mode_aspect_ratio_defs)

static int method_wlr_output_mode_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_output_mode = {
    .name = MOD_NAME "/wlr-output-mode",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_output_mode_get,
    JANET_ATEND_GET
};


static const JanetAbstractType jwlr_at_wlr_output_cursor = {
    .name = MOD_NAME "/wlr-output-cursor",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static const jl_offset_def_t wlr_scene_tree_list_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_scene_tree, children),
    {NULL, 0},
};

static int method_wlr_scene_tree_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_scene_tree = {
    .name = MOD_NAME "/wlr-scene-tree",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_scene_tree_get,
    JANET_ATEND_GET
};


static const jl_key_def_t wlr_scene_node_type_defs[] = {
    {"tree", WLR_SCENE_NODE_TREE},
    {"rect", WLR_SCENE_NODE_RECT},
    {"buffer", WLR_SCENE_NODE_BUFFER},
    {NULL, 0},
};
#define __WLR_SCENE_NODE_TYPE_DEFS_COUNT JL_KEY_DEF_COUNT(wlr_scene_node_type_defs)

static const jl_offset_def_t wlr_scene_node_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_scene_node, events.destroy),
    {NULL, 0},
};

static int method_wlr_scene_node_get(void *p, Janet key, Janet *out);
static void method_wlr_scene_node_put(void *p, Janet key, Janet value);
static const JanetAbstractType jwlr_at_wlr_scene_node = {
    .name = MOD_NAME "/wlr-scene-node",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_scene_node_get,
    .put = method_wlr_scene_node_put,
    .marshal = NULL,
    .unmarshal = NULL,
    .tostring = method_wlr_abs_obj_tostring,
    JANET_ATEND_TOSTRING
};


static const JanetAbstractType jwlr_at_wlr_scene_buffer = {
    .name = MOD_NAME "/wlr-scene-buffer",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static int method_wlr_scene_surface_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_scene_surface = {
    .name = MOD_NAME "/wlr-scene-surface",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_scene_surface_get,
    JANET_ATEND_GET
};


static const jl_key_def_t wlr_input_device_defs[] = {
    {"keyboard", WLR_INPUT_DEVICE_KEYBOARD},
    {"pointer", WLR_INPUT_DEVICE_POINTER},
    {"touch", WLR_INPUT_DEVICE_TOUCH},
    {"tablet-tool", WLR_INPUT_DEVICE_TABLET_TOOL},
    {"tablet-pad", WLR_INPUT_DEVICE_TABLET_PAD},
    {"switch", WLR_INPUT_DEVICE_SWITCH},
    {NULL, 0},
};
#define __WLR_INPUT_DEVICE_DEFS_COUNT JL_KEY_DEF_COUNT(wlr_input_device_defs)

static const jl_offset_def_t wlr_input_device_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_input_device, events.destroy),
    {NULL, 0},
};

static int method_wlr_input_device_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_input_device = {
    .name = MOD_NAME "/wlr-input-device",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_input_device_get,
    .put = NULL,
    .marshal = NULL,
    .unmarshal = NULL,
    .tostring = method_wlr_abs_obj_tostring,
    .compare = method_wlr_abs_obj_compare,
    JANET_ATEND_COMPARE
};


static const jl_offset_def_t wlr_pointer_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_pointer, events.motion),
    JWLR_OFFSET_DEF(struct wlr_pointer, events.motion_absolute),
    JWLR_OFFSET_DEF(struct wlr_pointer, events.button),
    JWLR_OFFSET_DEF(struct wlr_pointer, events.axis),
    JWLR_OFFSET_DEF(struct wlr_pointer, events.frame),
    JWLR_OFFSET_DEF(struct wlr_pointer, events.swipe_begin),
    JWLR_OFFSET_DEF(struct wlr_pointer, events.swipe_update),
    JWLR_OFFSET_DEF(struct wlr_pointer, events.swipe_end),
    JWLR_OFFSET_DEF(struct wlr_pointer, events.pinch_begin),
    JWLR_OFFSET_DEF(struct wlr_pointer, events.pinch_update),
    JWLR_OFFSET_DEF(struct wlr_pointer, events.pinch_end),
    JWLR_OFFSET_DEF(struct wlr_pointer, events.hold_begin),
    JWLR_OFFSET_DEF(struct wlr_pointer, events.hold_end),
    {NULL, 0},
};

static int method_wlr_pointer_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_pointer = {
    .name = MOD_NAME "/wlr-pointer",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_pointer_get,
    JANET_ATEND_GET
};


static int method_wlr_pointer_motion_event_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_pointer_motion_event = {
    .name = MOD_NAME "/wlr-pointer-motion-event",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_pointer_motion_event_get,
    JANET_ATEND_GET
};


static int method_wlr_pointer_motion_absolute_event_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_pointer_motion_absolute_event = {
    .name = MOD_NAME "/wlr-pointer-motion-absolute-event",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_pointer_motion_absolute_event_get,
    JANET_ATEND_GET
};


static const jl_key_def_t wlr_button_state_defs[] = {
    {"released", WLR_BUTTON_RELEASED},
    {"pressed", WLR_BUTTON_PRESSED},
    {NULL, 0},
};
#define __WLR_BUTTON_STATE_DEFS_COUNT JL_KEY_DEF_COUNT(wlr_button_state_defs)

static int method_wlr_pointer_button_event_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_pointer_button_event = {
    .name = MOD_NAME "/wlr-pointer-button-event",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_pointer_button_event_get,
    JANET_ATEND_GET
};


static const jl_key_def_t wlr_axis_source_defs[] = {
    {"wheel", WLR_AXIS_SOURCE_WHEEL},
    {"finger", WLR_AXIS_SOURCE_FINGER},
    {"continuous", WLR_AXIS_SOURCE_CONTINUOUS},
    {"wheel-tilt", WLR_AXIS_SOURCE_WHEEL_TILT},
    {NULL, 0},
};
#define __WLR_AXIS_SOURCE_DEFS_COUNT JL_KEY_DEF_COUNT(wlr_axis_source_defs)

static const jl_key_def_t wlr_axis_orientation_defs[] = {
    {"vertical", WLR_AXIS_ORIENTATION_VERTICAL},
    {"horizontal", WLR_AXIS_ORIENTATION_HORIZONTAL},
    {NULL, 0},
};
#define __WLR_AXIS_ORIENTATION_DEFS_COUNT JL_KEY_DEF_COUNT(wlr_axis_orientation_defs)

static int method_wlr_pointer_axis_event_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_pointer_axis_event = {
    .name = MOD_NAME "/wlr-pointer-axis-event",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_pointer_axis_event_get,
    JANET_ATEND_GET
};


static const jl_offset_def_t wlr_seat_pointer_state_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_seat_pointer_state, events.focus_change),
    {NULL, 0},
};

static int method_wlr_seat_pointer_state_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_seat_pointer_state = {
    .name = MOD_NAME "/wlr-seat-pointer-state",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_seat_pointer_state_get,
    JANET_ATEND_GET
};


static const jl_offset_def_t wlr_seat_keyboard_state_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_seat_keyboard_state, events.focus_change),
    {NULL, 0},
};

static int method_wlr_seat_keyboard_state_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_seat_keyboard_state = {
    .name = MOD_NAME "/wlr-seat-keyboard-state",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_seat_keyboard_state_get,
    JANET_ATEND_GET
};


static int method_wlr_seat_pointer_request_set_cursor_event_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_seat_pointer_request_set_cursor_event = {
    .name = MOD_NAME "/wlr-seat-pointer-request-set-cursor-event",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_seat_pointer_request_set_cursor_event_get,
    JANET_ATEND_GET
};


static int method_wlr_seat_request_set_selection_event_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_seat_request_set_selection_event = {
    .name = MOD_NAME "/wlr-seat-request-set-selection-event",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_seat_request_set_selection_event_get,
    JANET_ATEND_GET
};


static const JanetAbstractType jwlr_at_wlr_data_source = {
    .name = MOD_NAME "/wlr-data-source",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static const jl_key_def_t wlr_keyboard_modifier_defs[] = {
    {"shift", WLR_MODIFIER_SHIFT},
    {"caps", WLR_MODIFIER_CAPS},
    {"ctrl", WLR_MODIFIER_CTRL},
    {"alt", WLR_MODIFIER_ALT},
    {"mod2", WLR_MODIFIER_MOD2},
    {"mod3", WLR_MODIFIER_MOD3},
    {"logo", WLR_MODIFIER_LOGO},
    {"mod5", WLR_MODIFIER_MOD5},
    {NULL, 0},
};

static int method_wlr_keyboard_modifiers_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_keyboard_modifiers = {
    .name = MOD_NAME "/wlr-keyboard-modifiers",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_keyboard_modifiers_get,
    JANET_ATEND_GET
};


static const jl_key_def_t wl_keyboard_key_state_defs[] = {
    {"released", WL_KEYBOARD_KEY_STATE_RELEASED},
    {"pressed", WL_KEYBOARD_KEY_STATE_PRESSED},
    {NULL, 0},
};

static int method_wlr_keyboard_key_event_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_keyboard_key_event = {
    .name = MOD_NAME "/wlr-keyboard-key-event",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_keyboard_key_event_get,
    JANET_ATEND_GET
};


static const jl_offset_def_t wlr_keyboard_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_keyboard, events.key),
    JWLR_OFFSET_DEF(struct wlr_keyboard, events.modifiers),
    JWLR_OFFSET_DEF(struct wlr_keyboard, events.keymap),
    JWLR_OFFSET_DEF(struct wlr_keyboard, events.repeat_info),
    {NULL, 0},
};

static int method_wlr_keyboard_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_keyboard = {
    .name = MOD_NAME "/wlr-keyboard",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_keyboard_get,
    JANET_ATEND_GET
};


static const jl_offset_def_t wlr_xwayland_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_xwayland, events.ready),
    JWLR_OFFSET_DEF(struct wlr_xwayland, events.new_surface),
    JWLR_OFFSET_DEF(struct wlr_xwayland, events.remove_startup_info),
    {NULL, 0},
};

static int method_wlr_xwayland_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xwayland = {
    .name = MOD_NAME "/wlr-xwayland",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xwayland_get,
    JANET_ATEND_GET
};


static const jl_offset_def_t wlr_xwayland_surface_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.destroy),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.request_configure),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.request_move),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.request_resize),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.request_minimize),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.request_maximize),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.request_fullscreen),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.request_activate),

    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.map),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.unmap),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.set_title),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.set_class),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.set_role),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.set_parent),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.set_pid),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.set_startup_id),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.set_window_type),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.set_hints),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.set_decorations),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.set_override_redirect),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.set_geometry),
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, events.ping_timeout),

    {NULL, 0},
};

static const jl_offset_def_t wlr_xwayland_surface_list_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_xwayland_surface, children),
    {NULL, 0},
};

static int method_wlr_xwayland_surface_get(void *p, Janet key, Janet *out);
static void method_wlr_xwayland_surface_put(void *p, Janet key, Janet value);
static const JanetAbstractType jwlr_at_wlr_xwayland_surface = {
    .name = MOD_NAME "/wlr-xwayland-surface",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xwayland_surface_get,
    .put = method_wlr_xwayland_surface_put,
    .marshal = NULL,
    .unmarshal = NULL,
    .tostring = method_wlr_abs_obj_tostring,
    JANET_ATEND_TOSTRING
};


static int method_wlr_xwayland_resize_event_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xwayland_resize_event = {
    .name = MOD_NAME "/wlr-xwayland-resize-event",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xwayland_resize_event_get,
    JANET_ATEND_GET
};


static int method_wlr_xwayland_minimize_event_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xwayland_minimize_event = {
    .name = MOD_NAME "/wlr-xwayland-minimize-event",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xwayland_minimize_event_get,
    JANET_ATEND_GET
};


static int method_wlr_xwayland_surface_configure_event_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xwayland_surface_configure_event = {
    .name = MOD_NAME "/wlr-xwayland-surface-configure-event",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xwayland_surface_configure_event_get,
    JANET_ATEND_GET
};


static const jl_offset_def_t wlr_layer_shell_v1_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_layer_shell_v1, events.new_surface),
    JWLR_OFFSET_DEF(struct wlr_layer_shell_v1, events.destroy),
    {NULL, 0},
};

static int method_wlr_layer_shell_v1_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_layer_shell_v1 = {
    .name = MOD_NAME "/wlr-layer-shell-v1",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_layer_shell_v1_get,
    JANET_ATEND_GET
};


#endif
