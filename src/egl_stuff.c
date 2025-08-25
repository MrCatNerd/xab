#include "pch.h"

#include "utils.h"
#include "logger.h"
#include "egl_stuff.h"

const char *get_EGL_error_string(EGLint error) {
    switch (error) {
    case EGL_SUCCESS:
        return "Success";
    case EGL_NOT_INITIALIZED:
        return "EGL is not or could not be initialized";
    case EGL_BAD_ACCESS:
        return "EGL cannot access a requested resource";
    case EGL_BAD_ALLOC:
        return "EGL failed to allocate resources for the requested operation";
    case EGL_BAD_ATTRIBUTE:
        return "An unrecognized attribute or attribute value was passed in the "
               "attribute list";
    case EGL_BAD_CONTEXT:
        return "An EGLContext argument does not name a valid EGL rendering "
               "context";
    case EGL_BAD_CONFIG:
        return "An EGLConfig argument does not name a valid EGL frame buffer "
               "configuration";
    case EGL_BAD_CURRENT_SURFACE:
        return "The current surface of the calling thread is a window, pixel "
               "buffer or pixmap that is no longer valid";
    case EGL_BAD_DISPLAY:
        return "An EGLDisplay argument does not name a valid EGL display "
               "connection";
    case EGL_BAD_SURFACE:
        return "An EGLSurface argument does not name a valid surface "
               "configured for GL rendering";
    case EGL_BAD_MATCH:
        return "Arguments are inconsistent";
    case EGL_BAD_PARAMETER:
        return "One or more argument values are invalid";
    case EGL_BAD_NATIVE_PIXMAP:
        return "A NativePixmapType argument does not refer to a valid native "
               "pixmap";
    case EGL_BAD_NATIVE_WINDOW:
        return "A NativeWindowType argument does not refer to a valid native "
               "window";
    case EGL_CONTEXT_LOST:
        return "The application must destroy all contexts and reinitialise";
    default:
        return "ERROR: UNKNOWN EGL ERROR";
    }
}

void pretty_print_egl_check(int do_assert_on_failure, const char *message) {
    int error = eglGetError();
#ifndef NLOG_DEBUG
    xab_log(LOG_DEBUG, "%s: %s\n", message, get_EGL_error_string(error));
#endif
    if (do_assert_on_failure) {
        Assert(error == EGL_SUCCESS);
    }
}
