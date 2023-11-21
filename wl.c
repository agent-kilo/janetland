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


static JanetReg cfuns[] = {
    {
        "wl-display-create", cfun_wl_display_create, 
        "(" MOD_NAME "/wl-display-create)\n\n" 
        "Creates a Wayland display object."
    },
    {NULL, NULL, NULL},
};


JANET_MODULE_ENTRY(JanetTable *env)
{
    janet_register_abstract_type(&jwl_at_wl_display);

    janet_cfuns(env, MOD_NAME, cfuns);
}
