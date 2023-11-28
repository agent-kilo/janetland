#ifndef __WLR_ABS_TYPES_H__
#define __WLR_ABS_TYPES_H__

#ifndef MOD_NAME
#define MOD_NAME WLR_MOD_NAME
#endif

#define JWLR_OFFSET_DEF(struct_type, member) \
    {#member, (uint64_t)&(((struct_type *)NULL)->member)}


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


static int method_wlr_scene_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_scene = {
    .name = MOD_NAME "/wlr-scene",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_scene_get,
    JANET_ATEND_GET
};


static const JanetAbstractType jwlr_at_wlr_scene_output = {
    .name = MOD_NAME "/wlr-scene-output",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GET
};


static int method_wlr_xdg_shell_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xdg_shell = {
    .name = MOD_NAME "/wlr-xdg-shell",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xdg_shell_get,
    JANET_ATEND_GET
};


static const JanetAbstractType jwlr_at_wlr_surface = {
    .name = MOD_NAME "/wlr-surface",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static int method_wlr_xdg_popup_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xdg_popup = {
    .name = MOD_NAME "/wlr-xdg-popup",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xdg_popup_get,
    JANET_ATEND_GET
};


static int method_wlr_xdg_toplevel_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xdg_toplevel = {
    .name = MOD_NAME "/wlr-xdg-toplevel",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xdg_toplevel_get,
    JANET_ATEND_GET
};


static int method_wlr_xdg_toplevel_resize_event_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_xdg_toplevel_resize_event = {
    .name = MOD_NAME "/wlr-xdg-toplevel-resize-even",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xdg_toplevel_resize_event_get,
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

static int method_wlr_xdg_surface_get(void *p, Janet key, Janet *out);
static void method_wlr_xdg_surface_put(void *p, Janet key, Janet value);
static const JanetAbstractType jwlr_at_wlr_xdg_surface = {
    .name = MOD_NAME "/wlr-xdg-surface",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_xdg_surface_get,
    .put = method_wlr_xdg_surface_put,
    JANET_ATEND_PUT
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
    JANET_ATEND_GCMARK
};


static int method_wlr_output_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_output = {
    .name = MOD_NAME "/wlr-output",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_output_get,
    JANET_ATEND_GET
};


static const JanetAbstractType jwlr_at_wlr_output_mode = {
    .name = MOD_NAME "/wlr-output-mode",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static const JanetAbstractType jwlr_at_wlr_output_cursor = {
    .name = MOD_NAME "/wlr-output-cursor",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static int method_wlr_scene_tree_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_scene_tree = {
    .name = MOD_NAME "/wlr-scene-tree",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_scene_tree_get,
    JANET_ATEND_GET
};


static int method_wlr_scene_node_get(void *p, Janet key, Janet *out);
static void method_wlr_scene_node_put(void *p, Janet key, Janet value);
static const JanetAbstractType jwlr_at_wlr_scene_node = {
    .name = MOD_NAME "/wlr-scene-node",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_scene_node_get,
    .put = method_wlr_scene_node_put,
    JANET_ATEND_PUT
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

static int method_wlr_input_device_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_input_device = {
    .name = MOD_NAME "/wlr-input-device",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_input_device_get,
    JANET_ATEND_GET
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

static const jl_key_def_t wlr_axis_orientation_defs[] = {
    {"vertical", WLR_AXIS_ORIENTATION_VERTICAL},
    {"horizontal", WLR_AXIS_ORIENTATION_HORIZONTAL},
    {NULL, 0},
};

static int method_wlr_pointer_axis_event_get(void *p, Janet key, Janet *out);
static const JanetAbstractType jwlr_at_wlr_pointer_axis_event = {
    .name = MOD_NAME "/wlr-pointer-axis-event",
    .gc = NULL,
    .gcmark = NULL,
    .get = method_wlr_pointer_axis_event_get,
    JANET_ATEND_GET
};


#endif
