#include <stdio.h>
#include <assert.h>

#include <janet.h>

#include <wayland-server-core.h>

#include <wlr/util/log.h>
#include <wlr/backend.h>
#include <wlr/xwayland.h>
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

#include <xkbcommon/xkbcommon.h>

#include "jl.h"
#include "types.h"
#include "wlr_abs_types.h"


#ifndef MOD_NAME
#define MOD_NAME WLR_MOD_NAME
#endif


JANET_THREAD_LOCAL JanetFunction *jwlr_log_callback_fn;

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


static int method_wlr_abs_obj_compare(void *lhs, void *rhs)
{
    void **left_p = (void **)lhs;
    void *left = *left_p;

    void **right_p = (void **)rhs;
    void *right = *right_p;

    if (left == right) {
        return 0;
    } else {
        return left > right ? 1 : -1;
    }
}


static const jl_key_def_t log_defs[] = {
    {"silent", WLR_SILENT},
    {"error", WLR_ERROR},
    {"info", WLR_INFO},
    {"debug", WLR_DEBUG},
    {"log-importance-last", WLR_LOG_IMPORTANCE_LAST},
    {NULL, 0},
};


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
    int locked = janet_gclock();
    int sig = janet_pcall(jwlr_log_callback_fn, 2, argv, &ret, &fiber);
    janet_gcunlock(locked);
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

    verbosity = jl_get_key_def(argv, 0, log_defs);
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

    verb = jl_get_key_def(argv, 0, log_defs);
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


static int method_box_get(void *p, Janet key, Janet *out)
{
    struct wlr_box *box = (struct wlr_box *)p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "x")) {
        *out = janet_wrap_integer(box->x);
        return 1;
    }
    if (!janet_cstrcmp(kw, "y")) {
        *out = janet_wrap_integer(box->y);
        return 1;
    }
    if (!janet_cstrcmp(kw, "width")) {
        *out = janet_wrap_integer(box->width);
        return 1;
    }
    if (!janet_cstrcmp(kw, "height")) {
        *out = janet_wrap_integer(box->height);
        return 1;
    }

    return 0;
}


static void method_box_put(void *p, Janet key, Janet value) {
    struct wlr_box *box = (struct wlr_box *)p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }
    if (!janet_checkint(value)) {
        janet_panicf("expected a 32-bit signed integer, got %v", value);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);
    int *member_p = NULL;
    if (!janet_cstrcmp(kw, "x")) {
        member_p = &box->x;
    } else if (!janet_cstrcmp(kw, "y")) {
        member_p = &box->y;
    } else if (!janet_cstrcmp(kw, "width")) {
        member_p = &box->width;
    } else if (!janet_cstrcmp(kw, "height")) {
        member_p = &box->height;
    } else {
        janet_panicf("unknown key: %v", key);
    }
    *member_p = janet_unwrap_integer(value);
}


static Janet cfun_box(int32_t argc, Janet *argv)
{
    if (argc & 0x01) {
        janet_panicf("expected even number of arguments, got %d", argc);
    }

    struct wlr_box *box = janet_abstract(&jwlr_at_box, sizeof(*box));
    memset(box, 0, sizeof(*box));

    for (int32_t k = 0, v = 1; k < argc; k += 2, v += 2) {
        const uint8_t *kw = janet_getkeyword(argv, k);
        if (!janet_cstrcmp(kw, "x")) {
            box->x = janet_getinteger(argv, v);
        } else if (!janet_cstrcmp(kw, "y")) {
            box->y = janet_getinteger(argv, v);
        } else if (!janet_cstrcmp(kw, "width")) {
            box->width = janet_getinteger(argv, v);
        } else if (!janet_cstrcmp(kw, "height")) {
            box->height = janet_getinteger(argv, v);
        }
    }

    return janet_wrap_abstract(box);
}


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


static Janet cfun_wlr_scene_node_at(int32_t argc, Janet *argv)
{
    struct wlr_scene_node *node;
    double x, y;

    struct wlr_scene_node *n_node;
    double nx = 0, ny = 0;
    Janet ret_tuple[3];

    janet_fixarity(argc, 3);

    node = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_scene_node);
    x = janet_getnumber(argv, 1);
    y = janet_getnumber(argv, 2);

    n_node = wlr_scene_node_at(node, x, y, &nx, &ny);
    if (n_node) {
        ret_tuple[0] = janet_wrap_abstract(jl_pointer_to_abs_obj(n_node, &jwlr_at_wlr_scene_node));
    } else {
        ret_tuple[0] = janet_wrap_nil();
    }
    ret_tuple[1] = janet_wrap_number(nx);
    ret_tuple[2] = janet_wrap_number(ny);

    return janet_wrap_tuple(janet_tuple_n(ret_tuple, 3));
}


static Janet cfun_wlr_scene_node_raise_to_top(int32_t argc, Janet *argv)
{
    struct wlr_scene_node *node;

    janet_fixarity(argc, 1);

    node = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_scene_node);
    wlr_scene_node_raise_to_top(node);
    return janet_wrap_nil();
}


static Janet cfun_wlr_scene_node_set_position(int32_t argc, Janet *argv)
{
    struct wlr_scene_node *node;
    int x, y;

    janet_fixarity(argc, 3);

    node = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_scene_node);
    x = janet_getinteger(argv, 1);
    y = janet_getinteger(argv, 2);

    wlr_scene_node_set_position(node, x, y);
    return janet_wrap_nil();
}


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


static int method_wlr_xdg_toplevel_resize_event_get(void *p, Janet key, Janet *out) {
    struct wlr_xdg_toplevel_resize_event **event_p = (struct wlr_xdg_toplevel_resize_event **)p;
    struct wlr_xdg_toplevel_resize_event *event = *event_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "toplevel")) {
        if (!(event->toplevel)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(event->toplevel, &jwlr_at_wlr_xdg_toplevel));
        return 1;
    }
    if (!janet_cstrcmp(kw, "seat")) {
        if (!(event->seat)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(event->seat, &jwlr_at_wlr_seat_client));
        return 1;
    }
    if (!janet_cstrcmp(kw, "serial")) {
        /* XXX: uint32_t -> int32_t conversion */
        *out = janet_wrap_integer(event->serial);
        return 1;
    }
    if (!janet_cstrcmp(kw, "edges")) {
        *out = janet_wrap_array(jl_get_flag_keys(event->edges, wlr_edges_defs));
        return 1;
    }

    return 0;
}


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
    if (!janet_cstrcmp(kw, "surface")) {
        if (!(surface->surface)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(surface->surface, &jwlr_at_wlr_surface));
        return 1;
    }
    if (!janet_cstrcmp(kw, "data")) {
        if (!(surface->data)) {
            *out = janet_wrap_nil();
            return 1;
        }
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

    janet_panicf("unknown key: %v", key);
}


static int method_wlr_cursor_get(void *p, Janet key, Janet *out) {
    struct wlr_cursor **cursor_p = (struct wlr_cursor **)p;
    struct wlr_cursor *cursor = *cursor_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(cursor, kw, wlr_cursor_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

    if (!janet_cstrcmp(kw, "x")) {
        *out = janet_wrap_number(cursor->x);
        return 1;
    }
    if (!janet_cstrcmp(kw, "y")) {
        *out = janet_wrap_number(cursor->y);
        return 1;
    }
    if (!janet_cstrcmp(kw, "data")) {
        if (!(cursor->data)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_pointer(cursor->data);
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


static Janet cfun_wlr_cursor_move(int32_t argc, Janet *argv)
{
    struct wlr_cursor *cursor;
    struct wlr_input_device *dev;
    double dx, dy;

    janet_fixarity(argc, 4);

    cursor = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_cursor);
    dev = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_input_device);
    dx = janet_getnumber(argv, 2);
    dy = janet_getnumber(argv, 3);

    wlr_cursor_move(cursor, dev, dx, dy);
    return janet_wrap_nil();
}


static Janet cfun_wlr_cursor_warp_absolute(int32_t argc, Janet *argv)
{
    struct wlr_cursor *cursor;
    struct wlr_input_device *dev;
    double x, y;

    janet_fixarity(argc, 4);

    cursor = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_cursor);
    dev = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_input_device);
    x = janet_getnumber(argv, 2);
    y = janet_getnumber(argv, 3);

    wlr_cursor_warp_absolute(cursor, dev, x, y);
    return janet_wrap_nil();
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


static Janet cfun_wlr_cursor_attach_input_device(int32_t argc, Janet *argv)
{
    struct wlr_cursor *cursor;
    struct wlr_input_device *device;

    janet_fixarity(argc, 2);

    cursor = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_cursor);
    device = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_input_device);
    wlr_cursor_attach_input_device(cursor, device);

    return janet_wrap_nil();
}


static Janet cfun_wlr_cursor_set_surface(int32_t argc, Janet *argv)
{
    struct wlr_cursor *cursor;
    struct wlr_surface *surface;
    int32_t hotspot_x, hotspot_y;

    janet_fixarity(argc, 4);

    cursor = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_cursor);
    surface = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_surface);
    hotspot_x = janet_getinteger(argv, 2);
    hotspot_y = janet_getinteger(argv, 3);

    wlr_cursor_set_surface(cursor, surface, hotspot_x, hotspot_y);
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


static int method_wlr_xcursor_image_get(void *p, Janet key, Janet *out)
{
    struct wlr_xcursor_image **image_p = (struct wlr_xcursor_image **)p;
    struct wlr_xcursor_image *image = *image_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "width")) {
        /* uint32_t -> uint64_t conversion */
        *out = janet_wrap_u64(image->width);
        return 1;
    }
    if (!janet_cstrcmp(kw, "height")) {
        /* uint32_t -> uint64_t conversion */
        *out = janet_wrap_u64(image->height);
        return 1;
    }
    if (!janet_cstrcmp(kw, "hotspot-x")) {
        /* XXX: wlr-xwayland-set-cursor accepts a signed 32-bit value,
           but the member in wlr_xcursor_image is a uint32_t.
           We do the conversion here, and assume the coordinates won't
           overflow a int32_t. */
        /* uint32_t -> int32_t conversion */
        *out = janet_wrap_integer(image->hotspot_x);
        return 1;
    }
    if (!janet_cstrcmp(kw, "hotspot-y")) {
        /* uint32_t -> int32_t conversion */
        *out = janet_wrap_integer(image->hotspot_y);
        return 1;
    }
    if (!janet_cstrcmp(kw, "delay")) {
        /* uint32_t -> uint64_t conversion */
        *out = janet_wrap_u64(image->delay);
        return 1;
    }
    if (!janet_cstrcmp(kw, "buffer")) {
        /* See xcursor_create_from_data() in wlroots for size calculation. */
        uint32_t buf_len = image->width * image->height * sizeof(uint32_t);
        /* XXX: uint32_t -> int32_t conversion */
        int32_t signed_len = (int32_t)buf_len;
        assert(signed_len >= 0);
        JanetBuffer *buf = janet_buffer(signed_len);
        janet_buffer_push_bytes(buf, image->buffer, signed_len);
        *out = janet_wrap_buffer(buf);
        return 1;
    }

    return 0;
}


static int method_wlr_xcursor_get(void *p, Janet key, Janet *out)
{
    struct wlr_xcursor **xcursor_p = (struct wlr_xcursor **)p;
    struct wlr_xcursor *xcursor = *xcursor_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "image-count")) {
        /* unsigned int -> uint64_t conversion */
        *out = janet_wrap_u64(xcursor->image_count);
        return 1;
    }
    if (!janet_cstrcmp(kw, "images")) {
        if (!(xcursor->images)) {
            *out = janet_wrap_nil();
            return 1;
        }
        unsigned int count = xcursor->image_count;
        JanetArray *img_arr = janet_array(xcursor->image_count);
        for (unsigned int i = 0; i < count; i++) {
            struct wlr_xcursor_image **image_p =
                (struct wlr_xcursor_image **)jl_pointer_to_abs_obj(xcursor->images[i],
                                                                   &jwlr_at_wlr_xcursor_image);
            janet_array_push(img_arr, janet_wrap_abstract(image_p));
        }
        *out = janet_wrap_array(img_arr);
        return 1;
    }
    if (!janet_cstrcmp(kw, "name")) {
        if (!(xcursor->name)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_cstringv(xcursor->name);
        return 1;
    }
    if (!janet_cstrcmp(kw, "total-delay")) {
        /* uint32_t -> uint64_t conversion */
        *out = janet_wrap_u64(xcursor->total_delay);
        return 1;
    }

    return 0;
}


static Janet cfun_wlr_xcursor_manager_get_xcursor(int32_t argc, Janet *argv)
{
    struct wlr_xcursor_manager *manager;
    const char *name;
    float scale;

    struct wlr_xcursor *xcursor;

    janet_fixarity(argc, 3);

    manager = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xcursor_manager);
    name = janet_getcstring(argv, 1);
    /* XXX: double -> float conversion */
    scale = (float)janet_getnumber(argv, 2);

    xcursor = wlr_xcursor_manager_get_xcursor(manager, name, scale);
    if (!xcursor) {
        janet_panicf("failed to find cursor: %s", name);
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(xcursor, &jwlr_at_wlr_xcursor));
}


static Janet cfun_wlr_xcursor_manager_set_cursor_image(int32_t argc, Janet *argv)
{
    struct wlr_xcursor_manager *manager;
    const char *name;
    struct wlr_cursor *cursor;

    janet_fixarity(argc, 3);

    manager = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xcursor_manager);
    name = janet_getcstring(argv, 1);
    cursor = jl_get_abs_obj_pointer(argv, 2, &jwlr_at_wlr_cursor);

    wlr_xcursor_manager_set_cursor_image(manager, name, cursor);
    return janet_wrap_nil();
}


static Janet cfun_wlr_keyboard_from_input_device(int32_t argc, Janet *argv)
{
    struct wlr_input_device *device;

    struct wlr_keyboard *keyboard;

    janet_fixarity(argc, 1);

    device = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_input_device);
    keyboard = wlr_keyboard_from_input_device(device);
    if (!keyboard) {
        janet_panic("failed to get keyboard object from input device");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(keyboard, &jwlr_at_wlr_keyboard));
}


static Janet cfun_wlr_keyboard_set_keymap(int32_t argc, Janet *argv)
{
    struct wlr_keyboard *keyboard;
    struct xkb_keymap *keymap;

    janet_fixarity(argc, 2);

    keyboard = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_keyboard);
    keymap = jl_get_abs_obj_pointer_by_name(argv, 1, XKB_MOD_NAME "/xkb-keymap");

    return janet_wrap_boolean(wlr_keyboard_set_keymap(keyboard, keymap));
}


static Janet cfun_wlr_keyboard_set_repeat_info(int32_t argc, Janet *argv)
{
    struct wlr_keyboard *keyboard;
    int32_t rate, delay;

    janet_fixarity(argc, 3);

    keyboard = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_keyboard);
    rate = janet_getinteger(argv, 1);
    delay = janet_getinteger(argv, 2);

    wlr_keyboard_set_repeat_info(keyboard, rate, delay);
    return janet_wrap_nil();
}


static Janet cfun_wlr_keyboard_get_modifiers(int32_t argc, Janet *argv)
{
    struct wlr_keyboard *keyboard;

    uint32_t modifiers;

    janet_fixarity(argc, 1);

    keyboard = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_keyboard);
    modifiers = wlr_keyboard_get_modifiers(keyboard);

    return janet_wrap_array(jl_get_flag_keys(modifiers, wlr_keyboard_modifier_defs));
}


static int method_wlr_seat_get(void *p, Janet key, Janet *out) {
    struct wlr_seat **seat_p = (struct wlr_seat **)p;
    struct wlr_seat *seat = *seat_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(seat, kw, wlr_seat_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

    if (!janet_cstrcmp(kw, "pointer-state")) {
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(&seat->pointer_state, &jwlr_at_wlr_seat_pointer_state));
        return 1;
    }
    if (!janet_cstrcmp(kw, "keyboard-state")) {
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(&seat->keyboard_state, &jwlr_at_wlr_seat_keyboard_state));
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


static Janet cfun_wlr_seat_set_keyboard(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;
    struct wlr_keyboard *keyboard;

    janet_fixarity(argc, 2);

    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    keyboard = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_keyboard);

    wlr_seat_set_keyboard(seat, keyboard);
    return janet_wrap_nil();
}


static Janet cfun_wlr_seat_keyboard_notify_modifiers(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;
    struct wlr_keyboard_modifiers *modifiers;

    janet_fixarity(argc, 2);

    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    modifiers = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_keyboard_modifiers);

    wlr_seat_keyboard_notify_modifiers(seat, modifiers);
    return janet_wrap_nil();
}


static Janet cfun_wlr_seat_keyboard_notify_key(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;
    uint32_t time;
    uint32_t keycode;
    uint32_t state;

    janet_fixarity(argc, 4);

    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    time = (uint32_t)janet_getuinteger64(argv, 1);
    keycode = (uint32_t)janet_getuinteger64(argv, 2);
    /* int32_t -> uint32_t conversion */
    state = (uint32_t)jl_get_key_def(argv, 3, wl_keyboard_key_state_defs);

    wlr_seat_keyboard_notify_key(seat, time, keycode, state);
    return janet_wrap_nil();
}


static Janet cfun_wlr_seat_keyboard_notify_enter(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;
    struct wlr_surface *surface;
    JanetView kc_view;
    struct wlr_keyboard_modifiers *modifiers;

    uint32_t keycodes[WLR_KEYBOARD_KEYS_CAP];

    janet_fixarity(argc, 4);

    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    surface = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_surface);
    kc_view = janet_getindexed(argv, 2);
    modifiers = jl_get_abs_obj_pointer(argv, 3, &jwlr_at_wlr_keyboard_modifiers);

    if (kc_view.len > WLR_KEYBOARD_KEYS_CAP) {
        janet_panicf("expected at most %d key codes, got %d", WLR_KEYBOARD_KEYS_CAP, kc_view.len);
    }

    for (int i = 0; i < kc_view.len; i++) {
        /* uint64_t -> uint32_t conversion */
        keycodes[i] = (uint32_t)janet_unwrap_u64(kc_view.items[i]);
    }

    wlr_seat_keyboard_notify_enter(seat, surface, keycodes, kc_view.len, modifiers);
    return janet_wrap_nil();
}


static Janet cfun_wlr_seat_keyboard_notify_clear_focus(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;

    janet_fixarity(argc, 1);

    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    wlr_seat_keyboard_notify_clear_focus(seat);
    return janet_wrap_nil();
}


static Janet cfun_wlr_seat_pointer_notify_button(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;
    uint32_t time;
    uint32_t button;
    enum wlr_button_state state;

    uint32_t serial;

    janet_fixarity(argc, 4);

    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    /* uint64_t -> uint32_t conversion */
    time = (uint32_t)janet_getuinteger64(argv, 1);
    button = (uint32_t)janet_getuinteger64(argv, 2);
    state = jl_get_key_def(argv, 3, wlr_button_state_defs);

    serial = wlr_seat_pointer_notify_button(seat, time, button, state);
    /* XXX: Janet doesn't have a 32-bit unsigned type, so this converts
       serial to int32_t. It should be fine as long as we don't do
       arithmetic on the returned serial numbers */
    return janet_wrap_integer(serial);
}


static Janet cfun_wlr_seat_pointer_notify_axis(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;
    uint32_t time;
    enum wlr_axis_orientation orientation;
    double value;
    int32_t value_discrete;
    enum wlr_axis_source source;

    janet_fixarity(argc, 6);

    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    /* uint64_t -> uint32_t conversion */
    time = (uint32_t)janet_getuinteger64(argv, 1);
    orientation = jl_get_key_def(argv, 2, wlr_axis_orientation_defs);
    value = janet_getnumber(argv, 3);
    value_discrete = janet_getinteger(argv, 4);
    source = jl_get_key_def(argv, 5, wlr_axis_source_defs);

    wlr_seat_pointer_notify_axis(seat, time, orientation, value, value_discrete, source);
    return janet_wrap_nil();
}


static Janet cfun_wlr_seat_pointer_notify_frame(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;

    janet_fixarity(argc, 1);

    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    wlr_seat_pointer_notify_frame(seat);
    return janet_wrap_nil();
}


static Janet cfun_wlr_seat_pointer_notify_enter(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;
    struct wlr_surface *surface;
    double sx, sy;

    janet_fixarity(argc, 4);

    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    surface = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_surface);
    sx = janet_getnumber(argv, 2);
    sy = janet_getnumber(argv, 3);

    wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
    return janet_wrap_nil();
}


static Janet cfun_wlr_seat_pointer_notify_motion(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;
    uint32_t time;
    double sx, sy;

    janet_fixarity(argc, 4);

    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    time = (uint32_t)janet_getuinteger64(argv, 1);
    sx = janet_getnumber(argv, 2);
    sy = janet_getnumber(argv, 3);

    wlr_seat_pointer_notify_motion(seat, time, sx, sy);
    return janet_wrap_nil();
}


static Janet cfun_wlr_seat_pointer_notify_clear_focus(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;

    janet_fixarity(argc, 1);

    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    wlr_seat_pointer_notify_clear_focus(seat);
    return janet_wrap_nil();
}


static Janet cfun_wlr_seat_pointer_clear_focus(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;

    janet_fixarity(argc, 1);

    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    wlr_seat_pointer_clear_focus(seat);
    return janet_wrap_nil();
}


static Janet cfun_wlr_seat_set_capabilities(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;
    uint32_t caps;

    janet_fixarity(argc, 2);
    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    caps = jl_get_key_flags(argv, 1, wl_seat_capability_defs);

    wlr_seat_set_capabilities(seat, caps);

    return janet_wrap_nil();
}


static Janet cfun_wlr_seat_set_selection(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;
    struct wlr_data_source *source;
    uint32_t serial;

    janet_fixarity(argc, 3);

    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    if (janet_checktype(argv[1], JANET_NIL)) {
        source = NULL;
    } else {
        source = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_data_source);
    }
    /* XXX: int32_t -> uint32_t conversion */
    serial = (uint32_t)janet_getinteger(argv, 2);

    wlr_seat_set_selection(seat, source, serial);
    return janet_wrap_nil();
}


static Janet cfun_wlr_seat_get_keyboard(int32_t argc, Janet *argv)
{
    struct wlr_seat *seat;

    struct wlr_keyboard *keyboard;

    janet_fixarity(argc, 1);

    seat = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_seat);
    keyboard = wlr_seat_get_keyboard(seat);

    if (!keyboard) {
        return janet_wrap_nil();
    } else {
        return janet_wrap_abstract(jl_pointer_to_abs_obj(keyboard, &jwlr_at_wlr_keyboard));
    }
}


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


static int method_wlr_output_mode_get(void *p, Janet key, Janet *out) {
    struct wlr_output_mode **mode_p = (struct wlr_output_mode **)p;
    struct wlr_output_mode *mode = *mode_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "width")) {
        *out = janet_wrap_integer(mode->width);
        return 1;
    }
    if (!janet_cstrcmp(kw, "height")) {
        *out = janet_wrap_integer(mode->height);
        return 1;
    }
    if (!janet_cstrcmp(kw, "refresh")) {
        *out = janet_wrap_integer(mode->refresh);
        return 1;
    }
    if (!janet_cstrcmp(kw, "preferred")) {
        *out = janet_wrap_boolean(mode->preferred);
        return 1;
    }
    if (!janet_cstrcmp(kw, "picture-aspect-ratio")) {
        if (mode->picture_aspect_ratio > __WLR_OUTPUT_MODE_ASPECT_RATIO_MAX) {
            janet_panicf("unknown aspect ration from wlroots output mode: %d", mode->picture_aspect_ratio);
        }
        *out = janet_ckeywordv(wlr_output_mode_aspect_ratio_defs[mode->picture_aspect_ratio].name);
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

    janet_fixarity(argc, 1);

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


static Janet cfun_wlr_output_layout_get_box(int32_t argc, Janet *argv)
{
    struct wlr_output_layout *layout;
    struct wlr_output *output;

    struct wlr_box *box;

    janet_fixarity(argc, 2);

    layout = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_output_layout);
    if (janet_checktype(argv[1], JANET_NIL)) {
        output = NULL;
    } else {
        output = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_output);
    }
    box = janet_abstract(&jwlr_at_box, sizeof(*box));

    wlr_output_layout_get_box(layout, output, box);
    return janet_wrap_abstract(box);
}


static Janet cfun_wlr_surface_get_root_surface(int32_t argc, Janet *argv)
{
    struct wlr_surface *surface;

    struct wlr_surface *root;

    janet_fixarity(argc, 1);

    surface = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_surface);
    root = wlr_surface_get_root_surface(surface);
    if (!root) {
        return janet_wrap_nil();
    } else {
        return janet_wrap_abstract(jl_pointer_to_abs_obj(root, &jwlr_at_wlr_surface));
    }
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


static Janet cfun_wlr_xdg_surface_schedule_configure(int32_t argc, Janet *argv)
{
    struct wlr_xdg_surface *surface;

    uint32_t serial;

    janet_fixarity(argc, 1);

    surface = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xdg_surface);
    serial = wlr_xdg_surface_schedule_configure(surface);

    /* XXX: Janet doesn't have a 32-bit unsigned type, so this converts
       serial to int32_t. It should be fine as long as we don't do
       arithmetic on the returned serial numbers */
    return janet_wrap_integer(serial);
}


static Janet cfun_wlr_xdg_surface_get_geometry(int32_t argc, Janet *argv)
{
    struct wlr_xdg_surface *surface;

    struct wlr_box *box;

    janet_fixarity(argc, 1);

    surface = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xdg_surface);
    box = janet_abstract(&jwlr_at_box, sizeof(*box));

    wlr_xdg_surface_get_geometry(surface, box);
    return janet_wrap_abstract(box);
}


static Janet cfun_wlr_xdg_toplevel_set_activated(int32_t argc, Janet *argv)
{
    struct wlr_xdg_toplevel *toplevel;
    bool activated;

    uint32_t serial;

    janet_fixarity(argc, 2);

    toplevel = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xdg_toplevel);
    activated = janet_getboolean(argv, 1);
    serial = wlr_xdg_toplevel_set_activated(toplevel, activated);

    /* XXX: Janet doesn't have a 32-bit unsigned type, so this converts
       serial to int32_t. It should be fine as long as we don't do
       arithmetic on the returned serial numbers */
    return janet_wrap_integer(serial);
}


static Janet cfun_wlr_xdg_toplevel_set_maximized(int32_t argc, Janet *argv)
{
    struct wlr_xdg_toplevel *toplevel;
    bool maximized;

    uint32_t serial;

    janet_fixarity(argc, 2);

    toplevel = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xdg_toplevel);
    maximized = janet_getboolean(argv, 1);
    serial = wlr_xdg_toplevel_set_maximized(toplevel, maximized);

    /* XXX: uint32_t -> int32_t conversion */
    return janet_wrap_integer(serial);
}


static Janet cfun_wlr_xdg_toplevel_set_fullscreen(int32_t argc, Janet *argv)
{
    struct wlr_xdg_toplevel *toplevel;
    bool fullscreen;

    uint32_t serial;

    janet_fixarity(argc, 2);

    toplevel = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xdg_toplevel);
    fullscreen = janet_getboolean(argv, 1);
    serial = wlr_xdg_toplevel_set_fullscreen(toplevel, fullscreen);

    /* XXX: uint32_t -> int32_t conversion */
    return janet_wrap_integer(serial);
}


static Janet cfun_wlr_xdg_toplevel_set_resizing(int32_t argc, Janet *argv)
{
    struct wlr_xdg_toplevel *toplevel;
    bool resizing;

    uint32_t serial;

    janet_fixarity(argc, 2);

    toplevel = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xdg_toplevel);
    resizing = janet_getboolean(argv, 1);
    serial = wlr_xdg_toplevel_set_resizing(toplevel, resizing);

    /* XXX: uint32_t -> int32_t conversion */
    return janet_wrap_integer(serial);
}


static Janet cfun_wlr_xdg_toplevel_set_tiled(int32_t argc, Janet *argv)
{
    struct wlr_xdg_toplevel *toplevel;
    uint32_t tiled_edges;

    uint32_t serial;

    janet_fixarity(argc, 2);

    toplevel = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xdg_toplevel);
    tiled_edges = jl_get_key_flags(argv, 1, wlr_edges_defs);
    serial = wlr_xdg_toplevel_set_tiled(toplevel, tiled_edges);

    /* XXX: uint32_t -> int32_t conversion */
    return janet_wrap_integer(serial);
}


static Janet cfun_wlr_xdg_toplevel_set_size(int32_t argc, Janet *argv)
{
    struct wlr_xdg_toplevel *toplevel;
    int32_t width, height;

    uint32_t serial;

    janet_fixarity(argc, 3);

    toplevel = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xdg_toplevel);
    width = janet_getinteger(argv, 1);
    height = janet_getinteger(argv, 2);
    serial = wlr_xdg_toplevel_set_size(toplevel, width, height);

    /* XXX: uint32_t -> int32_t conversion */
    return janet_wrap_integer(serial);
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

    if (!janet_cstrcmp(kw, "type")) {
        *out = janet_ckeywordv(wlr_scene_node_type_defs[node->type].name);
        return 1;
    }
    if (!janet_cstrcmp(kw, "parent")) {
        if (!(node->parent)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(node->parent, &jwlr_at_wlr_scene_tree));
        return 1;
    }
    if (!janet_cstrcmp(kw, "enabled")) {
        *out = janet_wrap_boolean(node->enabled);
        return 1;
    }
    if (!janet_cstrcmp(kw, "x")) {
        *out = janet_wrap_integer(node->x);
        return 1;
    }
    if (!janet_cstrcmp(kw, "y")) {
        *out = janet_wrap_integer(node->y);
        return 1;
    }
    if (!janet_cstrcmp(kw, "data")) {
        if (!(node->data)) {
            *out = janet_wrap_nil();
            return 1;
        }
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

    janet_panicf("unknown key: %v", key);
}

static int method_wlr_input_device_get(void *p, Janet key, Janet *out)
{
    struct wlr_input_device **device_p = (struct wlr_input_device **)p;
    struct wlr_input_device *device = *device_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(device, kw, wlr_input_device_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

    if (!janet_cstrcmp(kw, "type")) {
        *out = janet_ckeywordv(wlr_input_device_defs[device->type].name);
        return 1;
    }
    if (!janet_cstrcmp(kw, "vendor")) {
        /* XXX: unsigned int -> int32_t conversion */
        *out = janet_wrap_integer(device->vendor);
        return 1;
    }
    if (!janet_cstrcmp(kw, "product")) {
        /* XXX: unsigned int -> int32_t conversion */
        *out = janet_wrap_integer(device->product);
        return 1;
    }
    if (!janet_cstrcmp(kw, "name")) {
        if (!(device->name)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_cstringv(device->name);
        return 1;
    }
    if (!janet_cstrcmp(kw, "data")) {
        if (!(device->data)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_pointer(device->data);
        return 1;
    }

    return 0;
}


static int method_wlr_pointer_get(void *p, Janet key, Janet *out)
{
    struct wlr_pointer **pointer_p = (struct wlr_pointer **)p;
    struct wlr_pointer *pointer = *pointer_p;
    
    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(pointer, kw, wlr_pointer_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

    if (!janet_cstrcmp(kw, "base")) {
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(&pointer->base, &jwlr_at_wlr_input_device));
        return 1;
    }
    if (!janet_cstrcmp(kw, "output-name")) {
        *out = janet_cstringv(pointer->output_name);
        return 1;
    }
    if (!janet_cstrcmp(kw, "data")) {
        if (!(pointer->data)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_pointer(pointer->data);
        return 1;
    }

    return 0;
}


static int method_wlr_keyboard_modifiers_get(void *p, Janet key, Janet *out)
{
    struct wlr_keyboard_modifiers **modifiers_p = (struct wlr_keyboard_modifiers **)p;
    struct wlr_keyboard_modifiers *modifiers = *modifiers_p;
    
    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);
    xkb_mod_mask_t *member_p;

    if (!janet_cstrcmp(kw, "depressed")) {
        member_p = &modifiers->depressed;
    } else if (!janet_cstrcmp(kw, "latched")) {
        member_p = &modifiers->latched;
    } else if (!janet_cstrcmp(kw, "locked")) {
        member_p = &modifiers->locked;
    } else if (!janet_cstrcmp(kw, "group")) {
        member_p = &modifiers->group;
    } else {
        return 0;
    }

    *out = janet_wrap_array(jl_get_flag_keys(*member_p, wlr_keyboard_modifier_defs));
    return 1;
}


static int method_wlr_keyboard_key_event_get(void *p, Janet key, Janet *out)
{
    struct wlr_keyboard_key_event **event_p = (struct wlr_keyboard_key_event **)p;
    struct wlr_keyboard_key_event *event = *event_p;
    
    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "time-msec")) {
        /* uint32_t -> uint64_t */
        *out = janet_wrap_u64(event->time_msec);
        return 1;
    }
    if (!janet_cstrcmp(kw, "keycode")) {
        /* uint32_t -> uint64_t */
        *out = janet_wrap_u64(event->keycode);
        return 1;
    }
    if (!janet_cstrcmp(kw, "update-state")) {
        *out = janet_wrap_boolean(event->update_state);
        return 1;
    }
    if (!janet_cstrcmp(kw, "state")) {
        *out = janet_ckeywordv(wl_keyboard_key_state_defs[event->state].name);
        return 1;
    }

    return 0;
}


static int method_wlr_keyboard_get(void *p, Janet key, Janet *out)
{
    struct wlr_keyboard **keyboard_p = (struct wlr_keyboard **)p;
    struct wlr_keyboard *keyboard = *keyboard_p;
    
    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(keyboard, kw, wlr_keyboard_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

    if (!janet_cstrcmp(kw, "base")) {
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(&keyboard->base, &jwlr_at_wlr_input_device));
        return 1;
    }
    if (!janet_cstrcmp(kw, "keymap-string")) {
        if (!(keyboard->keymap_string)) {
            *out = janet_wrap_nil();
            return 1;
        }
        JanetBuffer *buf = janet_buffer(keyboard->keymap_size);
        janet_buffer_push_bytes(buf, (uint8_t *)keyboard->keymap_string, keyboard->keymap_size);
        *out = janet_wrap_buffer(buf);
        return 1;
    }
    if (!janet_cstrcmp(kw, "xkb-state")) {
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj_by_name(keyboard->xkb_state,
                                                                 XKB_MOD_NAME "/xkb-state"));
        return 1;
    }
    if (!janet_cstrcmp(kw, "keycodes")) {
        JanetArray *kc_arr = janet_array(keyboard->num_keycodes);
        for (size_t i = 0; i < keyboard->num_keycodes; i++) {
            janet_array_push(kc_arr, janet_wrap_u64(keyboard->keycodes[i]));
        }
        *out = janet_wrap_array(kc_arr);
        return 1;
    }
    if (!janet_cstrcmp(kw, "modifiers")) {
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(&keyboard->modifiers,
                                                         &jwlr_at_wlr_keyboard_modifiers));
        return 1;
    }
    if (!janet_cstrcmp(kw, "data")) {
        if (!(keyboard->data)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_pointer(keyboard->data);
        return 1;
    }

    return 0;
}


static int method_wlr_pointer_motion_event_get(void *p, Janet key, Janet *out)
{
    struct wlr_pointer_motion_event **event_p = (struct wlr_pointer_motion_event **)p;
    struct wlr_pointer_motion_event *event = *event_p;
    
    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "pointer")) {
        if (!(event->pointer)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(event->pointer, &jwlr_at_wlr_pointer));
        return 1;
    }
    if (!janet_cstrcmp(kw, "time-msec")) {
        /* uint32_t -> uint64_t */
        *out = janet_wrap_u64(event->time_msec);
        return 1;
    }
    if (!janet_cstrcmp(kw, "delta-x")) {
        *out = janet_wrap_number(event->delta_x);
        return 1;
    }
    if (!janet_cstrcmp(kw, "delta-y")) {
        *out = janet_wrap_number(event->delta_y);
        return 1;
    }
    if (!janet_cstrcmp(kw, "unaccel-dx")) {
        *out = janet_wrap_number(event->unaccel_dx);
        return 1;
    }
    if (!janet_cstrcmp(kw, "unaccel-dy")) {
        *out = janet_wrap_number(event->unaccel_dy);
        return 1;
    }

    return 0;
}


static int method_wlr_pointer_motion_absolute_event_get(void *p, Janet key, Janet *out)
{
    struct wlr_pointer_motion_absolute_event **event_p = (struct wlr_pointer_motion_absolute_event **)p;
    struct wlr_pointer_motion_absolute_event *event = *event_p;
    
    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "pointer")) {
        if (!(event->pointer)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(event->pointer, &jwlr_at_wlr_pointer));
        return 1;
    }
    if (!janet_cstrcmp(kw, "time-msec")) {
        /* uint32_t -> uint64_t */
        *out = janet_wrap_u64(event->time_msec);
        return 1;
    }
    if (!janet_cstrcmp(kw, "x")) {
        *out = janet_wrap_number(event->x);
        return 1;
    }
    if (!janet_cstrcmp(kw, "y")) {
        *out = janet_wrap_number(event->y);
        return 1;
    }

    return 0;
}


static int method_wlr_pointer_button_event_get(void *p, Janet key, Janet *out)
{
    struct wlr_pointer_button_event **event_p = (struct wlr_pointer_button_event **)p;
    struct wlr_pointer_button_event *event = *event_p;
    
    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "pointer")) {
        if (!(event->pointer)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(event->pointer, &jwlr_at_wlr_pointer));
        return 1;
    }
    if (!janet_cstrcmp(kw, "time-msec")) {
        /* uint32_t -> uint64_t */
        *out = janet_wrap_u64(event->time_msec);
        return 1;
    }
    if (!janet_cstrcmp(kw, "button")) {
        /* uint32_t -> uint64_t */
        *out = janet_wrap_u64(event->button);
        return 1;
    }
    if (!janet_cstrcmp(kw, "state")) {
        *out = janet_ckeywordv(wlr_button_state_defs[event->state].name);
        return 1;
    }

    return 0;
}


static int method_wlr_pointer_axis_event_get(void *p, Janet key, Janet *out)
{
    struct wlr_pointer_axis_event **event_p = (struct wlr_pointer_axis_event **)p;
    struct wlr_pointer_axis_event *event = *event_p;
    
    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "pointer")) {
        if (!(event->pointer)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(event->pointer, &jwlr_at_wlr_pointer));
        return 1;
    }
    if (!janet_cstrcmp(kw, "time-msec")) {
        /* uint32_t -> uint64_t */
        *out = janet_wrap_u64(event->time_msec);
        return 1;
    }
    if (!janet_cstrcmp(kw, "source")) {
        /* uint32_t -> uint64_t */
        *out = janet_ckeywordv(wlr_axis_source_defs[event->source].name);
        return 1;
    }
    if (!janet_cstrcmp(kw, "orientation")) {
        *out = janet_ckeywordv(wlr_axis_orientation_defs[event->orientation].name);
        return 1;
    }
    if (!janet_cstrcmp(kw, "delta")) {
        *out = janet_wrap_number(event->delta);
        return 1;
    }
    if (!janet_cstrcmp(kw, "delta-discrete")) {
        *out = janet_wrap_integer(event->delta_discrete);
        return 1;
    }

    return 0;
}


static int method_wlr_seat_pointer_state_get(void *p, Janet key, Janet *out)
{
    struct wlr_seat_pointer_state **state_p = (struct wlr_seat_pointer_state **)p;
    struct wlr_seat_pointer_state *state = *state_p;
    
    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(state, kw, wlr_seat_pointer_state_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

    if (!janet_cstrcmp(kw, "seat")) {
        if (!(state->seat)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(state->seat, &jwlr_at_wlr_seat));
        return 1;
    }
    if (!janet_cstrcmp(kw, "focused-client")) {
        if (!(state->focused_client)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(state->focused_client, &jwlr_at_wlr_seat_client));
        return 1;
    }
    if (!janet_cstrcmp(kw, "focused-surface")) {
        if (!(state->focused_surface)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(state->focused_surface, &jwlr_at_wlr_surface));
        return 1;
    }
    if (!janet_cstrcmp(kw, "sx")) {
        *out = janet_wrap_number(state->sx);
        return 1;
    }
    if (!janet_cstrcmp(kw, "sy")) {
        *out = janet_wrap_number(state->sy);
        return 1;
    }

    return 0;
}


static int method_wlr_seat_keyboard_state_get(void *p, Janet key, Janet *out)
{
    struct wlr_seat_keyboard_state **state_p = (struct wlr_seat_keyboard_state **)p;
    struct wlr_seat_keyboard_state *state = *state_p;
    
    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(state, kw,
                                                                    wlr_seat_keyboard_state_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

    if (!janet_cstrcmp(kw, "seat")) {
        if (!(state->seat)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(state->seat, &jwlr_at_wlr_seat));
        return 1;
    }
    if (!janet_cstrcmp(kw, "keyboard")) {
        if (!(state->keyboard)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(state->keyboard, &jwlr_at_wlr_keyboard));
        return 1;
    }
    if (!janet_cstrcmp(kw, "focused-client")) {
        if (!(state->focused_client)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(state->focused_client, &jwlr_at_wlr_seat_client));
        return 1;
    }
    if (!janet_cstrcmp(kw, "focused-surface")) {
        if (!(state->focused_surface)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(state->focused_surface, &jwlr_at_wlr_surface));
        return 1;
    }

    return 0;
}


static int method_wlr_seat_pointer_request_set_cursor_event_get(void *p, Janet key, Janet *out)
{
    struct wlr_seat_pointer_request_set_cursor_event **event_p = (struct wlr_seat_pointer_request_set_cursor_event **)p;
    struct wlr_seat_pointer_request_set_cursor_event *event = *event_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "seat-client")) {
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(event->seat_client, &jwlr_at_wlr_seat_client));
        return 1;
    }
    if (!janet_cstrcmp(kw, "surface")) {
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(event->surface, &jwlr_at_wlr_surface));
        return 1;
    }
    if (!janet_cstrcmp(kw, "serial")) {
        /* XXX: uint32_t -> int32_t conversion */
        *out = janet_wrap_integer(event->serial);
        return 1;
    }
    if (!janet_cstrcmp(kw, "hotspot-x")) {
        *out = janet_wrap_integer(event->hotspot_x);
        return 1;
    }
    if (!janet_cstrcmp(kw, "hotspot-y")) {
        *out = janet_wrap_integer(event->hotspot_y);
        return 1;
    }

    return 0;
}


static int method_wlr_seat_request_set_selection_event_get(void *p, Janet key, Janet *out)
{
    struct wlr_seat_request_set_selection_event **event_p = (struct wlr_seat_request_set_selection_event **)p;
    struct wlr_seat_request_set_selection_event *event = *event_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "source")) {
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(event->source, &jwlr_at_wlr_data_source));
        return 1;
    }
    if (!janet_cstrcmp(kw, "serial")) {
        /* XXX: uint32_t -> int32_t conversion */
        *out = janet_wrap_integer(event->serial);
        return 1;
    }

    return 0;
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


static Janet cfun_wlr_scene_buffer_from_node(int32_t argc, Janet *argv)
{
    struct wlr_scene_node *node;

    struct wlr_scene_buffer *buffer;

    janet_fixarity(argc, 1);

    node = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_scene_node);
    buffer = wlr_scene_buffer_from_node(node);
    if (!buffer) {
        janet_panic("failed to get wlroots buffer object");
    }

    return janet_wrap_abstract(jl_pointer_to_abs_obj(buffer, &jwlr_at_wlr_scene_buffer));
}


static int method_wlr_scene_surface_get(void *p, Janet key, Janet *out)
{
    struct wlr_scene_surface **surface_p = (struct wlr_scene_surface **)p;
    struct wlr_scene_surface *surface = *surface_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "buffer")) {
        if (!(surface->buffer)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(surface->buffer, &jwlr_at_wlr_scene_buffer));
        return 1;
    }
    if (!janet_cstrcmp(kw, "surface")) {
        if (!(surface->surface)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_abstract(jl_pointer_to_abs_obj(surface->surface, &jwlr_at_wlr_surface));
        return 1;
    }

    return 0;
}


static Janet cfun_wlr_scene_surface_from_buffer(int32_t argc, Janet *argv)
{
    struct wlr_scene_buffer *buffer;

    struct wlr_scene_surface *surface;

    janet_fixarity(argc, 1);

    buffer = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_scene_buffer);
    surface = wlr_scene_surface_from_buffer(buffer);
    if (!surface) {
        return janet_wrap_nil();
    } else {
        return janet_wrap_abstract(jl_pointer_to_abs_obj(surface, &jwlr_at_wlr_scene_surface));
    }
}


static int method_wlr_xwayland_get(void *p, Janet key, Janet *out)
{
    struct wlr_xwayland **xwayland_p = (struct wlr_xwayland **)p;
    struct wlr_xwayland *xwayland = *xwayland_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(xwayland, kw, wlr_xwayland_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

   if (!janet_cstrcmp(kw, "display-name")) {
       if (!(xwayland->display_name)) {
           *out = janet_wrap_nil();
           return 1;
       }
       *out = janet_cstringv(xwayland->display_name);
       return 1;
   }
   if (!janet_cstrcmp(kw, "wl-display")) {
       if (!(xwayland->wl_display)) {
           *out = janet_wrap_nil();
           return 1;
       }
       *out = janet_wrap_abstract(jl_pointer_to_abs_obj_by_name(xwayland->wl_display, WL_MOD_NAME "/wl-display"));
       return 1;
   }
   if (!janet_cstrcmp(kw, "compositor")) {
       if (!(xwayland->compositor)) {
           *out = janet_wrap_nil();
           return 1;
       }
       *out = janet_wrap_abstract(jl_pointer_to_abs_obj(xwayland->compositor, &jwlr_at_wlr_compositor));
       return 1;
   }
   if (!janet_cstrcmp(kw, "seat")) {
       if (!(xwayland->seat)) {
           *out = janet_wrap_nil();
           return 1;
       }
       *out = janet_wrap_abstract(jl_pointer_to_abs_obj(xwayland->seat, &jwlr_at_wlr_seat));
       return 1;
   }
   if (!janet_cstrcmp(kw, "data")) {
        if (!(xwayland->data)) {
            *out = janet_wrap_nil();
            return 1;
        }
        *out = janet_wrap_pointer(xwayland->data);
        return 1;
    }

    return 0;
}


static Janet cfun_wlr_xwayland_create(int32_t argc, Janet *argv)
{
    struct wl_display *display;
    struct wlr_compositor *compositor;
    bool lazy;

    struct wlr_xwayland *xwayland;

    janet_fixarity(argc, 3);

    display = jl_get_abs_obj_pointer_by_name(argv, 0, WL_MOD_NAME "/wl-display");
    compositor = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_compositor);
    lazy = janet_getboolean(argv, 2);

    xwayland = wlr_xwayland_create(display, compositor, lazy);
    if (!xwayland) {
        janet_panic("failed to create wlroots XWayland object");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(xwayland, &jwlr_at_wlr_xwayland));
}


static Janet cfun_wlr_xwayland_set_seat(int32_t argc, Janet *argv)
{
    struct wlr_xwayland *xwayland;
    struct wlr_seat *seat;

    janet_fixarity(argc, 2);

    xwayland = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xwayland);
    seat = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_seat);

    wlr_xwayland_set_seat(xwayland, seat);
    return janet_wrap_nil();
}


static Janet cfun_wlr_xwayland_set_cursor(int32_t argc, Janet *argv)
{
    struct wlr_xwayland *xwayland;
    JanetBuffer *pixels;
    uint32_t stride;
    uint32_t width, height;
    int32_t hotspot_x, hotspot_y;

    janet_fixarity(argc, 7);

    xwayland = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xwayland);
    pixels = janet_getbuffer(argv, 1);
    /* uint64_t -> uint32_t conversion */
    stride = (uint32_t)janet_getuinteger64(argv, 2);
    width = (uint32_t)janet_getuinteger64(argv, 3);
    height = (uint32_t)janet_getuinteger64(argv, 4);
    hotspot_x = janet_getinteger(argv, 5);
    hotspot_y = janet_getinteger(argv, 6);

    wlr_xwayland_set_cursor(xwayland, pixels->data, stride, width, height, hotspot_x, hotspot_y);
    return janet_wrap_nil();
}


static int method_wlr_xwayland_surface_get(void *p, Janet key, Janet *out)
{
    struct wlr_xwayland_surface **surface_p = (struct wlr_xwayland_surface **)p;
    struct wlr_xwayland_surface *surface = *surface_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    struct wl_signal **signal_p = get_abstract_struct_signal_member(surface, kw,
                                                                    wlr_xwayland_surface_signal_offsets);
    if (signal_p) {
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

   if (!janet_cstrcmp(kw, "surface")) {
       if (!(surface->surface)) {
           *out = janet_wrap_nil();
           return 1;
       }
       *out = janet_wrap_abstract(jl_pointer_to_abs_obj(surface->surface, &jwlr_at_wlr_surface));
       return 1;
   }
   if (!janet_cstrcmp(kw, "x")) {
       /* int16_t -> int32_t conversion */
       *out = janet_wrap_integer(surface->x);
       return 1;
   }
   if (!janet_cstrcmp(kw, "y")) {
       /* int16_t -> int32_t conversion */
       *out = janet_wrap_integer(surface->y);
       return 1;
   }
   if (!janet_cstrcmp(kw, "width")) {
       /* uint16_t -> int32_t conversion */
       *out = janet_wrap_integer(surface->width);
       return 1;
   }
   if (!janet_cstrcmp(kw, "height")) {
       /* uint16_t -> int32_t conversion */
       *out = janet_wrap_integer(surface->height);
       return 1;
   }
   if (!janet_cstrcmp(kw, "override-redirect")) {
       *out = janet_wrap_boolean(surface->override_redirect);
       return 1;
   }
   if (!janet_cstrcmp(kw, "mapped")) {
       *out = janet_wrap_boolean(surface->mapped);
       return 1;
   }
   if (!janet_cstrcmp(kw, "title")) {
       if (!(surface->title)) {
           *out = janet_wrap_nil();
           return 1;
       }
       *out = janet_cstringv(surface->title);
       return 1;
   }
   if (!janet_cstrcmp(kw, "class")) {
       if (!(surface->class)) {
           *out = janet_wrap_nil();
           return 1;
       }
       *out = janet_cstringv(surface->class);
       return 1;
   }
   if (!janet_cstrcmp(kw, "instance")) {
       if (!(surface->instance)) {
           *out = janet_wrap_nil();
           return 1;
       }
       *out = janet_cstringv(surface->instance);
       return 1;
   }
   if (!janet_cstrcmp(kw, "role")) {
       if (!(surface->role)) {
           *out = janet_wrap_nil();
           return 1;
       }
       *out = janet_cstringv(surface->role);
       return 1;
   }
   if (!janet_cstrcmp(kw, "startup-id")) {
       if (!(surface->startup_id)) {
           *out = janet_wrap_nil();
           return 1;
       }
       *out = janet_cstringv(surface->startup_id);
       return 1;
   }
   if (!janet_cstrcmp(kw, "parent")) {
       if (!(surface->parent)) {
           *out = janet_wrap_nil();
           return 1;
       }
       *out = janet_wrap_abstract(jl_pointer_to_abs_obj(surface->parent, &jwlr_at_wlr_xwayland_surface));
       return 1;
   }

    return 0;
}

static void method_wlr_xwayland_surface_put(void *p, Janet key, Janet value) {
    struct wlr_xwayland_surface **surface_p = (struct wlr_xwayland_surface **)p;
    struct wlr_xwayland_surface *surface = *surface_p;


    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "data")) {
        surface->data = jl_value_to_data_pointer(value);
        return;
    }

    janet_panicf("unknown key: %v", key);
}


static Janet cfun_wlr_xwayland_or_surface_wants_focus(int32_t argc, Janet *argv)
{
    struct wlr_xwayland_surface *surface;

    janet_fixarity(argc, 1);

    surface = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xwayland_surface);
    return janet_wrap_boolean(wlr_xwayland_or_surface_wants_focus(surface));
}


static Janet cfun_wlr_xwayland_surface_configure(int32_t argc, Janet *argv)
{
    struct wlr_xwayland_surface *surface;
    int16_t x, y;
    uint16_t width, height;

    janet_fixarity(argc, 5);

    surface = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xwayland_surface);
    /* int32_t -> int16_t conversion */
    /* XXX: Should check the range before conversion */
    x = (int16_t)janet_getinteger(argv, 1);
    y = (int16_t)janet_getinteger(argv, 2);
    /* int32_t -> uint16_t conversion */
    /* XXX: Should check the range before conversion */
    width = (uint16_t)janet_getinteger(argv, 3);
    height = (uint16_t)janet_getinteger(argv, 4);

    wlr_xwayland_surface_configure(surface, x, y, width, height);
    return janet_wrap_nil();
}


static Janet cfun_wlr_xwayland_surface_activate(int32_t argc, Janet *argv)
{
    struct wlr_xwayland_surface *surface;
    bool activated;

    janet_fixarity(argc, 2);

    surface = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xwayland_surface);
    activated = janet_getboolean(argv, 1);

    wlr_xwayland_surface_activate(surface, activated);
    return janet_wrap_nil();
}


static Janet cfun_wlr_xwayland_surface_set_maximized(int32_t argc, Janet *argv)
{
    struct wlr_xwayland_surface *surface;
    bool maximized;

    janet_fixarity(argc, 2);

    surface = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xwayland_surface);
    maximized = janet_getboolean(argv, 1);

    wlr_xwayland_surface_set_maximized(surface, maximized);
    return janet_wrap_nil();
}


static Janet cfun_wlr_xwayland_surface_set_minimized(int32_t argc, Janet *argv)
{
    struct wlr_xwayland_surface *surface;
    bool minimized;

    janet_fixarity(argc, 2);

    surface = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xwayland_surface);
    minimized = janet_getboolean(argv, 1);

    wlr_xwayland_surface_set_minimized(surface, minimized);
    return janet_wrap_nil();
}


static Janet cfun_wlr_xwayland_surface_set_fullscreen(int32_t argc, Janet *argv)
{
    struct wlr_xwayland_surface *surface;
    bool fullscreen;

    janet_fixarity(argc, 2);

    surface = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xwayland_surface);
    fullscreen = janet_getboolean(argv, 1);

    wlr_xwayland_surface_set_fullscreen(surface, fullscreen);
    return janet_wrap_nil();
}


static Janet cfun_wlr_xwayland_surface_close(int32_t argc, Janet *argv)
{
    struct wlr_xwayland_surface *surface;

    janet_fixarity(argc, 1);

    surface = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xwayland_surface);
    wlr_xwayland_surface_close(surface);
    return janet_wrap_nil();
}


#define __XCB_STACK_MODE_MAX 4
static const jl_key_def_t xcb_stack_mode_defs[] = {
    {"above", XCB_STACK_MODE_ABOVE},
    {"below", XCB_STACK_MODE_BELOW},
    {"top-if", XCB_STACK_MODE_TOP_IF},
    {"bottom-if", XCB_STACK_MODE_BOTTOM_IF},
    {"opposite", XCB_STACK_MODE_OPPOSITE},
    {NULL, 0},
};

static Janet cfun_wlr_xwayland_surface_restack(int32_t argc, Janet *argv)
{
    struct wlr_xwayland_surface *surface, *sibling;
    enum xcb_stack_mode_t mode;

    janet_fixarity(argc, 3);

    surface = jl_get_abs_obj_pointer(argv, 0, &jwlr_at_wlr_xwayland_surface);
    if (janet_checktype(argv[1], JANET_NIL)) {
        sibling = NULL;
    } else {
        sibling = jl_get_abs_obj_pointer(argv, 1, &jwlr_at_wlr_xwayland_surface);
    }
    mode = jl_get_key_def(argv, 2, xcb_stack_mode_defs);

    wlr_xwayland_surface_restack(surface, sibling, mode);
    return janet_wrap_nil();
}


static int method_wlr_xwayland_surface_configure_event_get(void *p, Janet key, Janet *out)
{
    struct wlr_xwayland_surface_configure_event **event_p = (struct wlr_xwayland_surface_configure_event **)p;
    struct wlr_xwayland_surface_configure_event *event = *event_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

   if (!janet_cstrcmp(kw, "surface")) {
       if (!(event->surface)) {
           *out = janet_wrap_nil();
           return 1;
       }
       *out = janet_wrap_abstract(jl_pointer_to_abs_obj(event->surface, &jwlr_at_wlr_xwayland_surface));
       return 1;
   }
   if (!janet_cstrcmp(kw, "x")) {
       /* int16_t -> int32_t conversion */
       *out = janet_wrap_integer(event->x);
       return 1;
   }
   if (!janet_cstrcmp(kw, "y")) {
       /* int16_t -> int32_t conversion */
       *out = janet_wrap_integer(event->y);
       return 1;
   }
   if (!janet_cstrcmp(kw, "width")) {
       /* uint16_t -> int32_t conversion */
       *out = janet_wrap_integer(event->width);
       return 1;
   }
   if (!janet_cstrcmp(kw, "height")) {
       /* uint16_t -> int32_t conversion */
       *out = janet_wrap_integer(event->height);
       return 1;
   }
   if (!janet_cstrcmp(kw, "mask")) {
       /* uint16_t -> int32_t conversion */
       *out = janet_wrap_integer(event->mask);
       return 1;
   }

    return 0;
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
        "box", cfun_box,
        "(" MOD_NAME "/box ...)\n\n"
        "Creates a box object."
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
        "wlr-scene-node-at", cfun_wlr_scene_node_at,
        "(" MOD_NAME "/wlr-scene-node-at wlr-scene-node x y)\n\n"
        "Finds the topmost node that contains the specified point."
    },
    {
        "wlr-scene-node-raise-to-top", cfun_wlr_scene_node_raise_to_top,
        "(" MOD_NAME "/wlr-scene-node-raise-to-top wlr-scene-node)\n\n"
        "Move the node above all of its sibling nodes."
    },
    {
        "wlr-scene-node-set-position", cfun_wlr_scene_node_set_position,
        "(" MOD_NAME "/wlr-scene-node-set-position wlr-scene-node x y)\n\n"
        "Sets the position of a scene node."
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
        "wlr-cursor-move", cfun_wlr_cursor_move,
        "(" MOD_NAME "/wlr-cursor-move wlr-cursor wlr-input-device delta-x delta-y)\n\n"
        "Moves the cursor."
    },
    {
        "wlr-cursor-warp-absolute", cfun_wlr_cursor_warp_absolute,
        "(" MOD_NAME "/wlr-cursor-warp-absolute wlr-cursor wlr-input-device x y)\n\n"
        "Moves the cursor using absolute coordinates."
    },
    {
        "wlr-cursor-attach-output-layout", cfun_wlr_cursor_attach_output_layout,
        "(" MOD_NAME "/wlr-cursor-attach-output-layout wlr-cursor wlr-output-layout)\n\n"
        "Attaches a wlroots output layout object to a cursor object."
    },
    {
        "wlr-cursor-attach-input-device", cfun_wlr_cursor_attach_input_device,
        "(" MOD_NAME "/wlr-cursor-attach-input-device wlr-cursor wlr-input-device)\n\n"
        "Attaches a wlroots input device object to a cursor object."
    },
    {
        "wlr-cursor-set-surface", cfun_wlr_cursor_set_surface,
        "(" MOD_NAME "/wlr-cursor-set-surface)\n\n"
        "Sets a surface as the cursor image."
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
        "wlr-xcursor-manager-get-xcursor", cfun_wlr_xcursor_manager_get_xcursor,
        "(" MOD_NAME "/wlr-xcursor-manager-get-xcursor wlr-xcursor-manager name scale)\n\n"
        "Retrieves xcursor image data."
    },
    {
        "wlr-xcursor-manager-set-cursor-image", cfun_wlr_xcursor_manager_set_cursor_image,
        "(" MOD_NAME "/wlr-xcursor-manager-set-cursor-image wlr-xcursor-manager name wlr-cursor)\n\n"
        "Sets the cursor image from the loaded xcursor theme."
    },
    {
        "wlr-keyboard-from-input-device", cfun_wlr_keyboard_from_input_device,
        "(" MOD_NAME "/wlr-keyboard-from-input-device wlr-input-device)\n\n"
        "Converts a wlroots input device to a keyboard object."
    },
    {
        "wlr-keyboard-set-keymap", cfun_wlr_keyboard_set_keymap,
        "(" MOD_NAME "/wlr-keyboard-set-keymap wlr-keyboard xkb-keymap)\n\n"
        "Sets an xkb keymap to a keyboard object."
    },
    {
        "wlr-keyboard-set-repeat-info", cfun_wlr_keyboard_set_repeat_info,
        "(" MOD_NAME "/wlr-keyboard-set-repeat-info wlr-keyboard rate delay)\n\n"
        "Sets key repeat parameters for a keyboard object."
    },
    {
        "wlr-keyboard-get-modifiers", cfun_wlr_keyboard_get_modifiers,
        "(" MOD_NAME "/wlr-keyboard-get-modifiers wlr-keyboard)\n\n"
        "Gets the keyboard modifiers that's currently in effect."
    },
    {
        "wlr-seat-create", cfun_wlr_seat_create,
        "(" MOD_NAME "/wlr-seat-create wl-display name)\n\n"
        "Creates a wlroots seat object."
    },
    {
        "wlr-seat-set-keyboard", cfun_wlr_seat_set_keyboard,
        "(" MOD_NAME "/wlr-seat-set-keyboard wlr-seat wlr-keyboard)\n\n"
        "Connects a keyboard to a seat object."
    },
    {
        "wlr-seat-keyboard-notify-modifiers", cfun_wlr_seat_keyboard_notify_modifiers,
        "(" MOD_NAME "/wlr-seat-keyboard-notify-modifiers wlr-seat wlr-keyboard-modifiers)\n\n"
        "Notifies the seat object that the states of keyboard modifiers changed."
    },
    {
        "wlr-seat-keyboard-notify-key", cfun_wlr_seat_keyboard_notify_key,
        "(" MOD_NAME "/wlr-seat-keyboard-notify-key wlr-seat time key state)\n\n"
        "Notifies the seat object that there's a key event."
    },
    {
        "wlr-seat-keyboard-notify-enter", cfun_wlr_seat_keyboard_notify_enter,
        "(" MOD_NAME "/wlr-seat-keyboard-notify-enter wlr-seat wlr-surface keycodes wlr-keyboard-modifiers)\n\n"
        "Notifies the seat object that the keyboard focus has changed."
    },
    {
        "wlr-seat-keyboard-notify-clear-focus", cfun_wlr_seat_keyboard_notify_clear_focus,
        "(" MOD_NAME "/wlr-seat-keyboard-notify-clear-focus wlr-seat)\n\n"
        "Notifies the seat object that the keyboard focus has left the current surface."
    },
    {
        "wlr-seat-pointer-notify-button", cfun_wlr_seat_pointer_notify_button,
        "(" MOD_NAME "/wlr-seat-pointer-notify-button wlr-seat time button state)\n\n"
        "Notifies the seat object that there's a pointer button event."
    },
    {
        "wlr-seat-pointer-notify-axis", cfun_wlr_seat_pointer_notify_axis,
        "(" MOD_NAME "/wlr-seat-pointer-notify-axis wlr-seat time orientation value value-discrete source)\n\n"
        "Notifies the seat object that there's an pointer axis event."
    },
    {
        "wlr-seat-pointer-notify-frame", cfun_wlr_seat_pointer_notify_frame,
        "(" MOD_NAME "/wlr-seat-pointer-notify-frame wlr-seat)\n\n"
        "Notifies the seat object that there's an pointer frame event."
    },
    {
        "wlr-seat-pointer-notify-enter", cfun_wlr_seat_pointer_notify_enter,
        "(" MOD_NAME "/wlr-seat-pointer-notify-enter wlr-seat wlr-surface sx sy)\n\n"
        "Notifies the seat object that there's an pointer enter event."
    },
    {
        "wlr-seat-pointer-notify-motion", cfun_wlr_seat_pointer_notify_motion,
        "(" MOD_NAME "/wlr-seat-pointer-notify-motion wlr-seat time sx sy)\n\n"
        "Notifies the seat object that there's an pointer motion event."
    },
    {
        "wlr-seat-pointer-notify-clear-focus", cfun_wlr_seat_pointer_notify_clear_focus,
        "(" MOD_NAME "/wlr-seat-pointer-notify-clear-focus wlr-seat)\n\n"
        "Notifies the seat object that the pointer focus has left."
    },
    {
        "wlr-seat-pointer-clear-focus", cfun_wlr_seat_pointer_clear_focus,
        "(" MOD_NAME "/wlr-seat-pointer-clear-focus wlr-seat)\n\n"
        "Clears the focused surface for the pointer."
    },
    {
        "wlr-seat-set-capabilities", cfun_wlr_seat_set_capabilities,
        "(" MOD_NAME "/wlr-seat-set-capabilities wlr-seat capabilities)\n\n"
        "Sets the capabilities of a seat."
    },
    {
        "wlr-seat-set-selection", cfun_wlr_seat_set_selection,
        "(" MOD_NAME "/wlr-seat-set-selection wlr-seat wlr-data-source serial)\n\n"
        "Sets the current selection for the seat."
    },
    {
        "wlr-seat-get-keyboard", cfun_wlr_seat_get_keyboard,
        "(" MOD_NAME "/wlr-seat-get-keyboard wlr-seat)\n\n"
        "Gets the current active keyboard for the seat."
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
        "wlr-output-layout-get-box", cfun_wlr_output_layout_get_box,
        "(" MOD_NAME "/wlr-output-layout-get-box wlr-output-layout wlr-output)\n\n"
        "Get the box of the layout for the given reference output."
    },
    {
        "wlr-surface-get-root-surface", cfun_wlr_surface_get_root_surface,
        "(" MOD_NAME "/wlr-surface-get-root-surface wlr-surface)\n\n"
        "Gets the root of the subsurface tree for the specified surface."
    },
    {
        "wlr-xdg-surface-from-wlr-surface", cfun_wlr_xdg_surface_from_wlr_surface,
        "(" MOD_NAME "/wlr-xdg-surface-from-wlr-surface wlr-surface)\n\n"
        "Wraps a surface object with an xdg surface object."
    },
    {
        "wlr-xdg-surface-schedule-configure", cfun_wlr_xdg_surface_schedule_configure,
        "(" MOD_NAME "/wlr-xdg-surface-schedule-configure wlr-xdg-surface)\n\n"
        "Sends a configure for the xdg surface."
    },
    {
        "wlr-xdg-surface-get-geometry", cfun_wlr_xdg_surface_get_geometry,
        "(" MOD_NAME "/wlr-xdg-surface-get-geometry wlr-xdg-surface)\n\n"
        "Gets the geometry of a wlroots xdg surface object."
    },
    {
        "wlr-xdg-toplevel-set-activated", cfun_wlr_xdg_toplevel_set_activated,
        "(" MOD_NAME "/wlr-xdg-toplevel-set-activated wlr-xdg-toplevel activated)\n\n"
        "Sets the toplevel in an activated or deactivated state."
    },
    {
        "wlr-xdg-toplevel-set-maximized", cfun_wlr_xdg_toplevel_set_maximized,
        "(" MOD_NAME "/wlr-xdg-toplevel-set-maximized wlr-xdg-toplevel maximized)\n\n"
        "Sets the toplevel in a maximized state."
    },
    {
        "wlr-xdg-toplevel-set-fullscreen", cfun_wlr_xdg_toplevel_set_fullscreen,
        "(" MOD_NAME "/wlr-xdg-toplevel-set-fullscreen wlr-xdg-toplevel fullscreen)\n\n"
        "Sets the toplevel in a fullscreen state."
    },
    {
        "wlr-xdg-toplevel-set-resizing", cfun_wlr_xdg_toplevel_set_resizing,
        "(" MOD_NAME "/wlr-xdg-toplevel-set-resizing wlr-xdg-toplevel resizing)\n\n"
        "Sets the toplevel in a resizing state."
    },
    {
        "wlr-xdg-toplevel-set-tiled", cfun_wlr_xdg_toplevel_set_tiled,
        "(" MOD_NAME "/wlr-xdg-toplevel-set-tiled wlr-xdg-toplevel tiled-edges)\n\n"
        "Sets the toplevel in a tiled state. Edges can be :top, :bottom, :left or :right."
    },
    {
        "wlr-xdg-toplevel-set-size", cfun_wlr_xdg_toplevel_set_size,
        "(" MOD_NAME "/wlr-xdg-toplevel-set-size wlr-xdg-toplevel width height)\n\n"
        "Requests that this toplevel surface be the given size."
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
    {
        "wlr-scene-buffer-from-node", cfun_wlr_scene_buffer_from_node,
        "(" MOD_NAME "/wlr-scene-buffer-from-node wlr-scene-node)\n\n"
        "Gets the associated wlr-scene-buffer object from a scene node."
    },
    {
        "wlr-scene-surface-from-buffer", cfun_wlr_scene_surface_from_buffer,
        "(" MOD_NAME "/wlr-scene-surface-from-buffer wlr-scene-buffer)\n\n"
        "Gets the wlr-scene-surface backing the specified buffer."
    },
    {
        "wlr-xwayland-create", cfun_wlr_xwayland_create,
        "(" MOD_NAME "/wlr-xwayland-create wl-display wlr-compositor lazy)\n\n"
        "Creates an XWayland server and XWM."
    },
    {
        "wlr-xwayland-set-seat", cfun_wlr_xwayland_set_seat,
        "(" MOD_NAME "/wlr-xwayland-set-seat wlr-xwayland wlr-seat)\n\n"
        "Lets the XWayland object know about the seat."
    },
    {
        "wlr-xwayland-set-cursor", cfun_wlr_xwayland_set_cursor,
        "(" MOD_NAME "/wlr-xwayland-set-cursor wlr-xwayland pixels stride width height hotspot-x hotspot-y)\n\n"
        "Sets the cursor image for XWayland."
    },
    {
        "wlr-xwayland-or-surface-wants-focus", cfun_wlr_xwayland_or_surface_wants_focus,
        "(" MOD_NAME "/wlr-xwayland-or-surface-wants-focus wlr-xwayland-surface)\n\n"
        "Sets the cursor image for XWayland."
    },
    {
        "wlr-xwayland-surface-configure", cfun_wlr_xwayland_surface_configure,
        "(" MOD_NAME "/wlr-xwayland-surface-configure wlr-xwayland-surface x y width height)\n\n"
        "Configures an XWayland surface with the given geometry."
    },
    {
        "wlr-xwayland-surface-activate", cfun_wlr_xwayland_surface_activate,
        "(" MOD_NAME "/wlr-xwayland-surface-activate wlr-xwayland-surface activated)\n\n"
        "Activates an XWayland surface."
    },
    {
        "wlr-xwayland-surface-set-maximized", cfun_wlr_xwayland_surface_set_maximized,
        "(" MOD_NAME "/wlr-xwayland-surface-set-maximized wlr-xwayland-surface maximized)\n\n"
        "Sets the XWayland surface in a maximized state."
    },
    {
        "wlr-xwayland-surface-set-minimized", cfun_wlr_xwayland_surface_set_minimized,
        "(" MOD_NAME "/wlr-xwayland-surface-set-minimized wlr-xwayland-surface minimized)\n\n"
        "Sets the XWayland surface in a minimized state."
    },
    {
        "wlr-xwayland-surface-set-fullscreen", cfun_wlr_xwayland_surface_set_fullscreen,
        "(" MOD_NAME "/wlr-xwayland-surface-set-fullscreen wlr-xwayland-surface fullscreen)\n\n"
        "Sets the XWayland surface in a fullscreen state."
    },
    {
        "wlr-xwayland-surface-close", cfun_wlr_xwayland_surface_close,
        "(" MOD_NAME "/wlr-xwayland-surface-close wlr-xwayland-surface)\n\n"
        "Closes an XWayland surface."
    },
    {
        "wlr-xwayland-surface-restack", cfun_wlr_xwayland_surface_restack,
        "(" MOD_NAME "/wlr-xwayland-surface-restack wlr-xwayland-surface sibling mode)\n\n"
        "Restacks surface relative to sibling."
    },
    {NULL, NULL, NULL},
};


JANET_MODULE_ENTRY(JanetTable *env)
{
    /* Import wl module first, so that we can find the wl_* abstract types */
    /* XXX: this will pollute the environment with wl/ stuff, even :export is set to nil */
    jl_import(WL_MOD_FULL_NAME);

    janet_register_abstract_type(&jwlr_at_box);
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
    janet_register_abstract_type(&jwlr_at_wlr_scene_buffer);
    janet_register_abstract_type(&jwlr_at_wlr_scene_surface);
    janet_register_abstract_type(&jwlr_at_wlr_xdg_shell);
    janet_register_abstract_type(&jwlr_at_wlr_surface);
    janet_register_abstract_type(&jwlr_at_wlr_xdg_surface);
    janet_register_abstract_type(&jwlr_at_wlr_xdg_toplevel);
    janet_register_abstract_type(&jwlr_at_wlr_xdg_toplevel_resize_event);
    janet_register_abstract_type(&jwlr_at_wlr_xdg_popup);
    janet_register_abstract_type(&jwlr_at_wlr_cursor);
    janet_register_abstract_type(&jwlr_at_wlr_xcursor_manager);
    janet_register_abstract_type(&jwlr_at_wlr_output);
    janet_register_abstract_type(&jwlr_at_wlr_output_mode);
    janet_register_abstract_type(&jwlr_at_wlr_output_cursor);
    janet_register_abstract_type(&jwlr_at_wlr_input_device);
    janet_register_abstract_type(&jwlr_at_wlr_pointer);
    janet_register_abstract_type(&jwlr_at_wlr_pointer_motion_event);
    janet_register_abstract_type(&jwlr_at_wlr_pointer_motion_absolute_event);
    janet_register_abstract_type(&jwlr_at_wlr_pointer_button_event);
    janet_register_abstract_type(&jwlr_at_wlr_pointer_axis_event);
    janet_register_abstract_type(&jwlr_at_wlr_seat);
    janet_register_abstract_type(&jwlr_at_wlr_seat_pointer_state);
    janet_register_abstract_type(&jwlr_at_wlr_seat_pointer_request_set_cursor_event);
    janet_register_abstract_type(&jwlr_at_wlr_seat_keyboard_state);
    janet_register_abstract_type(&jwlr_at_wlr_seat_request_set_selection_event);
    janet_register_abstract_type(&jwlr_at_wlr_data_source);
    janet_register_abstract_type(&jwlr_at_wlr_keyboard);
    janet_register_abstract_type(&jwlr_at_wlr_keyboard_modifiers);
    janet_register_abstract_type(&jwlr_at_wlr_keyboard_key_event);
    janet_register_abstract_type(&jwlr_at_wlr_xwayland);
    janet_register_abstract_type(&jwlr_at_wlr_xwayland_surface);
    janet_register_abstract_type(&jwlr_at_wlr_xwayland_surface_configure_event);

    janet_cfuns(env, MOD_NAME, cfuns);
}
