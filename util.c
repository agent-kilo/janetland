#include <janet.h>

#include "jl.h"

#define MOD_NAME "util"


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
    JanetSymbol name;

    const JanetAbstractType *at;
    void **abs_p;

    janet_fixarity(argc, 2);

    data_p = janet_getpointer(argv, 0);
    if (!data_p) {
        return janet_wrap_nil();
    }
    name = janet_getsymbol(argv, 1);

    at = jl_get_abstract_type_by_name((const char *)name);
    abs_p = janet_abstract(at, sizeof(data_p));
    *abs_p = data_p;

    return janet_wrap_abstract(abs_p);
}


static JanetReg cfuns[] = {
    {
        "get-listener-data", cfun_get_listener_data,
        "(" MOD_NAME "/get-listener-data data)\n\n"
        "Converts a raw data pointer to Janet built-in data types."
    },
    {
        "get-abstract-listener-data", cfun_get_abstract_listener_data,
        "(" MOD_NAME "/get-abstract-listener-data data)\n\n"
        "Converts a raw data pointer to Janet abstract data types."
    },
};


JANET_MODULE_ENTRY(JanetTable *env)
{
    janet_cfuns(env, MOD_NAME, cfuns);

    janet_def(env, "NULL", janet_wrap_pointer(NULL), "Value for comparing pointers.");
}
