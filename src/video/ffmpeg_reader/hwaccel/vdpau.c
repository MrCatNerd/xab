#include "logger.h"
#include "vdpau_vtable.h"

#include "hwaccels_apis.h"

#include <X11/Xlib.h>
#include <libavutil/buffer.h>
#include <stdlib.h>
#include <string.h>
#include <vdpau/vdpau.h>
#include <vdpau/vdpau_x11.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vdpau.h>

#include "vdpau_vtable.h"

typedef struct VdpauSharedHandle {
        _Atomic int refs;
        VdpDevice device;
        VdpGetProcAddress *get_proc_address;
        VdpauVTable_t *vtable;
} VdpauSharedHandle_t;

typedef struct VdpauHandle {
        VdpauSharedHandle_t *shandle;

        AVBufferRef *av_device_ref;

        // vdpau stuff
        unsigned int picture_width;
        unsigned int picture_height;
        VdpDecoder decoder;
        VdpVideoSurface video_surface;
        VdpOutputSurface output_surface;
} VdpauHandle_t;

static VdpauSharedHandle_t *RefSharedHandle(void) {
    static VdpauSharedHandle_t shandle = {0};
    shandle.refs++;
    return &shandle;
}

static void UnrefSharedHandle(VdpauSharedHandle_t *shandle) {
    if (!shandle)
        return;

    if (--shandle->refs <= 0) {
        shandle->vtable->device_destroy(shandle->device);
        free(shandle->vtable);
        memset(shandle, 0, sizeof(VdpauSharedHandle_t));
    }
}

int initSharedHandle(void) {
    static _Atomic bool initialized = false;
    if (initialized)
        return 0;

    xab_log(LOG_VERBOSE, "Initalized VDPAU shared handle...\n");
    VdpauSharedHandle_t *shandle = RefSharedHandle();

    // create device
    xab_log(LOG_DEBUG, "Creating VDPAU X11 device\n");
    Display *display = XOpenDisplay(NULL);
    if (!display)
        return -1;
    if (vdp_device_create_x11(display, DefaultScreen(display), &shandle->device,
                              &shandle->get_proc_address) != VDP_STATUS_OK) {
        // TODO: log status error instead (see VdpStatus)
        return -1;
    }
    XCloseDisplay(display);

    // load vtable
    xab_log(LOG_VERBOSE, "Allocating VDPAU vtable memory\n");
    shandle->vtable = calloc(1, sizeof(VdpauVTable_t));
    {
        int status = vdpau_vtable_init(shandle->vtable, shandle->device,
                                       shandle->get_proc_address);
        if (status < 0)
            return status;
    }

    // abuse ref system
    shandle->refs--;

    initialized = true;
    xab_log(LOG_VERBOSE, "VDPAU shared handle is initialized!\n");
    return 0;
}

int init_vdpau(HwaAPIHandle *handle) {
    if (!handle)
        return -1;

    {
        const int status = initSharedHandle();
        if (status < 0)
            return status;
    }

    // allocate memory
    VdpauHandle_t *vdp_handle = calloc(1, sizeof(VdpauHandle_t));
    if (!vdp_handle)
        return -1;
    vdp_handle->shandle = RefSharedHandle();
    *handle = vdp_handle;

    // set every vdpau thingy to uninitialized
    vdp_handle->decoder = VDP_INVALID_HANDLE;
    vdp_handle->video_surface = VDP_INVALID_HANDLE;
    vdp_handle->output_surface = VDP_INVALID_HANDLE;
    vdp_handle->picture_width = 0;
    vdp_handle->picture_height = 0;

    // create video output surface
    // xab_log(LOG_VERBOSE, "Creating video output surface\n");
    // vdp_handle->shandle->vtable->output_surface_create(
    //     vdp_handle->shandle->device, VDP_RGBA_FORMAT_B8G8R8A8, 1, 1,
    //     &vdp_handle->output_surface);

    return 0;
}

static void free_device_ref(struct AVHWDeviceContext *hwctx) {
    HwaAPIHandle *handle = (void *)hwctx->user_opaque;
    if (!handle) {
        xab_log(LOG_ERROR, "VDPAU free device ref user_opaque is NULL!\n");
        return;
    }
    if (exit_vdpau(handle) < 0) {
        xab_log(LOG_WARN, "exit_vdpau failed!\n");
    }
}

create_device_vdpau_t create_device_vdpau(HwaAPIHandle handle) {
    xab_log(LOG_VERBOSE, "Creating VDPAU AVHWDeviceContext\n");
    create_device_vdpau_t ret = {
        .err = -1,
        .hwctx = NULL,
    };

    VdpauHandle_t *vdp_handle = handle;

    xab_log(LOG_TRACE,
            "Allocating AVHWDeviceContext via av_hwdevice_ctx_alloc\n");
    AVBufferRef *avref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VDPAU);
    if (!avref) {
        if (exit_vdpau(handle) < 0) {
            xab_log(LOG_WARN, "exit_vdpau failed!\n");
        }
        return ret;
    }

    vdp_handle->av_device_ref = avref;

    xab_log(LOG_TRACE, "Setting AVHWDeviceContext's free device callback\n");
    AVHWDeviceContext *hwctx = (void *)avref;
    hwctx->free = free_device_ref;
    hwctx->user_opaque = handle;

    xab_log(LOG_TRACE, "Setting AVHWDeviceContext's VDPAU context\n");
    AVVDPAUDeviceContext *vdpauctx = hwctx->hwctx;
    if (!vdpauctx) {
        xab_log(LOG_ERROR, "AVHWDeviceContext's VDPAU context is NULL!\n");
        if (exit_vdpau(handle) < 0) {
            xab_log(LOG_WARN, "exit_vdpau failed!\n");
        }
        return ret;
    }
    vdpauctx->device = vdp_handle->shandle->device;
    vdpauctx->get_proc_address = vdp_handle->shandle->get_proc_address;

    xab_log(LOG_TRACE,
            "Initializing AVHWDeviceContext via av_hwdevice_ctx_init\n");
    if (av_hwdevice_ctx_init(avref) < 0) {
        if (exit_vdpau(handle) < 0) {
            xab_log(LOG_WARN, "exit_vdpau failed!\n");
        }
        return ret;
    }

    ret.hwctx = hwctx;
    ret.err = 0;
    xab_log(LOG_DEBUG, "VDPAU device created successfully!\n");
    return ret;
}

int exit_vdpau(HwaAPIHandle handle) {
    if (!handle)
        return -1;
    VdpauHandle_t *vdp_handle = handle;

    if (vdp_handle->output_surface != VDP_INVALID_HANDLE)
        vdp_handle->shandle->vtable->output_surface_destroy(
            vdp_handle->output_surface);
    av_buffer_unref(&vdp_handle->av_device_ref);

    UnrefSharedHandle(vdp_handle->shandle);
    free(vdp_handle);
    return 0;
}
