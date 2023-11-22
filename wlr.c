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

#include "jl.h"
#include "types.h"

#define MOD_NAME "wlr"
#define WL_MOD_NAME "wl"
#define WL_MOD_FULL_NAME "janetland/wl"


JANET_THREAD_LOCAL JanetFunction *jwlr_log_callback_fn;


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


static int wlr_backend_get(void *p, Janet key, Janet *out) {
    struct wlr_backend **backend_p = (struct wlr_backend **)p;
    struct wlr_backend *backend = *backend_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

#define __EVENTS_PREFIX "events."

    int events_prefix_len = strlen(__EVENTS_PREFIX);
    if (!strncmp(__EVENTS_PREFIX, (const char *)kw, events_prefix_len)) {
        struct wl_signal *signal = NULL;
        const uint8_t *kw_suffix = kw + events_prefix_len;
        if (!janet_cstrcmp(kw_suffix, "destroy")) {
            signal = &backend->events.destroy;
        } else if (!janet_cstrcmp(kw_suffix, "new_input")) {
            signal = &backend->events.new_input;
        } else if (!janet_cstrcmp(kw_suffix, "new_output")) {
            signal = &backend->events.new_output;
        } else {
            return 0;
        }
        struct wl_signal **signal_p = janet_abstract(jl_get_abstract_type_by_name(WL_MOD_NAME "/wl-signal"),
                                                     sizeof(*signal_p));
        *signal_p = signal;
        *out = janet_wrap_abstract(signal_p);
        return 1;
    }

#undef __EVENTS_PREFIX

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
    struct wl_display **display;

    struct wlr_backend **backend;

    janet_fixarity(argc, 1);

    display = janet_getabstract(argv, 0, jl_get_abstract_type_by_name(WL_MOD_NAME "/wl-display"));
    backend = janet_abstract(&jwlr_at_wlr_backend, sizeof(*backend));
    *backend = wlr_backend_autocreate(*display);
    if (!(*backend)) {
        janet_panic("failed to create wlroots backend object");
    }

    return janet_wrap_abstract(backend);
}


static const JanetAbstractType jwlr_at_wlr_renderer = {
    .name = MOD_NAME "/wlr-renderer",
    .gc = NULL, /* TODO: close the renderer? */
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

static Janet cfun_wlr_renderer_autocreate(int32_t argc, Janet *argv)
{
    struct wlr_backend **backend;

    struct wlr_renderer **renderer;

    janet_fixarity(argc, 1);

    backend = janet_getabstract(argv, 0, &jwlr_at_wlr_backend);
    renderer = janet_abstract(&jwlr_at_wlr_renderer, sizeof(*renderer));
    *renderer = wlr_renderer_autocreate(*backend);
    if (!(*renderer)) {
        janet_panic("failed to create wlroots renderer object");
    }

    return janet_wrap_abstract(renderer);
}


static Janet cfun_wlr_renderer_init_wl_display(int32_t argc, Janet *argv)
{
    struct wlr_renderer **renderer;
    struct wl_display **display;

    bool ret;

    janet_fixarity(argc, 2);

    renderer = janet_getabstract(argv, 0, &jwlr_at_wlr_renderer);
    display = janet_getabstract(argv, 1, jl_get_abstract_type_by_name(WL_MOD_NAME "/wl-display"));
    ret = wlr_renderer_init_wl_display(*renderer, *display);

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
    struct wlr_backend **backend;
    struct wlr_renderer **renderer;

    struct wlr_allocator **allocator;

    janet_fixarity(argc, 2);

    backend = janet_getabstract(argv, 0, &jwlr_at_wlr_backend);
    renderer = janet_getabstract(argv, 1, &jwlr_at_wlr_renderer);
    allocator = janet_abstract(&jwlr_at_wlr_allocator, sizeof(*allocator));
    *allocator = wlr_allocator_autocreate(*backend, *renderer);
    if (!(*allocator)) {
        janet_panic("failed to create wlroots allocator object");
    }

    return janet_wrap_abstract(allocator);
}


static const JanetAbstractType jwlr_at_wlr_compositor = {
    .name = MOD_NAME "/wlr-compositor",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

static Janet cfun_wlr_compositor_create(int32_t argc, Janet *argv)
{
    struct wl_display **display;
    struct wlr_renderer **renderer;

    struct wlr_compositor **compositor;

    janet_fixarity(argc, 2);

    display = janet_getabstract(argv, 0, jl_get_abstract_type_by_name(WL_MOD_NAME "/wl-display"));
    renderer = janet_getabstract(argv, 1, &jwlr_at_wlr_renderer);
    compositor = janet_abstract(&jwlr_at_wlr_compositor, sizeof(*compositor));
    *compositor = wlr_compositor_create(*display, *renderer);
    if (!(*compositor)) {
        janet_panic("failed to create wlroots compositor object");
    }

    return janet_wrap_abstract(compositor);
}


static const JanetAbstractType jwlr_at_wlr_subcompositor = {
    .name = MOD_NAME "/wlr-subcompositor",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

static Janet cfun_wlr_subcompositor_create(int32_t argc, Janet *argv)
{
    struct wl_display **display;

    struct wlr_subcompositor **subcompositor;

    janet_fixarity(argc, 1);

    display = janet_getabstract(argv, 0, jl_get_abstract_type_by_name(WL_MOD_NAME "/wl-display"));
    subcompositor = janet_abstract(&jwlr_at_wlr_subcompositor, sizeof(*subcompositor));
    *subcompositor = wlr_subcompositor_create(*display);
    if (!(*subcompositor)) {
        janet_panic("failed to create wlroots subcompositor object");
    }

    return janet_wrap_abstract(subcompositor);
}


static const JanetAbstractType jwlr_at_wlr_data_device_manager = {
    .name = MOD_NAME "/wlr-data-device-manager",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

static Janet cfun_wlr_data_device_manager_create(int32_t argc, Janet *argv)
{
    struct wl_display **display;

    struct wlr_data_device_manager **manager;

    janet_fixarity(argc, 1);

    display = janet_getabstract(argv, 0, jl_get_abstract_type_by_name(WL_MOD_NAME "/wl-display"));
    manager = janet_abstract(&jwlr_at_wlr_data_device_manager, sizeof(*manager));
    *manager = wlr_data_device_manager_create(*display);
    if (!(*manager)) {
        janet_panic("failed to create wlroots data device manager object");
    }

    return janet_wrap_abstract(manager);
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

    struct wlr_output_layout **layout;

    janet_fixarity(argc, 0);

    layout = janet_abstract(&jwlr_at_wlr_output_layout, sizeof(*layout));
    *layout = wlr_output_layout_create();
    if (!(*layout)) {
        janet_panic("failed to create wlroots output layout object");
    }

    return janet_wrap_abstract(layout);
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

    struct wlr_scene **scene;

    janet_fixarity(argc, 0);

    scene = janet_abstract(&jwlr_at_wlr_scene, sizeof(*scene));
    *scene = wlr_scene_create();
    if (!(*scene)) {
        janet_panic("failed to create wlroots scene object");
    }

    return janet_wrap_abstract(scene);
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
        "wlr-scene-atttach-output-layout", cfun_wlr_scene_attach_output_layout,
        "(" MOD_NAME "/wlr-scene-attach-output-layout wlr-scene wlr-output-layout)\n\n"
        "Attaches a wlroots output layout object to a scene object."
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

    janet_cfuns(env, MOD_NAME, cfuns);
}
