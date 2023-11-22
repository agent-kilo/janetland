#ifndef __TYPES_H__
#define __TYPES_H__

typedef struct {
    const char *name;
    int key;
} jl_key_def_t;

typedef struct {
    const char *name;
    uint64_t offset;
} jl_offset_def_t;

#endif
