#ifndef __JL_H__
#define __JL_H__

#define jl_log(verb, fmt, ...) \
    wlr_log(verb, "[JL]" fmt, ##__VA_ARGS__)

#define jl_log_val(val, fmt) \
    jl_log(WLR_DEBUG, #val " = " fmt, (val))

static inline Janet jl_import(const char *name)
{
    Janet import_fn = janet_resolve_core("import*");
    if (!janet_checktype(import_fn, JANET_FUNCTION)) {
        janet_panic("core function import* not found");
    }
    Janet import_argv[] = {
        janet_cstringv(name),
    };
    return janet_call(janet_unwrap_function(import_fn), 1, import_argv);
}

static inline const JanetAbstractType *jl_get_abstract_type_by_name(const char *name)
{
    Janet at_name = janet_csymbolv(name);
    const JanetAbstractType *at = janet_get_abstract_type(at_name);
    if (!at) {
        janet_panicf("cant't find abstract type %s", name);
    }
    return at;
}

#endif
