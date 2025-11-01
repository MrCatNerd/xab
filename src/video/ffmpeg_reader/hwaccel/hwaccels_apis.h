#pragma once

#include <libavutil/hwcontext.h>

typedef void *HwaAPIHandle;

extern int init_vdpau(HwaAPIHandle *handle);
typedef struct {
        int err;
        AVBufferRef *avref;
} create_device_vdpau_t;
extern create_device_vdpau_t create_device_vdpau(HwaAPIHandle handle);
extern int exit_vdpau(HwaAPIHandle handle);
