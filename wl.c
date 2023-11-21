#include <janet.h>

#include <wayland-server-core.h>

#include "jl.h"
#include "types.h"

#define MOD_NAME "wl"


static const JanetAbstractType jwl_at_wl_display = {
    .name = MOD_NAME "/wl-display",
    .gc = NULL, /* TODO: close the display? */
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

static Janet cfun_wl_display_create(int32_t argc, Janet *argv)
{
    struct wl_display **display;

    (void)argv;
    janet_fixarity(argc, 0);

    display = janet_abstract(&jwl_at_wl_display, sizeof(*display));
    *display = wl_display_create();
    if (!(*display)) {
        janet_panic("failed to create Wayland display object");
    }
    return janet_wrap_abstract(display);
}


static const JanetAbstractType jwl_at_wl_signal = {
    .name = MOD_NAME "/wl-signal",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

typedef struct {
    struct wl_listener wl_listener;
    JanetFunction *notify_fn;
} jwl_listener_t;

static const JanetAbstractType jwl_at_listener = {
    .name = MOD_NAME "/wl-listener",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

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
    int sig = janet_pcall(notify_fn, 2, argv, &ret, &fiber);
    if (JANET_SIGNAL_OK != sig) {
        janet_stacktrace(fiber, ret);
    }
}

static Janet cfun_wl_signal_add(int32_t argc, Janet *argv)
{
    struct wl_signal **signal;
    JanetFunction *notify_fn;

    jwl_listener_t *listener;

    janet_fixarity(argc, 2);

    signal = janet_getabstract(argv, 0, &jwl_at_wl_signal);
    notify_fn = janet_getfunction(argv, 1);

    listener = janet_abstract(&jwl_at_listener, sizeof(*listener));
    listener->wl_listener.notify = jwl_listener_notify_callback;
    listener->notify_fn = notify_fn;
    janet_gcroot(janet_wrap_function(notify_fn));

    wl_signal_add(*signal, &listener->wl_listener);

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
    struct wl_signal **signal_p;
    Janet data;

    janet_fixarity(argc, 2);

    signal_p = janet_getabstract(argv, 0, &jwl_at_wl_signal);
    data = argv[1];

    wl_signal_emit(*signal_p, &data);

    return janet_wrap_nil();
}


static JanetReg cfuns[] = {
    {
        "wl-display-create", cfun_wl_display_create,
        "(" MOD_NAME "/wl-display-create)\n\n"
        "Creates a Wayland display object."
    },
    {
        "wl-signal-add", cfun_wl_signal_add,
        "(" MOD_NAME "/wl-signal-add wl-signal notify-fn)\n\n"
        "Adds a listener to a signal. Returns a new listener object which "
        "can be used to remove notify-fn from the signal."
    },
    {
        "wl-signal-remove", cfun_wl_signal_remove,
        "(" MOD_NAME "/wl-signal-remove wl-listener)\n\n"
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
    janet_register_abstract_type(&jwl_at_wl_display);
    janet_register_abstract_type(&jwl_at_wl_signal);
    janet_register_abstract_type(&jwl_at_listener);

    janet_cfuns(env, MOD_NAME, cfuns);
}
