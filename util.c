#include <janet.h>

#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "jl.h"
#include "types.h"

#define MOD_NAME "util"
#define WL_MOD_NAME "wl"
#define WL_MOD_FULL_NAME "janetland/wl"
#define WLR_MOD_NAME "wlr"
#define WLR_MOD_FULL_NAME "janetland/wlr"


static Janet cfun_get_listener_data(int32_t argc, Janet *argv)
{
    void *data_p;

    janet_fixarity(argc, 1);

    data_p = janet_getpointer(argv, 0);
    if (!data_p) {
        return janet_wrap_nil();
    }

    return *(Janet *)data_p;
}


/* XXX: This function relies on the fact that most of wl & wlr abstract types are simple
   wrappers around a pointer to the actual data structures managed by wl/wlr. If your
   abstract types come from somewhere else, don't use this function. */
static Janet cfun_get_abstract_listener_data(int32_t argc, Janet *argv)
{
    void *data_p;

    janet_fixarity(argc, 2);

    data_p = janet_getpointer(argv, 0);
    if (!data_p) {
        return janet_wrap_nil();
    }
    return janet_wrap_abstract(jl_pointer_to_abs_obj_by_key(data_p, argv[1]));
}


static const jl_offset_def_t link_offsets[] =
{
    {WLR_MOD_NAME "/wlr-output-mode", offsetof(struct wlr_output_mode, link)},
    {WLR_MOD_NAME "/wlr-output-cursor", offsetof(struct wlr_output_cursor, link)},
    {WLR_MOD_NAME "/wlr-output-layout-output", offsetof(struct wlr_output_layout_output , link)},
    {WLR_MOD_NAME "/wlr-xdg-surface", offsetof(struct wlr_xdg_surface , link)},
    {WLR_MOD_NAME "/wlr-xdg-popup", offsetof(struct wlr_xdg_popup , link)},
    {NULL, 0},
};

static Janet cfun_wl_list_to_array(int32_t argc, Janet *argv)
{
    struct wl_list *list;
    const uint8_t *element_at_name;

    const JanetAbstractType *element_at;
    uint64_t link_offset = (uint64_t)-1;
    JanetArray *arr;

    janet_fixarity(argc, 2);

    list = jl_get_abs_obj_pointer_by_name(argv, 0, WL_MOD_NAME "/wl-list");
    element_at_name = janet_getsymbol(argv, 1);

    element_at = jl_get_abstract_type_by_key(argv[1]);
    for (int i = 0; NULL != link_offsets[i].name; i++) {
        if (!janet_cstrcmp(element_at_name, link_offsets[i].name)) {
            link_offset = link_offsets[i].offset;
        }
    }
    if (((uint64_t)-1) == link_offset) {
        janet_panicf("unknown abstract type for calculating linked list offset: %v", argv[1]);
    }

    arr = janet_array(0);
    for (void *p = ((void *)(list->next)) - link_offset;
         p + link_offset != (void *)list;
         p = ((void *)(((struct wl_list *)(p + link_offset))->next)) - link_offset) {
        janet_array_push(arr, janet_wrap_abstract(jl_pointer_to_abs_obj(p, element_at)));
    }

    return janet_wrap_array(arr);
}


static Janet cfun_pointer_to_abstract_object(int32_t argc, Janet *argv)
{
    return cfun_get_abstract_listener_data(argc, argv);
}


static JanetReg cfuns[] = {
    {
        "get-listener-data", cfun_get_listener_data,
        "(" MOD_NAME "/get-listener-data data)\n\n"
        "Converts a raw data pointer to Janet built-in data types."
    },
    {
        "get-abstract-listener-data", cfun_get_abstract_listener_data,
        "(" MOD_NAME "/get-abstract-listener-data data abstract-type)\n\n"
        "Converts a raw data pointer to Janet abstract data types."
    },
    {
        "wl-list-to-array", cfun_wl_list_to_array,
        "(" MOD_NAME "/wl-list-to-array wl-list element-abstract-type)\n\n"
        "Converts a wl-list to a Janet array containing elements of element-abstract-type."
    },
    {
        "pointer-to-abstract-object", cfun_pointer_to_abstract_object,
        "(" MOD_NAME "/pointer-to-abstract-object pointer abs-type)\n\n"
        "Converts a raw pointer to an object of type abs-type."
    },
};


JANET_MODULE_ENTRY(JanetTable *env)
{
    /* Import these module first, so that we can find the abstract types */
    jl_import(WL_MOD_FULL_NAME);
    jl_import(WLR_MOD_FULL_NAME);

    janet_cfuns(env, MOD_NAME, cfuns);

    janet_def(env, "NULL", janet_wrap_pointer(NULL), "Value for comparing pointers.");
}
