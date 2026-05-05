#include "logger.h"
#include "vdpau_vtable.h"

#include "hwaccels_apis.h"

#include <X11/Xlib.h>
#include <libavutil/buffer.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <vdpau/vdpau.h>
#include <vdpau/vdpau_x11.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_vdpau.h>

#include "vdpau_vtable.h"
#include "utils.h"

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
        xab_log(LOG_VERBOSE,
                "Shared module has %d refs left, uninitializing...\n",
                shandle->refs);
        if (shandle->vtable) {
            if (shandle->vtable->device_destroy && shandle->device)
                shandle->vtable->device_destroy(shandle->device);
            free(shandle->vtable);
        }
        memset(shandle, 0, sizeof(VdpauSharedHandle_t));
    }
}

static int x_error_handler(Display *dpy, XErrorEvent *pErr) {
    char *err_code = NULL;
    switch (pErr->error_code) {
    default:
        err_code = "unknown error code";
        break;
    case Success:
        err_code = "everything's okay";
        break;
    case BadRequest:
        err_code = "bad request code";
        break;
    case BadValue:
        err_code = "int parameter out of range";
        break;
    case BadWindow:
        err_code = "parameter not a Window";
        break;
    case BadPixmap:
        err_code = "parameter not a Pixmap";
        break;
    case BadAtom:
        err_code = "parameter not an Atom";
        break;
    case BadCursor:
        err_code = "parameter not a Cursor";
        break;
    case BadFont:
        err_code = "parameter not a Font";
        break;
    case BadMatch:
        err_code = "parameter mismatch";
        break;
    case BadDrawable:
        err_code = "parameter not a Pixmap or Window";
        break;
    case BadAccess:
        err_code = "depending on context:\n"
                   "\t- key/button already grabbed\n"
                   "\t- attempt to free an illegal cmap entry\n"
                   "\t- attempt to store into a read-only color map entry\n"
                   "\t- attempt to modify the access control list from other "
                   "than the local host";
        break;
    case BadAlloc:
        err_code = "insufficient resources";
        break;
    case BadColor:
        err_code = "no such colormap";
        break;
    case BadGC:
        err_code = "parameter not a GC";
        break;
    case BadIDChoice:
        err_code = "choice not in range or already used";
        break;
    case BadName:
        err_code = "font or color name doesn't exist";
        break;
    case BadLength:
        err_code = "Request length incorrect";
        break;
    case BadImplementation:
        err_code = "server is defective";
        break;
    }

    // X Error of failed request:  BadAccess (attempt to access private resource
    // denied) Major opcode of failed request:  152 (GLX) Minor opcode of failed
    // request:  5 (X_GLXMakeCurrent) Serial number of failed request:  28
    // Current serial number in output stream:  28
    xab_log(LOG_ERROR,
            "X Error Handler called, values:\n- type: %d\n- serial: %lu\n- "
            "error code: `%s` (%d)\n- request code: %d\n- minor code: %d\n",
            pErr->type, pErr->serial, err_code, pErr->error_code,
            pErr->request_code, pErr->minor_code);

    Assert(err_code != NULL && "err_code string ptr is NULL!");

    if (pErr->request_code == 33) { // 33 (X_GrabKey)
        if (pErr->error_code == BadAccess) {
            xab_log(LOG_ERROR,
                    "ERROR: A client attempts to grab a key/button combination "
                    "already\n"
                    "        grabbed by another client. Ignoring.\n");
            return 0;
        }
    }
    return 0;
}

static int initSharedHandle(void) {
    static _Atomic bool initialized = false;
    if (initialized)
        return 0;

    xab_log(LOG_VERBOSE, "Initalizing VDPAU shared handle...\n");
    VdpauSharedHandle_t *shandle = RefSharedHandle();

    // create device
    xab_log(LOG_TRACE, "Connecting to the X server (Xlib)\n");
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        xab_log(LOG_ERROR, "Failed to connect to the X server via Xlib\n");
        return -1;
    }

    xab_log(LOG_TRACE, "Setting Xlib error handler\n");
    XSetErrorHandler(x_error_handler);

    xab_log(LOG_DEBUG, "Creating VDPAU X11 device\n");
    if (vdp_device_create_x11(display, DefaultScreen(display), &shandle->device,
                              &shandle->get_proc_address) != VDP_STATUS_OK) {
        xab_log(LOG_ERROR, "Failed to create an X11 VDPAU device\n");
        return -1;
    }
    xab_log(LOG_TRACE, "Disconnecting from the X server (Xlib)\n");
    XCloseDisplay(display);

    // load vtable
    xab_log(LOG_VERBOSE, "Allocating VDPAU vtable memory\n");
    shandle->vtable = calloc(1, sizeof(VdpauVTable_t));
    {
        const int status = vdpau_vtable_init(shandle->vtable, shandle->device,
                                             shandle->get_proc_address);
        if (status < 0) {

            xab_log(LOG_ERROR,
                    "VDPAU vtable Initialization returned %d, freeing vtable "
                    "memory\n",
                    status);
            free(shandle->vtable);
            shandle->vtable = NULL;
            return status;
        } else if (!shandle->vtable->is_initialized) {
            xab_log(LOG_ERROR, "VDPAU shared handle's vtable isn't initialized "
                               "even though init was called! freeing vtable\n");
            free(shandle->vtable);
            shandle->vtable = NULL;
            return -1;
        }
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
    xab_log(LOG_VERBOSE, "Creating video output surface\n");
    VdpStatus status = vdp_handle->shandle->vtable->output_surface_create(
        vdp_handle->shandle->device, VDP_RGBA_FORMAT_B8G8R8A8, 1, 1,
        &vdp_handle->output_surface);
    if (status != VDP_STATUS_OK) {
        xab_log(LOG_ERROR, "Couldn't create VDPAU output surface: %s\n",
                vdp_handle->shandle->vtable->get_error_string(status));
        return -1;
    }

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
        .avref = NULL,
    };

    VdpauHandle_t *vdp_handle = handle;

    xab_log(LOG_TRACE,
            "Allocating AVHWDeviceContext via av_hwdevice_ctx_alloc\n");
    AVBufferRef *avref = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VDPAU);
    if (!avref) {
        xab_log(LOG_ERROR, "Failed to allocate VDPAU AVHWDeviceContext!\n");
        if (exit_vdpau(handle) < 0) {
            xab_log(LOG_WARN, "exit_vdpau failed!\n");
        }
        return ret;
    }

    vdp_handle->av_device_ref = avref;

    xab_log(LOG_TRACE, "Setting AVHWDeviceContext's free device callback\n");
    AVHWDeviceContext *hwctx = (void *)avref->data;
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

    ret.avref = avref;
    ret.err = 0;
    xab_log(LOG_DEBUG, "VDPAU device created successfully!\n");
    return ret;
}

int exit_vdpau(HwaAPIHandle handle) {
    if (!handle)
        return -1;
    xab_log(LOG_VERBOSE, "Exiting a VDPAU hwaccel instance...\n");

    VdpauHandle_t *vdp_handle = handle;

    if (vdp_handle->output_surface != VDP_INVALID_HANDLE) {
        xab_log(LOG_TRACE, "Destroying VDPAU output surface\n");
        vdp_handle->shandle->vtable->output_surface_destroy(
            vdp_handle->output_surface);
    }

    if (vdp_handle->av_device_ref) {
        xab_log(LOG_TRACE, "Unrefing VDPAU device ref\n");
        av_buffer_unref(&vdp_handle->av_device_ref);
    }

    xab_log(LOG_TRACE, "Unrefing VPDAU shared handle\n");
    UnrefSharedHandle(vdp_handle->shandle);

    xab_log(LOG_TRACE, "Freeing the VDPAU hwaccel instance's handle\n");
    free(vdp_handle);

    return 0;
}
