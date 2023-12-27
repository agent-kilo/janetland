#include <signal.h>

#include <janet.h>

#include <wayland-server-core.h>

#include "jl.h"
#include "types.h"
#include "wl_abs_types.h"


#ifndef MOD_NAME
#define MOD_NAME WL_MOD_NAME
#endif


typedef struct {
    struct wl_event_source *event_source;
    JanetStream *stream;
    JanetFunction *cb_fn;
} jwl_event_source_t;


int jwl_event_loop_fd_callback(int fd, uint32_t mask, void *data)
{
    jwl_event_source_t *source = data;
    Janet argv[2];
    Janet ret = janet_wrap_nil();
    JanetFiber *fiber = NULL;

    if (source->stream) {
        argv[0] = janet_wrap_abstract(source->stream);
    } else {
        argv[0] = janet_wrap_integer(fd);
    }

    argv[1] = janet_wrap_array(jl_get_flag_keys(mask, wl_event_defs));

    int locked = janet_gclock();
    int sig = janet_pcall(source->cb_fn, 2, argv, &ret, &fiber);
    janet_gcunlock(locked);

    if (JANET_SIGNAL_OK != sig) {
        janet_stacktrace(fiber, ret);
        return 0;
    } else {
        if (janet_checkint(ret)) {
            return janet_unwrap_integer(ret);
        } else {
            janet_eprintf("non-integer return value from event loop fd callback: %v\n", ret);
            return 0;
        }
    }
}


int jwl_event_loop_timer_callback(void *data)
{
    jwl_event_source_t *source = data;
    Janet ret = janet_wrap_nil();
    JanetFiber *fiber = NULL;

    int locked = janet_gclock();
    int sig = janet_pcall(source->cb_fn, 0, NULL, &ret, &fiber);
    janet_gcunlock(locked);

    if (JANET_SIGNAL_OK != sig) {
        janet_stacktrace(fiber, ret);
        return 0;
    } else {
        if (janet_checkint(ret)) {
            return janet_unwrap_integer(ret);
        } else {
            janet_eprintf("non-integer return value from event loop timer callback: %v\n", ret);
            return 0;
        }
    }
}


int jwl_event_loop_signal_callback(int signal_number, void *data)
{
    jwl_event_source_t *source = data;
    Janet ret = janet_wrap_nil();
    JanetFiber *fiber = NULL;
    Janet argv[1];
    const char *signal_name = NULL;

    for (int i = 0; NULL != posix_signal_defs[i].name; i++) {
        if (signal_number == posix_signal_defs[i].key) {
            signal_name = posix_signal_defs[i].name;
            break;
        }
    }

    if (signal_name) {
        argv[0] = janet_ckeywordv(signal_name);
    } else {
        argv[0] = janet_wrap_integer(signal_number);
    }

    int locked = janet_gclock();
    int sig = janet_pcall(source->cb_fn, 1, argv, &ret, &fiber);
    janet_gcunlock(locked);

    if (JANET_SIGNAL_OK != sig) {
        janet_stacktrace(fiber, ret);
        return 0;
    } else {
        if (janet_checkint(ret)) {
            return janet_unwrap_integer(ret);
        } else {
            janet_eprintf("non-integer return value from event loop signal callback: %v\n", ret);
            return 0;
        }
    }
}


void jwl_event_loop_idle_callback(void *data)
{
    jwl_event_source_t *source = data;
    Janet ret = janet_wrap_nil();
    JanetFiber *fiber = NULL;

    int locked = janet_gclock();
    int sig = janet_pcall(source->cb_fn, 0, NULL, &ret, &fiber);
    janet_gcunlock(locked);

    if (JANET_SIGNAL_OK != sig) {
        janet_stacktrace(fiber, ret);
    }
}


typedef struct {
    struct wl_listener wl_listener;
    JanetFunction *notify_fn;
} jwl_listener_t;


void jwl_listener_notify_callback(struct wl_listener *wl_listener, void *data)
{
    jwl_listener_t *listener = wl_container_of(wl_listener, listener, wl_listener);
    JanetFunction *notify_fn = listener->notify_fn;
    Janet argv[] = {
        janet_wrap_abstract(listener),
        janet_wrap_pointer(data),
    };
    Janet ret = janet_wrap_nil();
    JanetFiber *fiber = NULL;
    /* XXX: janet_pcall() without janet_gclock() here causes memory violation,
       don't know why */
    int locked = janet_gclock();
    int sig = janet_pcall(notify_fn, 2, argv, &ret, &fiber);
    janet_gcunlock(locked);
    if (JANET_SIGNAL_OK != sig) {
        janet_stacktrace(fiber, ret);
    }
}


static int method_event_source_gcmark(void *p, size_t len)
{
    (void)len;
    jwl_event_source_t *source = p;

    if (source->stream) {
        janet_mark(janet_wrap_abstract(source->stream));
    }
    if (source->cb_fn) {
        janet_mark(janet_wrap_function(source->cb_fn));
    }

    return 0;
}


static int method_listener_gcmark(void *p, size_t len)
{
    (void)len;
    jwl_listener_t *listener = p;

    if (listener->notify_fn) {
        janet_mark(janet_wrap_function(listener->notify_fn));
    }

    return 0;
}


static Janet cfun_wl_event_loop_create(int32_t argc, Janet *argv)
{
    (void)argv;

    struct wl_event_loop *event_loop;

    janet_fixarity(argc, 0);

    event_loop = wl_event_loop_create();
    if (!event_loop) {
        janet_panic("failed to create wayland event loop");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(event_loop, &jwl_at_wl_event_loop));
}


static Janet cfun_wl_event_loop_destroy(int32_t argc, Janet *argv)
{
    struct wl_event_loop *event_loop;

    janet_fixarity(argc, 1);

    event_loop = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_event_loop);
    wl_event_loop_destroy(event_loop);
    return janet_wrap_nil();
}


static Janet cfun_wl_event_loop_dispatch(int32_t argc, Janet *argv)
{
    struct wl_event_loop *event_loop;
    int timeout;

    janet_fixarity(argc, 2);

    event_loop = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_event_loop);
    timeout = janet_getinteger(argv, 1);
    return janet_wrap_integer(wl_event_loop_dispatch(event_loop, timeout));
}


static Janet cfun_wl_event_loop_dispatch_idle(int32_t argc, Janet *argv)
{
    struct wl_event_loop *event_loop;

    janet_fixarity(argc, 1);

    event_loop = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_event_loop);
    wl_event_loop_dispatch_idle(event_loop);
    return janet_wrap_nil();
}


static Janet cfun_wl_event_loop_add_fd(int32_t argc, Janet *argv)
{
    struct wl_event_loop *event_loop;
    int fd;
    uint32_t mask;
    JanetFunction *func;

    jwl_event_source_t *source;

    janet_fixarity(argc, 4);

    source = janet_abstract(&jwl_at_event_source, sizeof(*source));
    memset(source, 0, sizeof(*source));

    event_loop = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_event_loop);
    if (janet_checkint(argv[1])) {
        fd = janet_getinteger(argv, 1);
    } else {
        JanetStream *stream = janet_getabstract(argv, 1, &janet_stream_type);
        source->stream = stream;
        /* XXX: Check whether the handle is a valid fd? */
        fd = stream->handle;
    }
    mask = jl_get_key_flags(argv, 2, wl_event_defs);
    func = janet_getfunction(argv, 3);

    source->cb_fn = func;
    source->event_source = wl_event_loop_add_fd(event_loop, fd, mask, jwl_event_loop_fd_callback, source);
    if (!(source->event_source)) {
        janet_panic("failed to add fd to wayland event loop");
    }
    return janet_wrap_abstract(source);
}


static Janet cfun_wl_event_source_fd_update(int32_t argc, Janet *argv)
{
    jwl_event_source_t *source;
    uint32_t mask;

    janet_fixarity(argc, 2);

    source = janet_getabstract(argv, 0, &jwl_at_event_source);
    mask = jl_get_key_flags(argv, 1, wl_event_defs);
    return janet_wrap_integer(wl_event_source_fd_update(source->event_source, mask));
}


static Janet cfun_wl_event_loop_add_timer(int32_t argc, Janet *argv)
{
    struct wl_event_loop *event_loop;
    JanetFunction *func;

    jwl_event_source_t *source;

    janet_fixarity(argc, 2);

    source = janet_abstract(&jwl_at_event_source, sizeof(*source));
    memset(source, 0, sizeof(*source));

    event_loop = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_event_loop);
    func = janet_getfunction(argv, 1);

    source->cb_fn = func;
    source->event_source = wl_event_loop_add_timer(event_loop, jwl_event_loop_timer_callback, source);
    if (!(source->event_source)) {
        janet_panic("failed to add timer to wayland event loop");
    }
    return janet_wrap_abstract(source);
}


static Janet cfun_wl_event_source_timer_update(int32_t argc, Janet *argv)
{
    jwl_event_source_t *source;
    int ms_delay;

    janet_fixarity(argc, 2);

    source = janet_getabstract(argv, 0, &jwl_at_event_source);
    ms_delay = janet_getinteger(argv, 1);
    return janet_wrap_integer(wl_event_source_timer_update(source->event_source, ms_delay));
}


static Janet cfun_wl_event_loop_add_signal(int32_t argc, Janet *argv)
{
    struct wl_event_loop *event_loop;
    int signal_number;
    JanetFunction *func;

    jwl_event_source_t *source;

    janet_fixarity(argc, 3);

    source = janet_abstract(&jwl_at_event_source, sizeof(*source));
    memset(source, 0, sizeof(*source));

    event_loop = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_event_loop);
    if (janet_checkint(argv[1])) {
        signal_number = janet_getinteger(argv, 1);
    } else {
        signal_number = jl_get_key_def(argv, 1, posix_signal_defs);
    }
    func = janet_getfunction(argv, 2);

    source->cb_fn = func;
    source->event_source = wl_event_loop_add_signal(event_loop, signal_number, jwl_event_loop_signal_callback, source);
    if (!(source->event_source)) {
        janet_panic("failed to add signal handler to wayland event loop");
    }
    return janet_wrap_abstract(source);
}


static Janet cfun_wl_event_loop_add_idle(int32_t argc, Janet *argv)
{
    struct wl_event_loop *event_loop;
    JanetFunction *func;

    jwl_event_source_t *source;

    janet_fixarity(argc, 2);

    source = janet_abstract(&jwl_at_event_source, sizeof(*source));
    memset(source, 0, sizeof(*source));

    event_loop = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_event_loop);
    func = janet_getfunction(argv, 1);

    source->cb_fn = func;
    source->event_source = wl_event_loop_add_idle(event_loop, jwl_event_loop_idle_callback, source);
    if (!(source->event_source)) {
        janet_panic("failed to add idle source to wayland event loop");
    }
    return janet_wrap_abstract(source);
}


static Janet cfun_wl_event_loop_get_fd(int32_t argc, Janet *argv)
{
    struct wl_event_loop *event_loop;

    janet_fixarity(argc, 1);

    event_loop = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_event_loop);
    return janet_wrap_integer(wl_event_loop_get_fd(event_loop));
}


static Janet cfun_wl_event_loop_add_destroy_listener(int32_t argc, Janet *argv)
{
    struct wl_event_loop *event_loop;
    JanetFunction *notify_fn;

    jwl_listener_t *listener;

    janet_fixarity(argc, 2);

    event_loop = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_event_loop);
    notify_fn = janet_getfunction(argv, 1);

    listener = janet_abstract(&jwl_at_listener, sizeof(*listener));
    listener->wl_listener.notify = jwl_listener_notify_callback;
    listener->notify_fn = notify_fn;

    wl_event_loop_add_destroy_listener(event_loop, &listener->wl_listener);

    return janet_wrap_abstract(listener);
}


static Janet cfun_wl_event_source_check(int32_t argc, Janet *argv)
{
    jwl_event_source_t *source;

    janet_fixarity(argc, 1);

    source = janet_getabstract(argv, 0, &jwl_at_event_source);
    wl_event_source_check(source->event_source);
    return janet_wrap_nil();
}


static Janet cfun_wl_event_source_remove(int32_t argc, Janet *argv)
{
    jwl_event_source_t *source;

    janet_fixarity(argc, 1);

    source = janet_getabstract(argv, 0, &jwl_at_event_source);
    return janet_wrap_integer(wl_event_source_remove(source->event_source));
}


static Janet cfun_wl_list_empty(int32_t argc, Janet *argv)
{
    struct wl_list *list;

    janet_fixarity(argc, 1);

    list = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_list);
    return janet_wrap_boolean(wl_list_empty(list));
}


static Janet cfun_wl_list_length(int32_t argc, Janet *argv)
{
    struct wl_list *list;

    janet_fixarity(argc, 1);

    list = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_list);
    return janet_wrap_integer(wl_list_length(list));
}


static Janet cfun_wl_display_create(int32_t argc, Janet *argv)
{
    (void)argv;

    struct wl_display *display;

    janet_fixarity(argc, 0);

    display = wl_display_create();
    if (!display) {
        janet_panic("failed to create Wayland display object");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(display, &jwl_at_wl_display));
}


static Janet cfun_wl_display_terminate(int32_t argc, Janet *argv)
{
    struct wl_display *display;

    janet_fixarity(argc, 1);

    display = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_display);
    wl_display_terminate(display);

    return janet_wrap_nil();
}


static Janet cfun_wl_display_destroy(int32_t argc, Janet *argv)
{
    struct wl_display *display;

    janet_fixarity(argc, 1);

    display = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_display);
    wl_display_destroy(display);

    return janet_wrap_nil();
}


static Janet cfun_wl_display_destroy_clients(int32_t argc, Janet *argv)
{
    struct wl_display *display;

    janet_fixarity(argc, 1);

    display = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_display);
    wl_display_destroy_clients(display);

    return janet_wrap_nil();
}


static Janet cfun_wl_display_run(int32_t argc, Janet *argv)
{
    struct wl_display *display;

    janet_fixarity(argc, 1);

    display = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_display);
    wl_display_run(display);

    return janet_wrap_nil();
}


static Janet cfun_wl_display_add_socket_auto(int32_t argc, Janet *argv)
{
    struct wl_display *display;

    const char *socket;

    janet_fixarity(argc, 1);

    display = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_display);
    socket = wl_display_add_socket_auto(display);
    if (!socket) {
        janet_panic("failed to create Wayland socket");
    }
    return janet_cstringv(socket);
}


static Janet cfun_wl_display_get_event_loop(int32_t argc, Janet *argv)
{
    struct wl_display *display;

    struct wl_event_loop *event_loop;

    janet_fixarity(argc, 1);

    display = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_display);
    event_loop = wl_display_get_event_loop(display);
    if (!event_loop) {
        janet_panic("failed to get Wayland event loop");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(event_loop, &jwl_at_wl_event_loop));
}


static Janet cfun_wl_signal_add(int32_t argc, Janet *argv)
{
    struct wl_signal *signal;
    JanetFunction *notify_fn;

    jwl_listener_t *listener;

    janet_fixarity(argc, 2);

    signal = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_signal);
    notify_fn = janet_getfunction(argv, 1);

    listener = janet_abstract(&jwl_at_listener, sizeof(*listener));
    listener->wl_listener.notify = jwl_listener_notify_callback;
    listener->notify_fn = notify_fn;

    wl_signal_add(signal, &listener->wl_listener);

    return janet_wrap_abstract(listener);
}


static Janet cfun_wl_signal_remove(int32_t argc, Janet *argv)
{
    jwl_listener_t *listener;

    janet_fixarity(argc, 1);

    listener = janet_getabstract(argv, 0, &jwl_at_listener);

    wl_list_remove(&listener->wl_listener.link);
    janet_gcunroot(janet_wrap_function(listener->notify_fn));

    return janet_wrap_nil();
}


static Janet cfun_wl_signal_emit(int32_t argc, Janet *argv)
{
    struct wl_signal *signal;
    Janet data;

    janet_fixarity(argc, 2);

    signal = jl_get_abs_obj_pointer(argv, 0, &jwl_at_wl_signal);
    data = argv[1];

    /* Here we pass a raw pointer to the Janet listener function. Depending on the
       type of your listener, use (util/get-listener-data ...) or
       (util/get-abstract-listener-data ...) to get the actual value. */
    wl_signal_emit(signal, &data);

    return janet_wrap_nil();
}


static JanetReg cfuns[] = {
    {
        "wl-event-loop-create", cfun_wl_event_loop_create,
        "(" MOD_NAME "/wl-event-loop-create)\n\n"
        "Creates a Wayland event loop."
    },
    {
        "wl-event-loop-destroy", cfun_wl_event_loop_destroy,
        "(" MOD_NAME "/wl-event-loop-destroy)\n\n"
        "Destroys a Wayland event loop."
    },
    {
        "wl-event-loop-dispatch", cfun_wl_event_loop_dispatch,
        "(" MOD_NAME "/wl-event-loop-dispatch wl-event-loop timeout)\n\n"
        "Dispatches events from the event sources registered in the event loop."
    },
    {
        "wl-event-loop-dispatch-idle", cfun_wl_event_loop_dispatch_idle,
        "(" MOD_NAME "/wl-event-loop-dispatch-idle wl-event-loop)\n\n"
        "Dispatches idle events registered in the event loop."
    },
    {
        "wl-event-loop-add-fd", cfun_wl_event_loop_add_fd,
        "(" MOD_NAME "/wl-event-loop-add-fd wl-event-loop fd-or-stream mask func)\n\n"
        "Adds a file descriptor as an event source."
    },
    {
        "wl-event-source-fd-update", cfun_wl_event_source_fd_update,
        "(" MOD_NAME "/wl-event-source-fd-update event-source mask)\n\n"
        "Updates a file descriptor source's event mask."
    },
    {
        "wl-event-loop-add-timer", cfun_wl_event_loop_add_timer,
        "(" MOD_NAME "/wl-event-loop-add-timer wl-event-loop func)\n\n"
        "Adds a timer as an event source."
    },
    {
        "wl-event-source-timer-update", cfun_wl_event_source_timer_update,
        "(" MOD_NAME "/wl-event-source-timer-update event-source ms-delay)\n\n"
        "Updates a timer source's expiration time."
    },
    {
        "wl-event-loop-add-signal", cfun_wl_event_loop_add_signal,
        "(" MOD_NAME "/wl-event-loop-add-signal wl-event-loop signal func)\n\n"
        "Blocks a POSIX signal and dispatches it through the event loop."
    },
    {
        "wl-event-loop-add-idle", cfun_wl_event_loop_add_idle,
        "(" MOD_NAME "/wl-event-loop-add-idle wl-event-loop func)\n\n"
        "Adds an idle event source to the event loop."
    },
    {
        "wl-event-loop-get-fd", cfun_wl_event_loop_get_fd,
        "(" MOD_NAME "/wl-event-loop-get-fd wl-event-loop)\n\n"
        "Returns the file descriptor for the event loop."
    },
    {
        "wl-event-loop-add-destroy-listener", cfun_wl_event_loop_add_destroy_listener,
        "(" MOD_NAME "/wl-event-loop-add-destroy-listener wl-event-loop func)\n\n"
        "Listens to the destroy event for the event loop."
    },
    {
        "wl-event-source-check", cfun_wl_event_source_check,
        "(" MOD_NAME "/wl-event-source-check event-source)\n\n"
        "Schedules an event source to be checked again by the event loop."
    },
    {
        "wl-event-source-remove", cfun_wl_event_source_remove,
        "(" MOD_NAME "/wl-event-source-remove event-source)\n\n"
        "Removes an event source from the event loop."
    },
    {
        "wl-list-empty", cfun_wl_list_empty,
        "(" MOD_NAME "/wl-list-empty wl-list)\n\n"
        "Check if a wl-list is empty."
    },
    {
        "wl-list-length", cfun_wl_list_length,
        "(" MOD_NAME "/wl-list-length wl-list)\n\n"
        "Returns the length of a wl-list."
    },
    {
        "wl-display-create", cfun_wl_display_create,
        "(" MOD_NAME "/wl-display-create)\n\n"
        "Creates a Wayland display object."
    },
    {
        "wl-display-terminate", cfun_wl_display_terminate,
        "(" MOD_NAME "/wl-display-terminate)\n\n"
        "Stops a Wayland display event loop."
    },
    {
        "wl-display-destroy", cfun_wl_display_destroy,
        "(" MOD_NAME "/wl-display-destroy wl-display)\n\n"
        "Destroys a Wayland display object."
    },
    {
        "wl-display-destroy-clients", cfun_wl_display_destroy_clients,
        "(" MOD_NAME "/wl-display-destroy-clients wl-display)\n\n"
        "Destroys all clients connected to the Wayland display."
    },
    {
        "wl-display-run", cfun_wl_display_run,
        "(" MOD_NAME "/wl-display-run wl-display)\n\n"
        "Runs the Wayland event loop."
    },
    {
        "wl-display-add-socket-auto", cfun_wl_display_add_socket_auto,
        "(" MOD_NAME "/wl-display-add-socket-auto wl-display)\n\n"
        "Adds a Unix socket to the Wayland display."
    },
    {
        "wl-display-get-event-loop", cfun_wl_display_get_event_loop,
        "(" MOD_NAME "/wl-display-get-event-loop wl-display)\n\n"
        "Retrieves the Wayland event loop."
    },
    {
        "wl-signal-add", cfun_wl_signal_add,
        "(" MOD_NAME "/wl-signal-add wl-signal notify-fn)\n\n"
        "Adds a listener to a signal. Returns a new listener object which "
        "can be used to remove notify-fn from the signal."
    },
    {
        "wl-signal-remove", cfun_wl_signal_remove,
        "(" MOD_NAME "/wl-signal-remove listener)\n\n"
        "Removes a listener from a signal."
    },
    {
        "wl-signal-emit", cfun_wl_signal_emit,
        "(" MOD_NAME "/wl-signal-emit wl-signal data)\n\n"
        "Emits a signal."
    },
    {NULL, NULL, NULL},
};


JANET_MODULE_ENTRY(JanetTable *env)
{
    janet_register_abstract_type(&jwl_at_wl_event_loop);
    janet_register_abstract_type(&jwl_at_event_source);
    janet_register_abstract_type(&jwl_at_wl_list);
    janet_register_abstract_type(&jwl_at_wl_signal);
    janet_register_abstract_type(&jwl_at_listener);
    janet_register_abstract_type(&jwl_at_wl_display);

    janet_cfuns(env, MOD_NAME, cfuns);
}
