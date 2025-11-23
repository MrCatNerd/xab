#pragma once

#include <libavutil/hwcontext.h>
#include "video/ffmpeg_reader/hwaccel/hwaccelslist.h"

typedef void *HwaAPIHandle;

#define HWACCEL_API_DEF(lower_name, _)                                         \
    extern int init_##lower_name(HwaAPIHandle *handle);                        \
    typedef struct {                                                           \
            int err;                                                           \
            AVBufferRef *avref;                                                \
    } create_device_##lower_name##_t;                                          \
    extern create_device_##lower_name##_t create_device_##lower_name(          \
        HwaAPIHandle handle);                                                  \
    extern int exit_##lower_name(HwaAPIHandle handle);

HWACCELSXMACRO(HWACCEL_API_DEF)
#undef HWACCEL_API_DEF
