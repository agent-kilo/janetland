#include <stdio.h>

#include <janet.h>

#include <wayland-server-core.h>

#include <wlr/util/log.h>

#include "jl.h"
#include "types.h"

#define MOD_NAME "wlr"


#define WL_MOD_NAME "janetland/wl"


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
    {NULL, NULL, NULL},
};


JANET_MODULE_ENTRY(JanetTable *env)
{
    Janet import_fn = janet_resolve_core("import*");
    if (!janet_checktype(import_fn, JANET_FUNCTION)) {
        janet_panic("core function import* not found");
    }
    Janet import_argv[] = {
        janet_cstringv(WL_MOD_NAME),
    };
    /* Import wl module first, so that we can find the wl_* abstract types */
    /* XXX: this will pollute the environment with wl/ stuff, even :export is set to nil */
    janet_call(janet_unwrap_function(import_fn), 1, import_argv);

    janet_cfuns(env, MOD_NAME, cfuns);
}
