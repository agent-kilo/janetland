#include <time.h>
#include <errno.h>

#include <janet.h>

#include <wlr/xwayland.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>

#include "jl.h"
#include "types.h"


#ifndef MOD_NAME
#define MOD_NAME UTIL_MOD_NAME
#endif


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


#define JUTIL_LINK_OFFSET_DEF(at_name, struct_type, member) \
    {at_name, #member, offsetof(struct_type, member)}

static const jl_link_offset_def_t link_offsets[] =
{
    JUTIL_LINK_OFFSET_DEF(WLR_MOD_NAME "/wlr-output-mode", struct wlr_output_mode, link),
    JUTIL_LINK_OFFSET_DEF(WLR_MOD_NAME "/wlr-output-cursor", struct wlr_output_cursor, link),
    JUTIL_LINK_OFFSET_DEF(WLR_MOD_NAME "/wlr-output-layout-output", struct wlr_output_layout_output, link),
    JUTIL_LINK_OFFSET_DEF(WLR_MOD_NAME "/wlr-xdg-surface", struct wlr_xdg_surface, link),
    JUTIL_LINK_OFFSET_DEF(WLR_MOD_NAME "/wlr-xdg-popup", struct wlr_xdg_popup, link),
    JUTIL_LINK_OFFSET_DEF(WLR_MOD_NAME "/wlr-scene-node", struct wlr_scene_node, link),
    JUTIL_LINK_OFFSET_DEF(WLR_MOD_NAME "/wlr-xwayland-surface", struct wlr_xwayland_surface, parent_link),
    {NULL, NULL, 0},
};

static Janet cfun_wl_list_to_array(int32_t argc, Janet *argv)
{
    struct wl_list *list;
    const uint8_t *element_at_name;
    const uint8_t *element_link_member;

    const JanetAbstractType *element_at;
    uint64_t link_offset = (uint64_t)-1;
    JanetArray *arr;

    janet_fixarity(argc, 3);

    list = jl_get_abs_obj_pointer_by_name(argv, 0, WL_MOD_NAME "/wl-list");
    element_at_name = janet_getsymbol(argv, 1);
    element_link_member = janet_getkeyword(argv, 2);

    element_at = jl_get_abstract_type_by_key(argv[1]);
    for (int i = 0; NULL != link_offsets[i].type; i++) {
        if (!janet_cstrcmp(element_at_name, link_offsets[i].type) &&
            !janet_cstrcmp(element_link_member, link_offsets[i].member)) {
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


static Janet cfun_pointer_to_table(int32_t argc, Janet *argv)
{
    void *data;

    janet_fixarity(argc, 1);

    data = janet_getpointer(argv, 0);
    if (!data) {
        return janet_wrap_nil();
    }
    return janet_wrap_table((JanetTable *)data);
}


static const jl_key_def_t clock_id_defs[] = {
    {"realtime", CLOCK_REALTIME},
    {"monotonic", CLOCK_MONOTONIC},
    {"process-cputime-id", CLOCK_PROCESS_CPUTIME_ID},
    {"thread-cputime-id", CLOCK_THREAD_CPUTIME_ID},
    {NULL, 0},
};

static const JanetAbstractType jutil_at_timespec = {
    .name = MOD_NAME "/timespec",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

static clockid_t jutil_get_clock_id(const Janet *argv, int32_t n)
{
    const uint8_t *kw = janet_getkeyword(argv, n);
    for (int i = 0; clock_id_defs[i].name; i++) {
        if (!janet_cstrcmp(kw, clock_id_defs[i].name)) {
            return clock_id_defs[i].key;
        }
    }
    janet_panicf("unknown clock id: %v", argv[n]);
}

static Janet cfun_clock_gettime(int32_t argc, Janet *argv)
{
    clockid_t clk_id;
    struct timespec *tspec;

    janet_fixarity(argc, 1);

    clk_id = jutil_get_clock_id(argv, 0);
    tspec = janet_abstract(&jutil_at_timespec, sizeof(*tspec));
    if (clock_gettime(clk_id, tspec) < 0) {
        janet_panicf("failed to get time with clock id %v: %d", argv[0], errno);
    }
    return janet_wrap_abstract(tspec);
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
    {
        "pointer-to-table", cfun_pointer_to_table,
        "(" MOD_NAME "/pointer-to-table pointer)\n\n"
        "Converts a raw pointer to a Janet table."
    },
    {
        "clock-gettime", cfun_clock_gettime,
        "(" MOD_NAME "/clock-gettime clock-id)\n\n"
        "Calls the clock_gettime C function."
    },
};


JANET_MODULE_ENTRY(JanetTable *env)
{
    /* Import these module first, so that we can find the abstract types */
    jl_import(WL_MOD_FULL_NAME);
    jl_import(WLR_MOD_FULL_NAME);

    janet_register_abstract_type(&jutil_at_timespec);

    janet_cfuns(env, MOD_NAME, cfuns);

    janet_def(env, "NULL", janet_wrap_pointer(NULL), "Value for comparing pointers.");
}
