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
#include <wlr/types/wlr_output.h>

#include "jl.h"
#include "types.h"
#include "wlr_abs_types.h"


#ifndef MOD_NAME
#define MOD_NAME WLR_MOD_NAME
#endif


JANET_THREAD_LOCAL JanetFunction *jwlr_log_callback_fn;

#define JWLR_OFFSET_DEF(struct_type, member) \
    {#member, (uint64_t)&(((struct_type *)NULL)->member)}

static struct wl_signal **get_abstract_struct_signal_member(void *p,
                                                            const uint8_t *kw_name,
                                                            const jl_offset_def_t *offsets)
{
    for (int i = 0; NULL != offsets[i].name; i++) {
        if (!janet_cstrcmp(kw_name, offsets[i].name)) {
            return (struct wl_signal **)jl_pointer_to_abs_obj_by_name(p + offsets[i].offset,
                                                                      WL_MOD_NAME "/wl-signal");
        }
    }
    return NULL;
}

static struct wl_list **get_abstract_struct_list_member(void *p,
                                                        const uint8_t *kw_name,
                                                        const jl_offset_def_t *offsets)
{
    for (int i = 0; NULL != offsets[i].name; i++) {
        if (!janet_cstrcmp(kw_name, offsets[i].name)) {
            return (struct wl_list **)jl_pointer_to_abs_obj_by_name(p + offsets[i].offset,
                                                                    WL_MOD_NAME "/wl-list");
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
    JWLR_OFFSET_DEF(struct wlr_backend, events.destroy),
    JWLR_OFFSET_DEF(struct wlr_backend, events.new_input),
    JWLR_OFFSET_DEF(struct wlr_backend, events.new_output),
    {NULL, 0},
};

static int method_wlr_backend_get(void *p, Janet key, Janet *out) {
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


static Janet cfun_wlr_backend_autocreate(int32_t argc, Janet *argv)
{
    struct wl_display *display;

    struct wlr_backend *backend;

    janet_fixarity(argc, 1);

    display = jl_get_abs_obj_pointer_by_name(argv, 0, WL_MOD_NAME "/wl-display");
    backend = wlr_backend_autocreate(display);
    if (!backend) {
        janet_panic("failed to create wlroots backend object");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(backend, &jwlr_at_wlr_backend));
}


static Janet cfun_wlr_backend_destroy(int32_t argc, Janet *argv)
{
    struct wlr_backend *backend;

    janet_fixarity(argc, 1);

    backend = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_backend);
    wlr_backend_destroy(backend);

    return janet_wrap_nil();
}


static Janet cfun_wlr_backend_start(int32_t argc, Janet *argv)
{
    struct wlr_backend *backend;

    janet_fixarity(argc, 1);

    backend = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_backend);
    return janet_wrap_boolean(wlr_backend_start(backend));
}


static Janet cfun_wlr_renderer_autocreate(int32_t argc, Janet *argv)
{
    struct wlr_backend *backend;

    struct wlr_renderer *renderer;

    janet_fixarity(argc, 1);

    backend = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_backend);
    renderer = wlr_renderer_autocreate(backend);
    if (!renderer) {
        janet_panic("failed to create wlroots renderer object");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(renderer, &jwlr_at_wlr_renderer));
}


static Janet cfun_wlr_renderer_init_wl_display(int32_t argc, Janet *argv)
{
    struct wlr_renderer *renderer;
    struct wl_display *display;

    bool ret;

    janet_fixarity(argc, 2);

    renderer = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_renderer);
    display = jl_get_abs_obj_pointer_by_name(argv, 1, WL_MOD_NAME "/wl-display");
    ret = wlr_renderer_init_wl_display(renderer, display);

    return janet_wrap_boolean(ret);
}


static Janet cfun_wlr_allocator_autocreate(int32_t argc, Janet *argv)
{
    struct wlr_backend *backend;
    struct wlr_renderer *renderer;

    struct wlr_allocator *allocator;

    janet_fixarity(argc, 2);

    backend = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_backend);
    renderer = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_renderer);
    allocator = wlr_allocator_autocreate(backend, renderer);
    if (!allocator) {
        janet_panic("failed to create wlroots allocator object");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(allocator, &jwlr_at_wlr_allocator));
}


static Janet cfun_wlr_compositor_create(int32_t argc, Janet *argv)
{
    struct wl_display *display;
    struct wlr_renderer *renderer;

    struct wlr_compositor *compositor;

    janet_fixarity(argc, 2);

    display = jl_get_abs_obj_pointer_by_name(argv, 0, WL_MOD_NAME "/wl-display");
    renderer = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_renderer);
    compositor = wlr_compositor_create(display, renderer);
    if (!compositor) {
        janet_panic("failed to create wlroots compositor object");
    }

    return janet_wrap_abstract(jl_pointer_to_abs_obj(compositor, &jwlr_at_wlr_compositor));
}


static Janet cfun_wlr_subcompositor_create(int32_t argc, Janet *argv)
{
    struct wl_display *display;

    struct wlr_subcompositor *subcompositor;

    janet_fixarity(argc, 1);

    display = jl_get_abs_obj_pointer_by_name(argv, 0, WL_MOD_NAME "/wl-display");
    subcompositor = wlr_subcompositor_create(display);
    if (!subcompositor) {
        janet_panic("failed to create wlroots subcompositor object");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(subcompositor, &jwlr_at_wlr_subcompositor));
}


static Janet cfun_wlr_data_device_manager_create(int32_t argc, Janet *argv)
{
    struct wl_display *display;

    struct wlr_data_device_manager *manager;

    janet_fixarity(argc, 1);

    display = jl_get_abs_obj_pointer_by_name(argv, 0, WL_MOD_NAME "/wl-display");
    manager = wlr_data_device_manager_create(display);
    if (!manager) {
        janet_panic("failed to create wlroots data device manager object");
    }

    return janet_wrap_abstract(jl_pointer_to_abs_obj(manager, &jwlr_at_wlr_data_device_manager));
}


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

static int method_wlr_output_layout_get(void *p, Janet key, Janet *out) {
    struct wlr_output_layout **layout_p = (struct wlr_output_layout **)p;
    struct wlr_output_layout *layout = *layout_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(layout, kw, wlr_output_layout_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

    struct wl_list **list_p = get_abstract_struct_list_member(layout, kw, wlr_output_layout_list_offsets);
    if (list_p) {
        *out = janet_wrap_abstract(list_p);
        return 1;
    }
    return 0;
}


static Janet cfun_wlr_output_layout_create(int32_t argc, Janet *argv)
{
    (void)argv;

    struct wlr_output_layout *layout;

    janet_fixarity(argc, 0);

    layout = wlr_output_layout_create();
    if (!layout) {
        janet_panic("failed to create wlroots output layout object");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(layout, &jwlr_at_wlr_output_layout));
}


static const jl_offset_def_t wlr_scene_list_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_scene, outputs),
};

static int method_wlr_scene_get(void *p, Janet key, Janet *out)
{
    struct wlr_scene **scene_p = (struct wlr_scene **)p;
    struct wlr_scene *scene = *scene_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_list **list_p = get_abstract_struct_list_member(scene, kw, wlr_scene_list_offsets);
    if (list_p) {
        *out = janet_wrap_abstract(list_p);
        return 1;
    }

    if (!janet_cstrcmp(kw, "tree")) {
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(&scene->tree, &jwlr_at_wlr_scene_tree));
        return 1;
    }

    return 0;
}


static Janet cfun_wlr_scene_create(int32_t argc, Janet *argv)
{
    (void)argv;

    struct wlr_scene *scene;

    janet_fixarity(argc, 0);

    scene = wlr_scene_create();
    if (!scene) {
        janet_panic("failed to create wlroots scene object");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(scene, &jwlr_at_wlr_scene));
}


static Janet cfun_wlr_scene_attach_output_layout(int32_t argc, Janet *argv)
{
    struct wlr_scene *scene;
    struct wlr_output_layout *layout;

    janet_fixarity(argc, 2);

    scene = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_scene);
    layout = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_output_layout);

    return janet_wrap_boolean(wlr_scene_attach_output_layout(scene, layout));
}


static const jl_offset_def_t wlr_xdg_shell_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_xdg_shell, events.new_surface),
    JWLR_OFFSET_DEF(struct wlr_xdg_shell, events.destroy),
    {NULL, 0},
};

static int method_wlr_xdg_shell_get(void *p, Janet key, Janet *out) {
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


static Janet cfun_wlr_xdg_shell_create(int32_t argc, Janet *argv)
{
    struct wl_display *display;
    uint32_t version;

    struct wlr_xdg_shell *xdg_shell;

    janet_fixarity(argc, 2);

    display = jl_get_abs_obj_pointer_by_name(argv, 0, WL_MOD_NAME "/wl-display");
    /* XXX: int32_t -> uint32_t conversion, to avoid s64/u64 hassle.
       Versions numbers should be small anyway, right? right?? */
    version = (uint32_t)janet_getinteger(argv, 1);
    xdg_shell = wlr_xdg_shell_create(display, version);
    if (!xdg_shell) {
        janet_panic("failed to create wlroots xdg shell object");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(xdg_shell, &jwlr_at_wlr_xdg_shell));
}


static const jl_offset_def_t wlr_xdg_popup_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_xdg_popup, events.reposition),
    {NULL, 0},
};

static int method_wlr_xdg_popup_get(void *p, Janet key, Janet *out) {
    struct wlr_xdg_popup **popup_p = (struct wlr_xdg_popup **)p;
    struct wlr_xdg_popup *popup = *popup_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(popup, kw, wlr_xdg_popup_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

    if (!janet_cstrcmp(kw, "parent")) {
        if (!(popup->parent)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(popup->parent, &jwlr_at_wlr_surface));
        return 1;
    }

    return 0;
}


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

static int method_wlr_xdg_toplevel_get(void *p, Janet key, Janet *out) {
    struct wlr_xdg_toplevel **toplevel_p = (struct wlr_xdg_toplevel **)p;
    struct wlr_xdg_toplevel *toplevel = *toplevel_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(toplevel, kw, wlr_xdg_toplevel_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

    if (!janet_cstrcmp(kw, "base")) {
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(toplevel->base, &jwlr_at_wlr_xdg_surface));
        return 1;
    }

    return 0;
}


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
};

static int method_wlr_xdg_surface_get(void *p, Janet key, Janet *out) {
    struct wlr_xdg_surface **surface_p = (struct wlr_xdg_surface **)p;
    struct wlr_xdg_surface *surface = *surface_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(surface, kw, wlr_xdg_surface_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

    struct wl_list **list_p = get_abstract_struct_list_member(surface, kw, wlr_xdg_surface_list_offsets);
    if (list_p) {
        *out = janet_wrap_abstract(list_p);
        return 1;
    }

    if (!janet_cstrcmp(kw, "role")) {
        *out = janet_ckeywordv(wlr_xdg_surface_role_defs[surface->role].name);
        return 1;
    }
    if (!janet_cstrcmp(kw, "toplevel")) {
        if (!(surface->toplevel)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(surface->toplevel, &jwlr_at_wlr_xdg_toplevel));
        return 1;
    }
    if (!janet_cstrcmp(kw, "popup")) {
        if (!(surface->popup)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(surface->popup, &jwlr_at_wlr_xdg_popup));
        return 1;
    }
    if (!janet_cstrcmp(kw, "data")) {
        *out = janet_wrap_pointer(surface->data);
        return 1;
    }

    return 0;
}

static void method_wlr_xdg_surface_put(void *p, Janet key, Janet value) {
    struct wlr_xdg_surface **surface_p = (struct wlr_xdg_surface **)p;
    struct wlr_xdg_surface *surface = *surface_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "data")) {
        surface->data = jl_value_to_data_pointer(value);
        return;
    }
}


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

static int method_wlr_cursor_get(void *p, Janet key, Janet *out) {
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


static Janet cfun_wlr_cursor_create(int32_t argc, Janet *argv)
{
    (void)argv;

    struct wlr_cursor *cursor;

    janet_fixarity(argc, 0);

    cursor = wlr_cursor_create();
    if (!cursor) {
        janet_panic("failed to create wlroots cursor object");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(cursor, &jwlr_at_wlr_cursor));
}


static Janet cfun_wlr_cursor_attach_output_layout(int32_t argc, Janet *argv)
{
    struct wlr_cursor *cursor;
    struct wlr_output_layout *layout;

    janet_fixarity(argc, 2);

    cursor = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_cursor);
    layout = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_output_layout);
    wlr_cursor_attach_output_layout(cursor, layout);

    return janet_wrap_nil();
}


static Janet cfun_wlr_xcursor_manager_create(int32_t argc, Janet *argv)
{
    const char *name;
    uint32_t size;

    struct wlr_xcursor_manager *manager;

    janet_fixarity(argc, 2);

    if (janet_checktype(argv[0], JANET_NIL)) {
        name = NULL;
    } else {
        name = (const char *)janet_getstring(argv, 0);
    }
    /* XXX: int32_t -> uint32_t conversion, to avoid s64/u64 hassle. */
    size = (uint32_t)janet_getinteger(argv, 1);
    manager = wlr_xcursor_manager_create(name, size);
    if (!manager) {
        janet_panic("failed to create wlroots xcursor manager object");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(manager, &jwlr_at_wlr_xcursor_manager));
}


static Janet cfun_wlr_xcursor_manager_load(int32_t argc, Janet *argv)
{
    struct wlr_xcursor_manager *manager;
    float scale;

    janet_fixarity(argc, 2);

    manager = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xcursor_manager);
    /* XXX: double -> float conversion */
    scale = (float)janet_getnumber(argv, 1);

    return janet_wrap_boolean(wlr_xcursor_manager_load(manager, scale));
}


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

static int method_wlr_seat_get(void *p, Janet key, Janet *out) {
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


static Janet cfun_wlr_seat_create(int32_t argc, Janet *argv)
{
    struct wl_display *display;
    const char *name;

    struct wlr_seat *seat;

    janet_fixarity(argc, 2);

    display = jl_get_abs_obj_pointer_by_name(argv, 0, WL_MOD_NAME "/wl-display");
    name = (const char *)janet_getstring(argv, 1);
    seat = wlr_seat_create(display, name);
    if (!seat) {
        janet_panic("failed to create wlroots seat object");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(seat, &jwlr_at_wlr_seat));
}


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

static int method_wlr_output_get(void *p, Janet key, Janet *out) {
    struct wlr_output **output_p = (struct wlr_output **)p;
    struct wlr_output *output = *output_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(output, kw, wlr_output_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

    struct wl_list **list_p = get_abstract_struct_list_member(output, kw, wlr_output_list_offsets);
    if (list_p) {
        *out = janet_wrap_abstract(list_p);
        return 1;
    }
    return 0;
}


static Janet cfun_wlr_output_init_render(int32_t argc, Janet *argv)
{
    struct wlr_output *output;
    struct wlr_allocator *allocator;
    struct wlr_renderer *renderer;

    janet_fixarity(argc, 3);

    output = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_output);
    allocator = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_allocator);
    renderer = jl_get_abs_obj_pointer(argv, 2, &jwlr_at_wlr_renderer);

    return janet_wrap_boolean(wlr_output_init_render(output, allocator, renderer));
}


static Janet cfun_wlr_output_preferred_mode(int32_t argc, Janet *argv)
{
    struct wlr_output *output;

    struct wlr_output_mode *output_mode;

    janet_fixarity(argc, 1);

    output = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_output);
    output_mode = wlr_output_preferred_mode(output);
    if (!output_mode) {
        janet_panic("failed to get preferred mode for output");
    }

    return janet_wrap_abstract(jl_pointer_to_abs_obj(output_mode, &jwlr_at_wlr_output_mode));
}


static Janet cfun_wlr_output_set_mode(int32_t argc, Janet *argv)
{
    struct wlr_output *output;
    struct wlr_output_mode *output_mode;

    janet_fixarity(argc, 2);

    output = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_output);
    output_mode = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_output_mode);

    wlr_output_set_mode(output, output_mode);
    return janet_wrap_nil();
}


static Janet cfun_wlr_output_enable(int32_t argc, Janet *argv)
{
    struct wlr_output *output;
    bool enabled;

    janet_fixarity(argc, 2);

    output = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_output);
    enabled = (bool)janet_getboolean(argv, 1);

    wlr_output_enable(output, enabled);
    return janet_wrap_nil();
}


static Janet cfun_wlr_output_commit(int32_t argc, Janet *argv)
{
    struct wlr_output *output;

    janet_fixarity(argc, 2);

    output = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_output);
    return janet_wrap_boolean(wlr_output_commit(output));
}


static Janet cfun_wlr_output_layout_add_auto(int32_t argc, Janet *argv)
{
    struct wlr_output_layout *layout;
    struct wlr_output *output;

    janet_fixarity(argc, 2);

    layout = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_output_layout);
    output = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_output);

    wlr_output_layout_add_auto(layout, output);
    return janet_wrap_nil();
}


static Janet cfun_wlr_xdg_surface_from_wlr_surface(int32_t argc, Janet *argv)
{
    struct wlr_surface *surface;

    struct wlr_xdg_surface *xdg_surface;

    janet_fixarity(argc, 1);

    surface = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_surface);
    xdg_surface = wlr_xdg_surface_from_wlr_surface(surface);
    if (!xdg_surface) {
        janet_panic("cannot retrieve xdg surface from wlr-surface");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(xdg_surface, &jwlr_at_wlr_xdg_surface));
}


static Janet cfun_wlr_scene_xdg_surface_create(int32_t argc, Janet *argv)
{
    struct wlr_scene_tree *parent;
    struct wlr_xdg_surface *surface;

    struct wlr_scene_tree *ret;

    janet_fixarity(argc, 2);

    parent = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_scene_tree);
    surface = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_xdg_surface);

    ret = wlr_scene_xdg_surface_create(parent, surface);
    if (!ret) {
        janet_panic("failed to create wlroots scene-graph node");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(ret, &jwlr_at_wlr_scene_tree));
}


static const jl_offset_def_t wlr_scene_tree_list_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_scene_tree, children),
    {NULL, 0},
};

static int method_wlr_scene_tree_get(void *p, Janet key, Janet *out)
{
    struct wlr_scene_tree **tree_p = (struct wlr_scene_tree **)p;
    struct wlr_scene_tree *tree = *tree_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_list **list_p = get_abstract_struct_list_member(tree, kw, wlr_scene_tree_list_offsets);
    if (list_p) {
        *out = janet_wrap_abstract(list_p);
        return 1;
    }

    if (!janet_cstrcmp(kw, "node")) {
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(&tree->node, &jwlr_at_wlr_scene_node));
        return 1;
    }

    return 0;
}


static const jl_offset_def_t wlr_scene_node_signal_offsets[] = {
    JWLR_OFFSET_DEF(struct wlr_scene_node, events.destroy),
    {NULL, 0},
};

static int method_wlr_scene_node_get(void *p, Janet key, Janet *out)
{
    struct wlr_scene_node **node_p = (struct wlr_scene_node **)p;
    struct wlr_scene_node *node = *node_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(node, kw, wlr_scene_node_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

    if (!janet_cstrcmp(kw, "data")) {
        *out = janet_wrap_pointer(node->data);
        return 1;
    }

    return 0;
}

static void method_wlr_scene_node_put(void *p, Janet key, Janet value) {
    struct wlr_scene_node **node_p = (struct wlr_scene_node **)p;
    struct wlr_scene_node *node = *node_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "data")) {
        node->data = jl_value_to_data_pointer(value);
        return;
    }
}


static Janet cfun_wlr_scene_get_scene_output(int32_t argc, Janet *argv)
{
    struct wlr_scene *scene;
    struct wlr_output *output;

    struct wlr_scene_output *scene_output;

    janet_fixarity(argc, 2);

    scene = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_scene);
    output = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_output);

    scene_output = wlr_scene_get_scene_output(scene, output);
    if (!scene_output) {
        janet_panic("failed to create wlroots scene output object");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(scene_output, &jwlr_at_wlr_scene_output));
}


static Janet cfun_wlr_scene_output_commit(int32_t argc, Janet *argv)
{
    struct wlr_scene_output *scene_output;

    janet_fixarity(argc, 1);

    scene_output = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_scene_output);
    return janet_wrap_boolean(wlr_scene_output_commit(scene_output));
}


static Janet cfun_wlr_scene_output_send_frame_done(int32_t argc, Janet *argv)
{
    struct wlr_scene_output *scene_output;
    struct timespec *now;

    janet_fixarity(argc, 2);

    scene_output = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_scene_output);
    now = janet_getabstract(argv, 1, jl_get_abstract_type_by_name(UTIL_MOD_NAME "/timespec"));

    wlr_scene_output_send_frame_done(scene_output, now);
    return janet_wrap_nil();
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
    {
        "wlr-output-init-render", cfun_wlr_output_init_render,
        "(" MOD_NAME "/wlr-output-init-render wlr-output wlr-allocator wlr-renderer)\n\n"
        "Configures the output object to use specified allocator & renderer."
    },
    {
        "wlr-output-preferred-mode", cfun_wlr_output_preferred_mode,
        "(" MOD_NAME "/wlr-output-preferred-mode wlr-output)\n\n"
        "Retrieves a preferred mode for an output object."
    },
    {
        "wlr-output-set-mode", cfun_wlr_output_set_mode,
        "(" MOD_NAME "/wlr-output-set-mode wlr-output wlr-output-mode)\n\n"
        "Sets a mode for an output object."
    },
    {
        "wlr-output-enable", cfun_wlr_output_enable,
        "(" MOD_NAME "/wlr-output-enable wlr-output bool)\n\n"
        "Sets whether an output object is enabled or not."
    },
    {
        "wlr-output-commit", cfun_wlr_output_commit,
        "(" MOD_NAME "/wlr-output-commit wlr-output)\n\n"
        "Commits an output object."
    },
    {
        "wlr-output-layout-add-auto", cfun_wlr_output_layout_add_auto,
        "(" MOD_NAME "/wlr-output-layout-add-auto wlr-output-layout wlr-output)\n\n"
        "Adds an output object to an output layout."
    },
    {
        "wlr-xdg-surface-from-wlr-surface", cfun_wlr_xdg_surface_from_wlr_surface,
        "(" MOD_NAME "/wlr-xdg-surface-from-wlr-surface wlr-surface)\n\n"
        "Wraps a surface object with an xdg surface object."
    },
    {
        "wlr-scene-xdg-surface-create", cfun_wlr_scene_xdg_surface_create,
        "(" MOD_NAME "/wlr-scene-xdg-surface-create wlr-scene-tree xdg-surface)\n\n"
        "Adds a node displaying an xdg_surface and all of its sub-surfaces to the scene-graph."
    },
    {
        "wlr-scene-get-scene-output", cfun_wlr_scene_get_scene_output,
        "(" MOD_NAME "/wlr-scene-get-scene-output wlr-scene wlr-output)\n\n"
        "Converts a wl-output object to a wl-scene-output object."
    },
    {
        "wlr-scene-output-commit", cfun_wlr_scene_output_commit,
        "(" MOD_NAME "/wlr-scene-output-commit wlr-scene-output)\n\n"
        "Commits a wlr-scene-output."
    },
    {
        "wlr-scene-output-send-frame-done", cfun_wlr_scene_output_send_frame_done,
        "(" MOD_NAME "/wlr-scene-output-send-frame-done wlr-scene-output timespec)\n\n"
        "Marks that we are done with the output frame."
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
    janet_register_abstract_type(&jwlr_at_wlr_output_layout_output);
    janet_register_abstract_type(&jwlr_at_wlr_scene);
    janet_register_abstract_type(&jwlr_at_wlr_scene_output);
    janet_register_abstract_type(&jwlr_at_wlr_scene_tree);
    janet_register_abstract_type(&jwlr_at_wlr_scene_node);
    janet_register_abstract_type(&jwlr_at_wlr_xdg_shell);
    janet_register_abstract_type(&jwlr_at_wlr_surface);
    janet_register_abstract_type(&jwlr_at_wlr_xdg_surface);
    janet_register_abstract_type(&jwlr_at_wlr_xdg_toplevel);
    janet_register_abstract_type(&jwlr_at_wlr_xdg_popup);
    janet_register_abstract_type(&jwlr_at_wlr_cursor);
    janet_register_abstract_type(&jwlr_at_wlr_xcursor_manager);
    janet_register_abstract_type(&jwlr_at_wlr_seat);
    janet_register_abstract_type(&jwlr_at_wlr_output);
    janet_register_abstract_type(&jwlr_at_wlr_output_mode);
    janet_register_abstract_type(&jwlr_at_wlr_output_cursor);
    janet_register_abstract_type(&jwlr_at_wlr_input_device);

    janet_cfuns(env, MOD_NAME, cfuns);
}
