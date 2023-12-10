#ifndef __TYPES_H__
#define __TYPES_H__

typedef struct {
    const char *name;
    int32_t key;
} jl_key_def_t;

#define JL_KEY_DEF_COUNT(defs_var) \
    ((sizeof(defs_var) / sizeof(jl_key_def_t)) - 1)

static inline int32_t jl_get_key_def(const Janet *argv, int32_t n, const jl_key_def_t *def_table)
{
    const uint8_t *kw = janet_getkeyword(argv, n);
    for (int i = 0; NULL != def_table[i].name; i++) {
        if (!janet_cstrcmp(kw, def_table[i].name)) {
            return def_table[i].key;
        }
    }
    janet_panicf("unknown key: %v", argv[n]);
}

static inline uint32_t jl_get_key_flags(const Janet *argv, int32_t n, const jl_key_def_t *def_table)
{
    uint32_t flags = 0;

    switch (janet_type(argv[n])) {
    case JANET_KEYWORD:
        flags = (uint32_t)jl_get_key_def(argv, n, def_table);
        break;
    case JANET_TUPLE:
    case JANET_ARRAY: {
        JanetView flags_view = janet_getindexed(argv, n);
        for (int32_t i = 0; i < flags_view.len; i++) {
            if (!janet_checktype(flags_view.items[i], JANET_KEYWORD)) {
                janet_panicf("expected keyword flags, got %v", flags_view.items[i]);
            }
            flags |= (uint32_t)jl_get_key_def(flags_view.items, i, def_table);
        }
        break;
    }
    default:
        janet_panicf("bad slot #%d: expected keyword or sequence of keywords, got %v",
                     n, argv[n]);
    }

    return flags;
}

static inline JanetArray *jl_get_flag_keys(uint32_t flags, const jl_key_def_t *def_table)
{
    JanetArray *keys_arr = janet_array(0);
    for (int i = 0; NULL != def_table[i].name; i++) {
        if (flags & (uint32_t)(def_table[i].key)) {
            janet_array_push(keys_arr, janet_ckeywordv(def_table[i].name));
        }
    }
    return keys_arr;
}

typedef struct {
    const char *name;
    uint64_t offset;
} jl_offset_def_t;

typedef struct {
    const char *type;
    const char *member;
    uint64_t offset;
} jl_link_offset_def_t;

#endif
