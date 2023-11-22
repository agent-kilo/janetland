#include <stdio.h>

#include <janet.h>

#include <wayland-server-core.h>

#include <wlr/util/log.h>
#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_seat.h>

#include "jl.h"
#include "types.h"

#define MOD_NAME "wlr"
#define WL_MOD_NAME "wl"
#define WL_MOD_FULL_NAME "janetland/wl"


JANET_THREAD_LOCAL JanetFunction *jwlr_log_callback_fn;

#define JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct_type, member) \
    {#member, (uint64_t)&(((struct_type *)NULL)->member)}

static struct wl_signal **get_abstract_struct_signal_member(void *p,
                                                            const uint8_t *kw_name,
                                                            const jl_offset_def_t *offsets)
{
    for (int i = 0; NULL != offsets[i].name; i++) {
        if (!janet_cstrcmp(kw_name, offsets[i].name)) {
            const JanetAbstractType *at = jl_get_abstract_type_by_name(WL_MOD_NAME "/wl-signal");
            struct wl_signal **signal_p = janet_abstract(at, sizeof(*signal_p));
            *signal_p = (struct wl_signal *)(p + offsets[i].offset);
            return signal_p;
        }
    }
    return NULL;
}


static const jl_key_def_t log_defs[] = {
    {"silent", WLR_SILENT},
    {"error", WLR_ERROR},
    {"info", WLR_INFO},
    {"debug", WLR_DEBUG},
    {"log-importance-last", WLR_LOG_IMPORTANCE_LAST},
};

static enum wlr_log_importance jwlr_get_log_importance(const Janet *argv, int32_t n)
{
    const uint8_t *kw = janet_getkeyword(argv, n);
    for (int i = 0; i < WLR_LOG_IMPORTANCE_LAST; i++) {
        if (!janet_cstrcmp(kw, log_defs[i].name)) {
            return log_defs[i].key;
        }
    }
    janet_panicf("unknown log type %v", argv[n]);
}

void jwlr_log_callback(enum wlr_log_importance importance, const char *fmt, va_list args)
{
    if (!jwlr_log_callback_fn) {
        /* May occur when this function is called from a thread which never called wlr_log_init() */
        fprintf(stderr, "%s:%d - log callback not initialized in current thread\n", __FILE__, __LINE__);
        return;
    }

    va_list args_copy;
    int str_len;
    JanetBuffer *buf = janet_buffer(256); /* arbitrary size */

    va_copy(args_copy, args);
    str_len = vsnprintf((char *)(buf->data), buf->capacity, fmt, args);
    if (str_len < 0) {
        fprintf(stderr, "%s:%d - vsnprintf() failed, fmt = \"%s\"\n", __FILE__, __LINE__, fmt);
        return;
    }
    if (str_len >= buf->capacity) {
        janet_buffer_ensure(buf, str_len + 1, 1);
        str_len = vsnprintf((char *)(buf->data), buf->capacity, fmt, args_copy);
    }
    buf->count = str_len;

    Janet argv[2] = {
        janet_ckeywordv(log_defs[importance].name),
        janet_wrap_buffer(buf),
    };
    Janet ret = janet_wrap_nil();
    JanetFiber *fiber = NULL;
    int sig = janet_pcall(jwlr_log_callback_fn, 2, argv, &ret, &fiber);
    if (JANET_SIGNAL_OK != sig) {
        janet_stacktrace(fiber, ret);
    }
}


static Janet cfun_wlr_log_init(int32_t argc, Janet *argv)
{
    enum wlr_log_importance verbosity;
    JanetFunction *cb;

    wlr_log_func_t ccb = NULL;

    janet_arity(argc, 1, 2);

    verbosity = jwlr_get_log_importance(argv, 0);
    cb = janet_optfunction(argv, argc, 1, NULL);
    if (cb) {
        if (jwlr_log_callback_fn) {
            janet_gcunroot(janet_wrap_function(jwlr_log_callback_fn));
        }
        jwlr_log_callback_fn = cb;
        janet_gcroot(janet_wrap_function(cb));
        ccb = jwlr_log_callback;
    }

    wlr_log_init(verbosity, ccb);

    return janet_wrap_nil();
}


static Janet cfun_wlr_log_get_verbosity(int32_t argc, Janet *argv)
{
    (void)argv;
    janet_fixarity(argc, 0);

    enum wlr_log_importance verb = wlr_log_get_verbosity();
    return janet_ckeywordv(log_defs[verb].name);
}


static Janet cfun_wlr_log(int32_t argc, Janet *argv)
{
    enum wlr_log_importance verb;
    Janet *fmt_argv;
    int32_t fmt_argc;

    Janet str_fmt;
    JanetCFunction str_fmt_fn;
    Janet formatted_str;

    janet_arity(argc, 2, -1);

    verb = jwlr_get_log_importance(argv, 0);
    fmt_argv = &(argv[1]);
    fmt_argc = argc - 1;

    str_fmt = janet_resolve_core("string/format");
    if (!janet_checktype(str_fmt, JANET_CFUNCTION)) {
        janet_panic("core cfun string/format not found");
    }
    str_fmt_fn = janet_unwrap_cfunction(str_fmt);
    formatted_str = str_fmt_fn(fmt_argc, fmt_argv);
    _wlr_log(verb, (const char *)janet_unwrap_string(formatted_str));

    return janet_wrap_nil();
}


static const jl_offset_def_t wlr_backend_signal_offsets[] = {
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_backend, events.destroy),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_backend, events.new_input),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_backend, events.new_output),
    {NULL, 0},
};

static int wlr_backend_get(void *p, Janet key, Janet *out) {
    struct wlr_backend **backend_p = (struct wlr_backend **)p;
    struct wlr_backend *backend = *backend_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    struct wl_signal **signal_p = get_abstract_struct_signal_member(backend,
                                                                    janet_unwrap_keyword(key),
                                                                    wlr_backend_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }
    return 0;
}

static const JanetAbstractType jwlr_at_wlr_backend = {
    .name = MOD_NAME "/wlr-backend",
    .gc = NULL, /* TODO: close the backend? */
    .gcmark = NULL,
    .get = wlr_backend_get,
    JANET_ATEND_GET
};

static Janet cfun_wlr_backend_autocreate(int32_t argc, Janet *argv)
{
    struct wl_display **display_p;

    struct wlr_backend **backend_p;

    janet_fixarity(argc, 1);

    display_p = janet_getabstract(argv, 0, jl_get_abstract_type_by_name(WL_MOD_NAME "/wl-display"));
    backend_p = janet_abstract(&jwlr_at_wlr_backend, sizeof(*backend_p));
    *backend_p = wlr_backend_autocreate(*display_p);
    if (!(*backend_p)) {
        janet_panic("failed to create wlroots backend object");
    }

    return janet_wrap_abstract(backend_p);
}


static Janet cfun_wlr_backend_destroy(int32_t argc, Janet *argv)
{
    struct wlr_backend **backend_p;

    janet_fixarity(argc, 1);

    backend_p = janet_getabstract(argv, 0, &jwlr_at_wlr_backend);
    wlr_backend_destroy(*backend_p);

    return janet_wrap_nil();
}


static Janet cfun_wlr_backend_start(int32_t argc, Janet *argv)
{
    struct wlr_backend **backend_p;

    janet_fixarity(argc, 1);

    backend_p = janet_getabstract(argv, 0, &jwlr_at_wlr_backend);
    return janet_wrap_boolean(wlr_backend_start(*backend_p));
}


static const JanetAbstractType jwlr_at_wlr_renderer = {
    .name = MOD_NAME "/wlr-renderer",
    .gc = NULL, /* TODO: close the renderer? */
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

static Janet cfun_wlr_renderer_autocreate(int32_t argc, Janet *argv)
{
    struct wlr_backend **backend_p;

    struct wlr_renderer **renderer_p;

    janet_fixarity(argc, 1);

    backend_p = janet_getabstract(argv, 0, &jwlr_at_wlr_backend);
    renderer_p = janet_abstract(&jwlr_at_wlr_renderer, sizeof(*renderer_p));
    *renderer_p = wlr_renderer_autocreate(*backend_p);
    if (!(*renderer_p)) {
        janet_panic("failed to create wlroots renderer object");
    }

    return janet_wrap_abstract(renderer_p);
}


static Janet cfun_wlr_renderer_init_wl_display(int32_t argc, Janet *argv)
{
    struct wlr_renderer **renderer_p;
    struct wl_display **display_p;

    bool ret;

    janet_fixarity(argc, 2);

    renderer_p = janet_getabstract(argv, 0, &jwlr_at_wlr_renderer);
    display_p = janet_getabstract(argv, 1, jl_get_abstract_type_by_name(WL_MOD_NAME "/wl-display"));
    ret = wlr_renderer_init_wl_display(*renderer_p, *display_p);

    return janet_wrap_boolean(ret);
}


static const JanetAbstractType jwlr_at_wlr_allocator = {
    .name = MOD_NAME "/wlr-allocator",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

static Janet cfun_wlr_allocator_autocreate(int32_t argc, Janet *argv)
{
    struct wlr_backend **backend_p;
    struct wlr_renderer **renderer_p;

    struct wlr_allocator **allocator_p;

    janet_fixarity(argc, 2);

    backend_p = janet_getabstract(argv, 0, &jwlr_at_wlr_backend);
    renderer_p = janet_getabstract(argv, 1, &jwlr_at_wlr_renderer);
    allocator_p = janet_abstract(&jwlr_at_wlr_allocator, sizeof(*allocator_p));
    *allocator_p = wlr_allocator_autocreate(*backend_p, *renderer_p);
    if (!(*allocator_p)) {
        janet_panic("failed to create wlroots allocator object");
    }

    return janet_wrap_abstract(allocator_p);
}


static const JanetAbstractType jwlr_at_wlr_compositor = {
    .name = MOD_NAME "/wlr-compositor",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

static Janet cfun_wlr_compositor_create(int32_t argc, Janet *argv)
{
    struct wl_display **display_p;
    struct wlr_renderer **renderer_p;

    struct wlr_compositor **compositor_p;

    janet_fixarity(argc, 2);

    display_p = janet_getabstract(argv, 0, jl_get_abstract_type_by_name(WL_MOD_NAME "/wl-display"));
    renderer_p = janet_getabstract(argv, 1, &jwlr_at_wlr_renderer);
    compositor_p = janet_abstract(&jwlr_at_wlr_compositor, sizeof(*compositor_p));
    *compositor_p = wlr_compositor_create(*display_p, *renderer_p);
    if (!(*compositor_p)) {
        janet_panic("failed to create wlroots compositor object");
    }

    return janet_wrap_abstract(compositor_p);
}


static const JanetAbstractType jwlr_at_wlr_subcompositor = {
    .name = MOD_NAME "/wlr-subcompositor",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

static Janet cfun_wlr_subcompositor_create(int32_t argc, Janet *argv)
{
    struct wl_display **display_p;

    struct wlr_subcompositor **subcompositor_p;

    janet_fixarity(argc, 1);

    display_p = janet_getabstract(argv, 0, jl_get_abstract_type_by_name(WL_MOD_NAME "/wl-display"));
    subcompositor_p = janet_abstract(&jwlr_at_wlr_subcompositor, sizeof(*subcompositor_p));
    *subcompositor_p = wlr_subcompositor_create(*display_p);
    if (!(*subcompositor_p)) {
        janet_panic("failed to create wlroots subcompositor object");
    }

    return janet_wrap_abstract(subcompositor_p);
}


static const JanetAbstractType jwlr_at_wlr_data_device_manager = {
    .name = MOD_NAME "/wlr-data-device-manager",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

static Janet cfun_wlr_data_device_manager_create(int32_t argc, Janet *argv)
{
    struct wl_display **display_p;

    struct wlr_data_device_manager **manager_p;

    janet_fixarity(argc, 1);

    display_p = janet_getabstract(argv, 0, jl_get_abstract_type_by_name(WL_MOD_NAME "/wl-display"));
    manager_p = janet_abstract(&jwlr_at_wlr_data_device_manager, sizeof(*manager_p));
    *manager_p = wlr_data_device_manager_create(*display_p);
    if (!(*manager_p)) {
        janet_panic("failed to create wlroots data device manager object");
    }

    return janet_wrap_abstract(manager_p);
}


static const JanetAbstractType jwlr_at_wlr_output_layout = {
    .name = MOD_NAME "/wlr-output-layout",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

static Janet cfun_wlr_output_layout_create(int32_t argc, Janet *argv)
{
    (void)argv;

    struct wlr_output_layout **layout_p;

    janet_fixarity(argc, 0);

    layout_p = janet_abstract(&jwlr_at_wlr_output_layout, sizeof(*layout_p));
    *layout_p = wlr_output_layout_create();
    if (!(*layout_p)) {
        janet_panic("failed to create wlroots output layout object");
    }

    return janet_wrap_abstract(layout_p);
}


static const JanetAbstractType jwlr_at_wlr_scene = {
    .name = MOD_NAME "/wlr-scene",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

static Janet cfun_wlr_scene_create(int32_t argc, Janet *argv)
{
    (void)argv;

    struct wlr_scene **scene_p;

    janet_fixarity(argc, 0);

    scene_p = janet_abstract(&jwlr_at_wlr_scene, sizeof(*scene_p));
    *scene_p = wlr_scene_create();
    if (!(*scene_p)) {
        janet_panic("failed to create wlroots scene object");
    }

    return janet_wrap_abstract(scene_p);
}


static Janet cfun_wlr_scene_attach_output_layout(int32_t argc, Janet *argv)
{
    struct wlr_scene **scene_p;
    struct wlr_output_layout **layout_p;

    janet_fixarity(argc, 2);

    scene_p = janet_getabstract(argv, 0, &jwlr_at_wlr_scene);
    layout_p = janet_getabstract(argv, 1, &jwlr_at_wlr_output_layout);

    return janet_wrap_boolean(wlr_scene_attach_output_layout(*scene_p, *layout_p));
}


static const jl_offset_def_t wlr_xdg_shell_signal_offsets[] = {
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_xdg_shell, events.new_surface),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_xdg_shell, events.destroy),
    {NULL, 0},
};

static int wlr_xdg_shell_get(void *p, Janet key, Janet *out) {
    struct wlr_xdg_shell **xdg_shell_p = (struct wlr_xdg_shell **)p;
    struct wlr_xdg_shell *xdg_shell = *xdg_shell_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    struct wl_signal **signal_p = get_abstract_struct_signal_member(xdg_shell,
                                                                    janet_unwrap_keyword(key),
                                                                    wlr_xdg_shell_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }
    return 0;
}

static const JanetAbstractType jwlr_at_wlr_xdg_shell = {
    .name = MOD_NAME "/wlr-xdg-shell",
    .gc = NULL,
    .gcmark = NULL,
    .get = wlr_xdg_shell_get,
    JANET_ATEND_GET
};

static Janet cfun_wlr_xdg_shell_create(int32_t argc, Janet *argv)
{
    struct wl_display **display_p;
    uint32_t version;

    struct wlr_xdg_shell **xdg_shell_p;

    janet_fixarity(argc, 2);

    display_p = janet_getabstract(argv, 0, jl_get_abstract_type_by_name(WL_MOD_NAME "/wl-display"));
    /* XXX: int32_t -> uint32_t conversion, to avoid s64/u64 hassle.
       Versions numbers should be small anyway, right? right?? */
    version = (uint32_t)janet_getinteger(argv, 1);
    xdg_shell_p = janet_abstract(&jwlr_at_wlr_xdg_shell, sizeof(*xdg_shell_p));
    *xdg_shell_p = wlr_xdg_shell_create(*display_p, version);
    if (!(*xdg_shell_p)) {
        janet_panic("failed to create wlroots xdg shell object");
    }

    return janet_wrap_abstract(xdg_shell_p);
}


static const jl_offset_def_t wlr_cursor_signal_offsets[] = {
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.motion),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.motion_absolute),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.button),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.axis),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.frame),

    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.swipe_begin),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.swipe_update),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.swipe_end),

    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.pinch_begin),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.pinch_update),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.pinch_end),

    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.hold_begin),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.hold_end),

    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.touch_up),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.touch_down),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.touch_motion),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.touch_cancel),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.touch_frame),

    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.tablet_tool_axis),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.tablet_tool_proximity),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.tablet_tool_tip),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_cursor, events.tablet_tool_button),

    {NULL, 0},
};

static int wlr_cursor_get(void *p, Janet key, Janet *out) {
    struct wlr_cursor **cursor_p = (struct wlr_cursor **)p;
    struct wlr_cursor *cursor = *cursor_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    struct wl_signal **signal_p = get_abstract_struct_signal_member(cursor,
                                                                    janet_unwrap_keyword(key),
                                                                    wlr_cursor_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }
    return 0;
}

static const JanetAbstractType jwlr_at_wlr_cursor = {
    .name = MOD_NAME "/wlr-cursor",
    .gc = NULL,
    .gcmark = NULL,
    .get = wlr_cursor_get,
    JANET_ATEND_GET
};

static Janet cfun_wlr_cursor_create(int32_t argc, Janet *argv)
{
    (void)argv;

    struct wlr_cursor **cursor_p;

    janet_fixarity(argc, 0);

    cursor_p = janet_abstract(&jwlr_at_wlr_cursor, sizeof(*cursor_p));
    *cursor_p = wlr_cursor_create();
    if (!(*cursor_p)) {
        janet_panic("failed to create wlroots cursor object");
    }

    return janet_wrap_abstract(cursor_p);
}


static Janet cfun_wlr_cursor_attach_output_layout(int32_t argc, Janet *argv)
{
    struct wlr_cursor **cursor_p;
    struct wlr_output_layout **layout_p;

    janet_fixarity(argc, 2);

    cursor_p = janet_getabstract(argv, 0, &jwlr_at_wlr_cursor);
    layout_p = janet_getabstract(argv, 1, &jwlr_at_wlr_output_layout);
    wlr_cursor_attach_output_layout(*cursor_p, *layout_p);

    return janet_wrap_nil();
}


static const JanetAbstractType jwlr_at_wlr_xcursor_manager = {
    .name = MOD_NAME "/wlr-xcursor-manager",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

static Janet cfun_wlr_xcursor_manager_create(int32_t argc, Janet *argv)
{
    const char *name;
    uint32_t size;

    struct wlr_xcursor_manager **manager_p;

    janet_fixarity(argc, 2);

    if (janet_checktype(argv[0], JANET_NIL)) {
        name = NULL;
    } else {
        name = (const char *)janet_getstring(argv, 0);
    }
    /* XXX: int32_t -> uint32_t conversion, to avoid s64/u64 hassle. */
    size = (uint32_t)janet_getinteger(argv, 1);
    manager_p = janet_abstract(&jwlr_at_wlr_xcursor_manager, sizeof(*manager_p));
    *manager_p = wlr_xcursor_manager_create(name, size);
    if (!(*manager_p)) {
        janet_panic("failed to create wlroots xcursor manager object");
    }

    return janet_wrap_abstract(manager_p);
}


static Janet cfun_wlr_xcursor_manager_load(int32_t argc, Janet *argv)
{
    struct wlr_xcursor_manager **manager_p;
    float scale;

    janet_fixarity(argc, 2);

    manager_p = janet_getabstract(argv, 0, &jwlr_at_wlr_xcursor_manager);
    /* XXX: double -> float conversion */
    scale = (float)janet_getnumber(argv, 1);

    return janet_wrap_boolean(wlr_xcursor_manager_load(*manager_p, scale));
}


static const jl_offset_def_t wlr_seat_signal_offsets[] = {
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_seat, events.pointer_grab_begin),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_seat, events.pointer_grab_end),

    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_seat, events.keyboard_grab_begin),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_seat, events.keyboard_grab_end),

    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_seat, events.touch_grab_begin),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_seat, events.touch_grab_end),

    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_seat, events.request_set_cursor),

    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_seat, events.request_set_selection),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_seat, events.set_selection),

    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_seat, events.request_set_primary_selection),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_seat, events.set_primary_selection),

    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_seat, events.request_start_drag),
    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_seat, events.start_drag),

    JWLR_EVENTS_SIGNAL_OFFSET_DEF(struct wlr_seat, events.destroy),

    {NULL, 0},
};

static int wlr_seat_get(void *p, Janet key, Janet *out) {
    struct wlr_seat **seat_p = (struct wlr_seat **)p;
    struct wlr_seat *seat = *seat_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    struct wl_signal **signal_p = get_abstract_struct_signal_member(seat,
                                                                    janet_unwrap_keyword(key),
                                                                    wlr_seat_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }
    return 0;
}

static const JanetAbstractType jwlr_at_wlr_seat = {
    .name = MOD_NAME "/wlr-seat",
    .gc = NULL,
    .gcmark = NULL,
    .get = wlr_seat_get,
    JANET_ATEND_GET
};

static Janet cfun_wlr_seat_create(int32_t argc, Janet *argv)
{
    struct wl_display **display_p;
    const char *name;

    struct wlr_seat **seat_p;

    janet_fixarity(argc, 2);

    display_p = janet_getabstract(argv, 0, jl_get_abstract_type_by_name(WL_MOD_NAME "/wl-display"));
    name = (const char *)janet_getstring(argv, 1);
    seat_p = janet_abstract(&jwlr_at_wlr_seat, sizeof(*seat_p));
    *seat_p = wlr_seat_create(*display_p, name);
    if (!(*seat_p)) {
        janet_panic("failed to create wlroots seat object");
    }

    return janet_wrap_abstract(seat_p);
}


static JanetReg cfuns[] = {
    {
        "wlr-log-init", cfun_wlr_log_init,
        "(" MOD_NAME "/wlr-log-init verbosity &opt callback)\n\n"
        "Initializes log infrastructure."
    },
    {
        "wlr-log-get-verbosity", cfun_wlr_log_get_verbosity,
        "(" MOD_NAME "/wlr-log-get-verbosity)\n\n"
        "Returns the current log verbosity."
    },
    {
        "wlr-log", cfun_wlr_log,
        "(" MOD_NAME "/wlr-log verbosity format & args)\n\n"
        "Logs a formatted string message."
    },
    {
        "wlr-backend-autocreate", cfun_wlr_backend_autocreate,
        "(" MOD_NAME "/wlr-backend-autocreate wl-display)\n\n"
        "Creates a wlroots backend object."
    },
    {
        "wlr-backend-destroy", cfun_wlr_backend_destroy,
        "(" MOD_NAME "/wlr-backend-destroy wlr-backend)\n\n"
        "Destroys a wlroots backend object."
    },
    {
        "wlr-backend-start", cfun_wlr_backend_start,
        "(" MOD_NAME "/wlr-backend-start wlr-backend)\n\n"
        "Starts the wlroots backend."
    },
    {
        "wlr-renderer-autocreate", cfun_wlr_renderer_autocreate,
        "(" MOD_NAME "/wlr-renderer-autocreate wlr-backend)\n\n"
        "Creates a wlroots renderer object."
    },
    {
        "wlr-renderer-init-wl-display", cfun_wlr_renderer_init_wl_display,
        "(" MOD_NAME "/wlr-renderer-init-wl-display wlr-renderer wl-display)\n\n"
        "Initializes a wlroots renderer object."
    },
    {
        "wlr-allocator-autocreate", cfun_wlr_allocator_autocreate,
        "(" MOD_NAME "/wlr-allocator-autocreate wlr-backend wlr-renderer)\n\n"
        "Creates a wlroots allocator object."
    },
    {
        "wlr-compositor-create", cfun_wlr_compositor_create,
        "(" MOD_NAME "/wlr-compositor-create wl-display wlr-renderer)\n\n"
        "Creates a wlroots compositor object."
    },
    {
        "wlr-subcompositor-create", cfun_wlr_subcompositor_create,
        "(" MOD_NAME "/wlr-subcompositor-create wl-display)\n\n"
        "Creates a wlroots subcompositor object."
    },
    {
        "wlr-data-device-manager-create", cfun_wlr_data_device_manager_create,
        "(" MOD_NAME "/wlr-data-device-manager-create wl-display)\n\n"
        "Creates a wlroots data device manager object."
    },
    {
        "wlr-output-layout-create", cfun_wlr_output_layout_create,
        "(" MOD_NAME "/wlr-output-layout-create)\n\n"
        "Creates a wlroots output layout object."
    },
    {
        "wlr-scene-create", cfun_wlr_scene_create,
        "(" MOD_NAME "/wlr-scene-create)\n\n"
        "Creates a wlroots scene object."
    },
    {
        "wlr-scene-attach-output-layout", cfun_wlr_scene_attach_output_layout,
        "(" MOD_NAME "/wlr-scene-attach-output-layout wlr-scene wlr-output-layout)\n\n"
        "Attaches a wlroots output layout object to a scene object."
    },
    {
        "wlr-xdg-shell-create", cfun_wlr_xdg_shell_create,
        "(" MOD_NAME "/wlr-xdg-shell-create wl-display version)\n\n"
        "Creates a wlroots xdg shell object."
    },
    {
        "wlr-cursor-create", cfun_wlr_cursor_create,
        "(" MOD_NAME "/wlr-cursor-create)\n\n"
        "Creates a wlroots cursor object."
    },
    {
        "wlr-cursor-attach-output-layout", cfun_wlr_cursor_attach_output_layout,
        "(" MOD_NAME "/wlr-cursor-attach-output-layout wlr-cursor wlr-output-layout)\n\n"
        "Attaches a wlroots output layout object to a cursor object."
    },
    {
        "wlr-xcursor-manager-create", cfun_wlr_xcursor_manager_create,
        "(" MOD_NAME "/wlr-xcursor-manager-create name size)\n\n"
        "Creates a wlroots xcursor manager object."
    },
    {
        "wlr-xcursor-manager-load", cfun_wlr_xcursor_manager_load,
        "(" MOD_NAME "/wlr-xcursor-manager-load wlr-xcursor-manager scale)\n\n"
        "Loads an xcursor theme at the given scale factor."
    },
    {
        "wlr-seat-create", cfun_wlr_seat_create,
        "(" MOD_NAME "/wlr-seat-create wl-display name)\n\n"
        "Creates a wlroots seat object."
    },
    {NULL, NULL, NULL},
};


JANET_MODULE_ENTRY(JanetTable *env)
{
    /* Import wl module first, so that we can find the wl_* abstract types */
    /* XXX: this will pollute the environment with wl/ stuff, even :export is set to nil */
    jl_import(WL_MOD_FULL_NAME);

    janet_register_abstract_type(&jwlr_at_wlr_backend);
    janet_register_abstract_type(&jwlr_at_wlr_renderer);
    janet_register_abstract_type(&jwlr_at_wlr_allocator);
    janet_register_abstract_type(&jwlr_at_wlr_compositor);
    janet_register_abstract_type(&jwlr_at_wlr_subcompositor);
    janet_register_abstract_type(&jwlr_at_wlr_data_device_manager);
    janet_register_abstract_type(&jwlr_at_wlr_output_layout);
    janet_register_abstract_type(&jwlr_at_wlr_scene);
    janet_register_abstract_type(&jwlr_at_wlr_xdg_shell);
    janet_register_abstract_type(&jwlr_at_wlr_cursor);
    janet_register_abstract_type(&jwlr_at_wlr_xcursor_manager);
    janet_register_abstract_type(&jwlr_at_wlr_seat);

    janet_cfuns(env, MOD_NAME, cfuns);
}
