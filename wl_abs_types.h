#ifndef __WL_ABS_TYPES_H__
#define __WL_ABS_TYPES_H__

#include <wayland-server-core.h>

#ifndef MOD_NAME
#define MOD_NAME WL_MOD_NAME
#endif


static const jl_key_def_t wl_event_defs[] = {
    {"readable", WL_EVENT_READABLE},
    {"writable", WL_EVENT_WRITABLE},
    {"hangup", WL_EVENT_HANGUP},
    {"error", WL_EVENT_ERROR},
    {NULL, 0},
};


static const JanetAbstractType jwl_at_wl_event_loop = {
    .name = MOD_NAME "/wl-event-loop",
    JANET_ATEND_NAME
};


static int method_event_source_gcmark(void *p, size_t len);
static const JanetAbstractType jwl_at_event_source = {
    .name = MOD_NAME "/event-source",
    .gc = NULL,
    .gcmark = method_event_source_gcmark,
    JANET_ATEND_GCMARK
};


static const JanetAbstractType jwl_at_wl_list = {
    .name = MOD_NAME "/wl-list",
    JANET_ATEND_NAME
};


static const JanetAbstractType jwl_at_wl_signal = {
    .name = MOD_NAME "/wl-signal",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static const JanetAbstractType jwl_at_listener = {
    .name = MOD_NAME "/listener",
    .gc = NULL,
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};


static const JanetAbstractType jwl_at_wl_display = {
    .name = MOD_NAME "/wl-display",
    .gc = NULL, /* TODO: close the display? */
    .gcmark = NULL,
    JANET_ATEND_GCMARK
};

#endif
