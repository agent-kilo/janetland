#ifndef __WL_ABS_TYPES_H__
#define __WL_ABS_TYPES_H__

#include <wayland-server-core.h>

#ifndef MOD_NAME
#define MOD_NAME WL_MOD_NAME
#endif

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
