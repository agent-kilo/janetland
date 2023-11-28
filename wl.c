#include <janet.h>

#include <wayland-server-core.h>

#include "jl.h"
#include "types.h"
#include "wl_abs_types.h"


#ifndef MOD_NAME
#define MOD_NAME WL_MOD_NAME
#endif


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
    janet_gcroot(janet_wrap_function(notify_fn));

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
    janet_register_abstract_type(&jwl_at_wl_list);
    janet_register_abstract_type(&jwl_at_wl_signal);
    janet_register_abstract_type(&jwl_at_listener);
    janet_register_abstract_type(&jwl_at_wl_display);

    janet_cfuns(env, MOD_NAME, cfuns);
}
