#include <stdbool.h>

#include <janet.h>

#include <xcb/xcb.h>

#include "jl.h"
#include "types.h"


#ifndef MOD_NAME
#define MOD_NAME XCB_MOD_NAME
#endif


static const JanetAbstractType jxcb_at_intern_atom_cookie_t = {
    .name = MOD_NAME "/intern-atom-cookie-t",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static int method_xcb_intern_atom_reply_t_gc(void *data, size_t len)
{
    (void)len;
    xcb_intern_atom_reply_t **reply_p = data;
    xcb_intern_atom_reply_t *reply = *reply_p;
    free(reply);
    return 0;
}

static int method_xcb_intern_atom_reply_t_get(void *p, Janet key, Janet *out)
{
    xcb_intern_atom_reply_t **reply_p = p;
    xcb_intern_atom_reply_t *reply = *reply_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "atom")) {
        /* uint32_t -> uint64_t conversion */
        *out = janet_wrap_u64(reply->atom);
        return 1;
    }

    return 0;
}


static const JanetAbstractType jxcb_at_xcb_intern_atom_reply_t = {
    .name = MOD_NAME "/xcb-intern-atom-reply-t",
    .gc = method_xcb_intern_atom_reply_t_gc,
    .gcmark = NULL,
    .get = method_xcb_intern_atom_reply_t_get,
    JANET_ATEND_GET
};


static int method_xcb_generic_error_t_gc(void *data, size_t len)
{
    (void)len;
    xcb_generic_error_t **error_p = data;
    xcb_generic_error_t *error = *error_p;
    free(error);
    return 0;
}

static int method_xcb_generic_error_t_get(void *p, Janet key, Janet *out)
{
    xcb_generic_error_t **error_p = p;
    xcb_generic_error_t *error = *error_p;

    if (!janet_checktype(key, JANET_KEYWORD)) {
        janet_panicf("expected keyword, got %v", key);
    }

    const uint8_t *kw = janet_unwrap_keyword(key);

    if (!janet_cstrcmp(kw, "error-code")) {
        *out = janet_wrap_integer(error->error_code);
        return 1;
    }

    return 0;
}

static const JanetAbstractType jxcb_at_xcb_generic_error_t = {
    .name = MOD_NAME "/xcb-generic-error-t",
    .gc = method_xcb_generic_error_t_gc,
    .gcmark = NULL,
    .get = method_xcb_generic_error_t_get,
    JANET_ATEND_GET
};


#define XCB_CONN_ERROR_DEFS_MAX 7
static const jl_key_def_t xcb_conn_error_defs[] = {
    {"none", 0},
    {"error", XCB_CONN_ERROR},
    {"closed-ext-notsupported", XCB_CONN_CLOSED_EXT_NOTSUPPORTED},
    {"closed-mem-insufficient", XCB_CONN_CLOSED_MEM_INSUFFICIENT},
    {"closed-req-len-exceed", XCB_CONN_CLOSED_REQ_LEN_EXCEED},
    {"closed-parse-err", XCB_CONN_CLOSED_PARSE_ERR},
    {"closed-invalid-screen", XCB_CONN_CLOSED_INVALID_SCREEN},
    {"closed-fdpassing-failed", XCB_CONN_CLOSED_FDPASSING_FAILED},
    {NULL, 0},
};


static const JanetAbstractType jxcb_at_xcb_connection_t = {
    .name = MOD_NAME "/xcb-connection-t",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static Janet cfun_xcb_connect(int32_t argc, Janet *argv)
{
    const char *display_name;

    xcb_connection_t *conn;
    int screen_num = 0;
    Janet ret_tuple[2];

    janet_fixarity(argc, 1);

    display_name = janet_getcstring(argv, 0);

    /* conn is always non-NULL, see xcb doc. */
    conn = xcb_connect(display_name, &screen_num);
    ret_tuple[0] = janet_wrap_abstract(jl_pointer_to_abs_obj(conn, &jxcb_at_xcb_connection_t));
    ret_tuple[1] = janet_wrap_integer(screen_num);

    return janet_wrap_tuple(janet_tuple_n(ret_tuple, 2));
}


static Janet cfun_xcb_disconnect(int32_t argc, Janet *argv)
{
    xcb_connection_t *conn;

    janet_fixarity(argc, 1);

    conn = jl_get_abs_obj_pointer(argv, 0, &jxcb_at_xcb_connection_t);
    xcb_disconnect(conn);
    return janet_wrap_nil();
}


static Janet cfun_xcb_connection_has_error(int32_t argc, Janet *argv)
{
    xcb_connection_t *conn;

    int err = 0;

    janet_fixarity(argc, 1);

    conn = jl_get_abs_obj_pointer(argv, 0, &jxcb_at_xcb_connection_t);
    err = xcb_connection_has_error(conn);
    if (err < 0 || err > XCB_CONN_ERROR_DEFS_MAX) {
        janet_panicf("unknown error code from xcb connection: %d", err);
    }
    return janet_ckeywordv(xcb_conn_error_defs[err].name);
}


static Janet cfun_xcb_intern_atom(int32_t argc, Janet *argv)
{
    xcb_connection_t *conn;
    bool only_if_exists;
    const char *name;

    xcb_intern_atom_cookie_t *cookie;

    janet_fixarity(argc, 3);

    conn = jl_get_abs_obj_pointer(argv, 0, &jxcb_at_xcb_connection_t);
    only_if_exists = janet_getboolean(argv, 1);
    name = janet_getcstring(argv, 2);

    cookie = janet_abstract(&jxcb_at_intern_atom_cookie_t, sizeof(*cookie));
    *cookie = xcb_intern_atom(conn, only_if_exists ? 1 : 0, strlen(name), name);
    return janet_wrap_abstract(cookie);
}


static Janet cfun_xcb_intern_atom_reply(int32_t argc, Janet *argv)
{
    xcb_connection_t *conn;
    xcb_intern_atom_cookie_t *cookie;

    xcb_intern_atom_reply_t *reply;
    xcb_generic_error_t *err = NULL;
    Janet ret_tuple[2];

    janet_fixarity(argc, 2);

    conn = jl_get_abs_obj_pointer(argv, 0, &jxcb_at_xcb_connection_t);
    cookie = janet_getabstract(argv, 1, &jxcb_at_intern_atom_cookie_t);

    reply = xcb_intern_atom_reply(conn, *cookie, &err);
    if (!reply) {
        ret_tuple[0] = janet_wrap_nil();
    } else {
        ret_tuple[0] = janet_wrap_abstract(jl_pointer_to_abs_obj(reply, &jxcb_at_xcb_intern_atom_reply_t));
    }
    if (!err) {
        ret_tuple[1] = janet_wrap_nil();
    } else {
        ret_tuple[1] = janet_wrap_abstract(jl_pointer_to_abs_obj(err, &jxcb_at_xcb_generic_error_t));
    }

    return janet_wrap_tuple(janet_tuple_n(ret_tuple, 2));
}


#define __XCB_ATOM_COUNT 70

static void define_xcb_atom_enum_t_constants(JanetTable *env)
{
    JanetKV *const_map = janet_struct_begin(__XCB_ATOM_COUNT);

    janet_struct_put(const_map, janet_ckeywordv("none"), janet_wrap_u64(XCB_ATOM_NONE));
    janet_struct_put(const_map, janet_ckeywordv("any"), janet_wrap_u64(XCB_ATOM_ANY));
    janet_struct_put(const_map, janet_ckeywordv("primary"), janet_wrap_u64(XCB_ATOM_PRIMARY));
    janet_struct_put(const_map, janet_ckeywordv("secondary"), janet_wrap_u64(XCB_ATOM_SECONDARY));
    janet_struct_put(const_map, janet_ckeywordv("arc"), janet_wrap_u64(XCB_ATOM_ARC));
    janet_struct_put(const_map, janet_ckeywordv("atom"), janet_wrap_u64(XCB_ATOM_ATOM));
    janet_struct_put(const_map, janet_ckeywordv("bitmap"), janet_wrap_u64(XCB_ATOM_BITMAP));
    janet_struct_put(const_map, janet_ckeywordv("cardinal"), janet_wrap_u64(XCB_ATOM_CARDINAL));
    janet_struct_put(const_map, janet_ckeywordv("colormap"), janet_wrap_u64(XCB_ATOM_COLORMAP));
    janet_struct_put(const_map, janet_ckeywordv("cursor"), janet_wrap_u64(XCB_ATOM_CURSOR));
    janet_struct_put(const_map, janet_ckeywordv("cut-buffer0"), janet_wrap_u64(XCB_ATOM_CUT_BUFFER0));
    janet_struct_put(const_map, janet_ckeywordv("cut-buffer1"), janet_wrap_u64(XCB_ATOM_CUT_BUFFER1));
    janet_struct_put(const_map, janet_ckeywordv("cut-buffer2"), janet_wrap_u64(XCB_ATOM_CUT_BUFFER2));
    janet_struct_put(const_map, janet_ckeywordv("cut-buffer3"), janet_wrap_u64(XCB_ATOM_CUT_BUFFER3));
    janet_struct_put(const_map, janet_ckeywordv("cut-buffer4"), janet_wrap_u64(XCB_ATOM_CUT_BUFFER4));
    janet_struct_put(const_map, janet_ckeywordv("cut-buffer5"), janet_wrap_u64(XCB_ATOM_CUT_BUFFER5));
    janet_struct_put(const_map, janet_ckeywordv("cut-buffer6"), janet_wrap_u64(XCB_ATOM_CUT_BUFFER6));
    janet_struct_put(const_map, janet_ckeywordv("cut-buffer7"), janet_wrap_u64(XCB_ATOM_CUT_BUFFER7));
    janet_struct_put(const_map, janet_ckeywordv("drawable"), janet_wrap_u64(XCB_ATOM_DRAWABLE));
    janet_struct_put(const_map, janet_ckeywordv("font"), janet_wrap_u64(XCB_ATOM_FONT));
    janet_struct_put(const_map, janet_ckeywordv("integer"), janet_wrap_u64(XCB_ATOM_INTEGER));
    janet_struct_put(const_map, janet_ckeywordv("pixmap"), janet_wrap_u64(XCB_ATOM_PIXMAP));
    janet_struct_put(const_map, janet_ckeywordv("point"), janet_wrap_u64(XCB_ATOM_POINT));
    janet_struct_put(const_map, janet_ckeywordv("rectangle"), janet_wrap_u64(XCB_ATOM_RECTANGLE));
    janet_struct_put(const_map, janet_ckeywordv("resource-manager"), janet_wrap_u64(XCB_ATOM_RESOURCE_MANAGER));
    janet_struct_put(const_map, janet_ckeywordv("rgb-color-map"), janet_wrap_u64(XCB_ATOM_RGB_COLOR_MAP));
    janet_struct_put(const_map, janet_ckeywordv("rgb-best-map"), janet_wrap_u64(XCB_ATOM_RGB_BEST_MAP));
    janet_struct_put(const_map, janet_ckeywordv("rgb-blue-map"), janet_wrap_u64(XCB_ATOM_RGB_BLUE_MAP));
    janet_struct_put(const_map, janet_ckeywordv("rgb-default-map"), janet_wrap_u64(XCB_ATOM_RGB_DEFAULT_MAP));
    janet_struct_put(const_map, janet_ckeywordv("rgb-gray-map"), janet_wrap_u64(XCB_ATOM_RGB_GRAY_MAP));
    janet_struct_put(const_map, janet_ckeywordv("rgb-green-map"), janet_wrap_u64(XCB_ATOM_RGB_GREEN_MAP));
    janet_struct_put(const_map, janet_ckeywordv("rgb-red-map"), janet_wrap_u64(XCB_ATOM_RGB_RED_MAP));
    janet_struct_put(const_map, janet_ckeywordv("string"), janet_wrap_u64(XCB_ATOM_STRING));
    janet_struct_put(const_map, janet_ckeywordv("visualid"), janet_wrap_u64(XCB_ATOM_VISUALID));
    janet_struct_put(const_map, janet_ckeywordv("window"), janet_wrap_u64(XCB_ATOM_WINDOW));
    janet_struct_put(const_map, janet_ckeywordv("wm-command"), janet_wrap_u64(XCB_ATOM_WM_COMMAND));
    janet_struct_put(const_map, janet_ckeywordv("wm-hints"), janet_wrap_u64(XCB_ATOM_WM_HINTS));
    janet_struct_put(const_map, janet_ckeywordv("wm-client-machine"), janet_wrap_u64(XCB_ATOM_WM_CLIENT_MACHINE));
    janet_struct_put(const_map, janet_ckeywordv("wm-icon-name"), janet_wrap_u64(XCB_ATOM_WM_ICON_NAME));
    janet_struct_put(const_map, janet_ckeywordv("wm-icon-size"), janet_wrap_u64(XCB_ATOM_WM_ICON_SIZE));
    janet_struct_put(const_map, janet_ckeywordv("wm-name"), janet_wrap_u64(XCB_ATOM_WM_NAME));
    janet_struct_put(const_map, janet_ckeywordv("wm-normal-hints"), janet_wrap_u64(XCB_ATOM_WM_NORMAL_HINTS));
    janet_struct_put(const_map, janet_ckeywordv("wm-size-hints"), janet_wrap_u64(XCB_ATOM_WM_SIZE_HINTS));
    janet_struct_put(const_map, janet_ckeywordv("wm-zoom-hints"), janet_wrap_u64(XCB_ATOM_WM_ZOOM_HINTS));
    janet_struct_put(const_map, janet_ckeywordv("min-space"), janet_wrap_u64(XCB_ATOM_MIN_SPACE));
    janet_struct_put(const_map, janet_ckeywordv("norm-space"), janet_wrap_u64(XCB_ATOM_NORM_SPACE));
    janet_struct_put(const_map, janet_ckeywordv("max-space"), janet_wrap_u64(XCB_ATOM_MAX_SPACE));
    janet_struct_put(const_map, janet_ckeywordv("end-space"), janet_wrap_u64(XCB_ATOM_END_SPACE));
    janet_struct_put(const_map, janet_ckeywordv("superscript-x"), janet_wrap_u64(XCB_ATOM_SUPERSCRIPT_X));
    janet_struct_put(const_map, janet_ckeywordv("superscript-y"), janet_wrap_u64(XCB_ATOM_SUPERSCRIPT_Y));
    janet_struct_put(const_map, janet_ckeywordv("subscript-x"), janet_wrap_u64(XCB_ATOM_SUBSCRIPT_X));
    janet_struct_put(const_map, janet_ckeywordv("subscript-y"), janet_wrap_u64(XCB_ATOM_SUBSCRIPT_Y));
    janet_struct_put(const_map, janet_ckeywordv("underline-position"), janet_wrap_u64(XCB_ATOM_UNDERLINE_POSITION));
    janet_struct_put(const_map, janet_ckeywordv("underline-thickness"), janet_wrap_u64(XCB_ATOM_UNDERLINE_THICKNESS));
    janet_struct_put(const_map, janet_ckeywordv("strikeout-ascent"), janet_wrap_u64(XCB_ATOM_STRIKEOUT_ASCENT));
    janet_struct_put(const_map, janet_ckeywordv("strikeout-descent"), janet_wrap_u64(XCB_ATOM_STRIKEOUT_DESCENT));
    janet_struct_put(const_map, janet_ckeywordv("italic-angle"), janet_wrap_u64(XCB_ATOM_ITALIC_ANGLE));
    janet_struct_put(const_map, janet_ckeywordv("x-height"), janet_wrap_u64(XCB_ATOM_X_HEIGHT));
    janet_struct_put(const_map, janet_ckeywordv("quad-width"), janet_wrap_u64(XCB_ATOM_QUAD_WIDTH));
    janet_struct_put(const_map, janet_ckeywordv("weight"), janet_wrap_u64(XCB_ATOM_WEIGHT));
    janet_struct_put(const_map, janet_ckeywordv("point-size"), janet_wrap_u64(XCB_ATOM_POINT_SIZE));
    janet_struct_put(const_map, janet_ckeywordv("resolution"), janet_wrap_u64(XCB_ATOM_RESOLUTION));
    janet_struct_put(const_map, janet_ckeywordv("copyright"), janet_wrap_u64(XCB_ATOM_COPYRIGHT));
    janet_struct_put(const_map, janet_ckeywordv("notice"), janet_wrap_u64(XCB_ATOM_NOTICE));
    janet_struct_put(const_map, janet_ckeywordv("font-name"), janet_wrap_u64(XCB_ATOM_FONT_NAME));
    janet_struct_put(const_map, janet_ckeywordv("family-name"), janet_wrap_u64(XCB_ATOM_FAMILY_NAME));
    janet_struct_put(const_map, janet_ckeywordv("full-name"), janet_wrap_u64(XCB_ATOM_FULL_NAME));
    janet_struct_put(const_map, janet_ckeywordv("cap-height"), janet_wrap_u64(XCB_ATOM_CAP_HEIGHT));
    janet_struct_put(const_map, janet_ckeywordv("wm-class"), janet_wrap_u64(XCB_ATOM_WM_CLASS));
    janet_struct_put(const_map, janet_ckeywordv("wm-transient-for"), janet_wrap_u64(XCB_ATOM_WM_TRANSIENT_FOR));

    janet_def(env, "xcb-atom-enum", janet_wrap_struct(janet_struct_end(const_map)),
              "Maps constant names to xcb_atom_enum_t values.");
}


static JanetReg cfuns[] = {
    {
        "xcb-connect", cfun_xcb_connect,
        "(" MOD_NAME "/xcb-connect display-name)\n\n"
        "Connects to the X server."
    },
    {
        "xcb-disconnect", cfun_xcb_disconnect,
        "(" MOD_NAME "/xcb-disconnect xcb-connection)\n\n"
        "Disconnects from the X server."
    },
    {
        "xcb-connection-has-error", cfun_xcb_connection_has_error,
        "(" MOD_NAME "/xcb-connection-has-error xcb-connection)\n\n"
        "Tests whether the connection has shut down due to a fatal error."
    },
    /* XXX: Should be generating these xcb APIs from protocol definition? */
    {
        "xcb-intern-atom", cfun_xcb_intern_atom,
        "(" MOD_NAME "/xcb-intern-atom xcb-connection only-if-exists name)\n\n"
        "Retrieves the identifier for the atom with the specified name."
    },
    {
        "xcb-intern-atom-reply", cfun_xcb_intern_atom_reply,
        "(" MOD_NAME "/xcb-intern-atom-reply xcb-connection intern-atom-cookie)\n\n"
        "Returns the reply of the intern atom request."
    },
    {NULL, NULL, NULL},
};


JANET_MODULE_ENTRY(JanetTable *env)
{
    define_xcb_atom_enum_t_constants(env);

    janet_register_abstract_type(&jxcb_at_xcb_connection_t);
    janet_register_abstract_type(&jxcb_at_xcb_generic_error_t);
    janet_register_abstract_type(&jxcb_at_intern_atom_cookie_t);
    janet_register_abstract_type(&jxcb_at_xcb_intern_atom_reply_t);

    janet_cfuns(env, MOD_NAME, cfuns);
}
