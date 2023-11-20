#include <stdio.h>

#include <janet.h>

#include <wayland-server-core.h>

/*
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
*/
#include <wlr/util/log.h>

/*
#include <xkbcommon/xkbcommon.h>
*/

#define MOD_NAME "wlr"


JANET_THREAD_LOCAL JanetFunction *jwlr_log_callback_fn;


static const KeyDef log_defs[] = {
    {"silent", WLR_SILENT},
    {"error", WLR_ERROR},
    {"info", WLR_INFO},
    {"debug", WLR_DEBUG},
    {"log-importance-last", WLR_LOG_IMPORTANCE_LAST},
};

static int jwlr_get_log_importance(const Janet *argv, int32_t n)
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
    Janet argv[2];
    va_list args_copy;
    int str_len;
    JanetBuffer *buf = janet_buffer(256); /* arbitrary size */

    argv[0] = janet_ckeywordv(log_defs[importance].name);

    va_copy(args_copy, args);
    str_len = vsnprintf(buf->data, buf->capacity, fmt, args_copy);
    if (str_len < 0) {
        fprintf(stderr, "%s:%d - vsnprintf() failed, fmt = \"%s\"\n", __FILE__, __LINE__, fmt);
        return;
    }
    if (str_len >= buf->capacity) {
        janet_buffer_ensure(buf, str_len + 1, 1);
        str_len = vsnprintf((char *)(buf->data), buf->capacity, fmt, args_copy);
    }
    buf->count = str_len;
    argv[1] = janet_wrap_buffer(buf);

    Janet ret = janet_wrap_nil();
    JanetFiber *fiber = NULL;
    int sig = janet_pcall(jwlr_log_callback_fn, 2, argv, &ret, &fiber);
    if (JANET_SIGNAL_OK != sig) {
        janet_stacktrace(fiber, out);
    }
}


static Janet cfun_wlr_log_init(int32_t argc, Janet *argv)
{
    enum wlr_log_importance verbosity;
    JanetFunction *cb;

    wlr_log_func_t ccb = NULL;

    janet_arity(argc, 1, 2);

    verbosity = janet_getinteger(argv, 0);
    cb = janet_optfunction(argv, argc, 1, NULL);
    if (cb) {
        jwlr_log_callback_fn = cb;
        janet_gcroot(janet_wrap_function(cb));
        ccb = jwlr_log_callback;
    }

    wlr_log_init(verbosity, ccb);

    return janet_wrap_nil();
}


static JanetReg cfuns[] = {
    {
        "wlr_log_init", cfun_wlr_log_init, 
        "(" MOD_NAME "/wlr_log_init verbosity callback)\n\n" 
        "Initializes log infrastructure."
    },
    {NULL, NULL, NULL},
};


JANET_MODULE_ENTRY(JanetTable *env)
{
    janet_cfuns(env, MOD_NAME, cfuns);
}
