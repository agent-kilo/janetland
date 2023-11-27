#ifndef __TYPES_H__
#define __TYPES_H__

typedef struct {
    const char *name;
    int key;
} jl_key_def_t;

static inline int jl_get_key_def(const Janet *argv, int32_t n, const jl_key_def_t *def_table)
{
    const uint8_t *kw = janet_getkeyword(argv, n);
    for (int i = 0; NULL != def_table[i].name; i++) {
        if (!janet_cstrcmp(kw, def_table[i].name)) {
            return def_table[i].key;
        }
    }
    janet_panicf("unknown key: %v", argv[n]);
}

typedef struct {
    const char *name;
    uint64_t offset;
} jl_offset_def_t;

#endif
