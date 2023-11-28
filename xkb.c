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
    names = NULL; /* TODO */
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
    {NULL, NULL, NULL},
};


JANET_MODULE_ENTRY(JanetTable *env)
{
    janet_register_abstract_type(&jxkb_at_xkb_context);
    janet_register_abstract_type(&jxkb_at_xkb_keymap);

    janet_cfuns(env, MOD_NAME, cfuns);
}
