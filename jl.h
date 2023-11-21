#ifndef __JL_H__
#define __JL_H__

#define jl_log(verb, fmt, ...) \
    wlr_log(verb, "[JL]" fmt, ##__VA_ARGS__)

#define jl_log_val(val, fmt) \
    jl_log(WLR_DEBUG, #val " = " fmt, (val))

#endif
