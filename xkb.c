#include <janet.h>

#include <xkbcommon/xkbcommon.h>

#include "jl.h"
#include "types.h"


#ifndef MOD_NAME
#define MOD_NAME XKB_MOD_NAME
#endif


static const jl_key_def_t xkb_context_flags_defs[] = {
    {"no-flags", XKB_CONTEXT_NO_FLAGS},
    {"no-default-includes", XKB_CONTEXT_NO_DEFAULT_INCLUDES},
    {"no-environment-names", XKB_CONTEXT_NO_ENVIRONMENT_NAMES},
    {NULL, 0},
};

static const JanetAbstractType jxkb_at_xkb_context = {
    .name = MOD_NAME "/xkb-context",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static Janet cfun_xkb_context_new(int32_t argc, Janet *argv)
{
    enum xkb_context_flags flags;

    struct xkb_context *context;

    janet_fixarity(argc, 1);

    flags = jl_get_key_flags(argv, 0, xkb_context_flags_defs);
    context = xkb_context_new(flags);
    if (!context) {
        janet_panic("failed to create xkb context");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(context, &jxkb_at_xkb_context));
}


static int method_xkb_rule_names_gcmark(void *p, size_t len)
{
    (void)len;
    struct xkb_rule_names* names = (struct xkb_rule_names *)p;

    if (names->rules) {
        janet_mark(janet_wrap_string(names->rules));
    }
    if (names->model) {
        janet_mark(janet_wrap_string(names->model));
    }
    if (names->layout) {
        janet_mark(janet_wrap_string(names->layout));
    }
    if (names->variant) {
        janet_mark(janet_wrap_string(names->variant));
    }
    if (names->options) {
        janet_mark(janet_wrap_string(names->options));
    }

    return 0;
}

static const JanetAbstractType jxkb_at_xkb_rule_names = {
    .name = MOD_NAME "/xkb-rule-names",
    .gc = NULL,
    .gcmark = method_xkb_rule_names_gcmark,
    JANET_ATEND_GCMARK
};

static const char *jxkb_get_xkb_rule_names_member(Janet *argv, int32_t n)
{
    switch (janet_type(argv[n])) {
    case JANET_NIL:
        return NULL;
    case JANET_STRING:
        return janet_getcstring(argv, n);
    default:
        janet_panicf("bad slot #%d: expected string or nil, got %v", n, argv[n]);
    }
}

static Janet cfun_xkb_rule_names(int32_t argc, Janet *argv)
{
    if (argc & 0x01) {
        janet_panicf("expected even number of arguments, got %d", argc);
    }

    struct xkb_rule_names *names = janet_abstract(&jxkb_at_xkb_rule_names, sizeof(*names));
    memset(names, 0, sizeof(*names));

    for (int32_t k = 0, v = 1; k < argc; k += 2, v += 2) {
        const uint8_t *kw = janet_getkeyword(argv, k);
        const char **member_p;

        if (!janet_cstrcmp(kw, "rules")) {
            member_p = &names->rules;
        } else if (!janet_cstrcmp(kw, "model")) {
            member_p = &names->model;
        } else if (!janet_cstrcmp(kw, "layout")) {
            member_p = &names->layout;
        } else if (!janet_cstrcmp(kw, "variant")) {
            member_p = &names->variant;
        } else if (!janet_cstrcmp(kw, "options")) {
            member_p = &names->options;
        } else {
            janet_panicf("unknown key: %v", argv[k]);
        }

        *member_p = jxkb_get_xkb_rule_names_member(argv, v);
    }

    return janet_wrap_abstract(names);
}


static const jl_key_def_t xkb_keymap_compile_flags_defs[] = {
    {"no-flags", XKB_KEYMAP_COMPILE_NO_FLAGS},
    {NULL, 0},
};

static const JanetAbstractType jxkb_at_xkb_keymap = {
    .name = MOD_NAME "/xkb-keymap",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static Janet cfun_xkb_keymap_new_from_names(int32_t argc, Janet *argv)
{
    struct xkb_context *context;
    const struct xkb_rule_names *names;
    enum xkb_keymap_compile_flags flags;

    struct xkb_keymap *keymap;

    janet_fixarity(argc, 3);

    context = jl_get_abs_obj_pointer(argv, 0, &jxkb_at_xkb_context);
    if (janet_checktype(argv[1], JANET_NIL)) {
        names = NULL;
    } else {
        names = janet_getabstract(argv, 1, &jxkb_at_xkb_rule_names);
    }
    flags = jl_get_key_flags(argv, 2, xkb_keymap_compile_flags_defs);

    keymap = xkb_keymap_new_from_names(context, names, flags);
    if (!keymap) {
        janet_panic("failed to create xkb keymap");
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj(keymap, &jxkb_at_xkb_keymap));
}


static JanetReg cfuns[] = {
    {
        "xkb-context-new", cfun_xkb_context_new,
        "(" MOD_NAME "/xkb-context-new flags)\n\n"
        "Creates a new xkb context."
    },
    {
        "xkb-keymap-new-from-names", cfun_xkb_keymap_new_from_names,
        "(" MOD_NAME "/xkb-keymap-new-from-names xkb-context xkb-rule-names xkb-keymap-compile-flags)\n\n"
        "Creates a new keymap from names."
    },
    {
        "xkb-rule-names", cfun_xkb_rule_names,
        "(" MOD_NAME "/xkb-rule-names ...)\n\n"
        "Creates a xkb_rule_names struct."
    },
    {NULL, NULL, NULL},
};


JANET_MODULE_ENTRY(JanetTable *env)
{
    janet_register_abstract_type(&jxkb_at_xkb_context);
    janet_register_abstract_type(&jxkb_at_xkb_keymap);

    janet_cfuns(env, MOD_NAME, cfuns);
}
