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

static inline const JanetAbstractType *jl_get_abstract_type_by_key(Janet key)
{
    const JanetAbstractType *at = janet_get_abstract_type(key);
    if (!at) {
        janet_panicf("cant't find abstract type %v", key);
    }
    return at;
}

static inline const JanetAbstractType *jl_get_abstract_type_by_name(const char *name)
{
    Janet at_key = janet_csymbolv(name);
    return jl_get_abstract_type_by_key(at_key);
}

static inline void **jl_pointer_to_abs_obj(void *ptr, const JanetAbstractType *at)
{
    void **ptr_p = janet_abstract(at, sizeof(ptr));
    *ptr_p = ptr;
    return ptr_p;
}

static inline void **jl_pointer_to_abs_obj_by_key(void *ptr, Janet key)
{
    const JanetAbstractType *at = jl_get_abstract_type_by_key(key);
    return jl_pointer_to_abs_obj(ptr, at);
}

static inline void **jl_pointer_to_abs_obj_by_name(void *ptr, const char *name)
{
    const JanetAbstractType *at = jl_get_abstract_type_by_name(name);
    return jl_pointer_to_abs_obj(ptr, at);
}

static inline void *jl_get_abs_obj_pointer(const Janet *argv, int32_t n, const JanetAbstractType *at)
{
    void **ptr_p = janet_getabstract(argv, n, at);
    return *ptr_p;
}

static inline void *jl_get_abs_obj_pointer_by_key(const Janet *argv, int32_t n, Janet key)
{
    const JanetAbstractType *at = jl_get_abstract_type_by_key(key);
    return jl_get_abs_obj_pointer(argv, n, at);
}

static inline void *jl_get_abs_obj_pointer_by_name(const Janet *argv, int32_t n, const char *name)
{
    const JanetAbstractType *at = jl_get_abstract_type_by_name(name);
    return jl_get_abs_obj_pointer(argv, n, at);
}

static inline void *jl_value_to_data_pointer(Janet value)
{
    /* XXX: Should be able to set the data pointer to refer to arbitrary Janet types */
    switch (janet_type(value)) {
    case JANET_NIL:
        return NULL;
    case JANET_POINTER:
        return janet_unwrap_pointer(value);
    case JANET_ABSTRACT: {
        void **data_p = janet_unwrap_abstract(value);
        return *data_p;
    }
    default:
        janet_panicf("expected abstract type or a pointer, got %v", value);
    }
}

#endif
