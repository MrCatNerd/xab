#include "pch.h"

#include "utils.h"
#include "logger.h"
#include "egl_stuff.h"

void clear_error(void) {
    while (glGetError() != GL_NO_ERROR)
        ;
}

GLenum glCheckError_(const char *file, int line) {
    GLenum errorCode;
    static unsigned long errorCount = 0;

    char *error = NULL;
    while ((errorCode = glGetError()) != GL_NO_ERROR) {
        switch (errorCode) {
        case GL_INVALID_ENUM:
            error = "INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            error = "INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            error = "INVALID_OPERATION";
            break;
        case GL_STACK_OVERFLOW:
            error = "STACK_OVERFLOW";
            break;
        case GL_STACK_UNDERFLOW:
            error = "STACK_UNDERFLOW";
            break;
        case GL_OUT_OF_MEMORY:
            error = "OUT_OF_MEMORY";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            error = "INVALID_FRAMEBUFFER_OPERATION";
            break;
        default:
            error = "Unrecognized error code";
            break;
        }

        errorCount++;
        if (errorCount <= 10)
            xab_log(LOG_DEBUG,
                    "[OpenGL Error] %s | %s:%d (error count: %lu | error "
                    "code:%d)\n",
                    error, file, line, errorCount, errorCode);
    }
    return errorCode;
}

void pretty_print_egl_check(int do_assert_on_failure, const char *message) {
    int error = eglGetError();
#ifndef NLOG_DEBUG
    const char *egl_success_string =
        "The last function succeeded without error.";
    const char *egl_not_initialized_string =
        "EGL is not initialized, or could not be initialized, for the "
        "specified EGL display connection. ";
    const char *egl_bad_access_string =
        "EGL cannot access a requested resource (for example a context is "
        "bound in another thread). ";
    const char *egl_bad_alloc_string =
        "EGL failed to allocate resources for the requested operation.";
    const char *egl_bad_attribute_string =
        "An unrecognized attribute or attribute value was passed in the "
        "attribute list. ";
    const char *egl_bad_context_string =
        "An EGLContext argument does not name a valid EGL rendering context. ";
    const char *egl_bad_config_string =
        "An EGLConfig argument does not name a valid EGL frame buffer "
        "configuration. ";
    const char *egl_bad_current_surface_string =
        "The current surface of the calling thread is a window, pixel buffer "
        "or pixmap that is no longer valid. ";
    const char *egl_bad_display_string =
        "An EGLDisplay argument does not name a valid EGL display connection.";
    const char *egl_bad_surface_string =
        "An EGLSurface argument does not name a valid surface (window, pixel "
        "buffer or pixmap) configured for GL rendering. ";
    const char *egl_bad_match_string =
        "Arguments are inconsistent (for example, a valid context requires "
        "buffers not supplied by a valid surface). ";
    const char *egl_bad_parameter_string =
        "One or more argument values are invalid.";
    const char *egl_bad_native_pixmap_string =
        "A NativePixmapType argument does not refer to a valid native pixmap. ";
    const char *egl_bad_native_window_string =
        "A NativeWindowType argument does not refer to a valid native window.";
    const char *egl_context_lost_string =
        "A power management event has occurred. The application must destroy "
        "all contexts and reinitialise OpenGL ES state and objects to continue "
        "rendering. ";

    switch (error) { // there is a better way to do this but idc i just copied
                     // it from the internet lol
    case EGL_SUCCESS:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_success_string);
        break;
    case EGL_NOT_INITIALIZED:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_not_initialized_string);
        break;
    case EGL_BAD_ACCESS:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_bad_access_string);
        break;
    case EGL_BAD_ALLOC:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_bad_alloc_string);
        break;
    case EGL_BAD_ATTRIBUTE:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_bad_attribute_string);
        break;
    case EGL_BAD_CONTEXT:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_bad_context_string);
        break;
    case EGL_BAD_CONFIG:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_bad_config_string);
        break;
    case EGL_BAD_CURRENT_SURFACE:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_bad_current_surface_string);
        break;
    case EGL_BAD_DISPLAY:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_bad_display_string);
        break;
    case EGL_BAD_SURFACE:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_bad_surface_string);
        break;
    case EGL_BAD_MATCH:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_bad_match_string);
        break;
    case EGL_BAD_PARAMETER:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_bad_parameter_string);
        break;
    case EGL_BAD_NATIVE_PIXMAP:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_bad_native_pixmap_string);
        break;
    case EGL_BAD_NATIVE_WINDOW:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_bad_native_window_string);
        break;
    case EGL_CONTEXT_LOST:
        xab_log(LOG_DEBUG, "%s: %s\n", message, egl_context_lost_string);
        break;
    default:
        xab_log(LOG_DEBUG, "%s: %s\n", message, "Unknown EGL error");
        break;
    }
#endif

    if (do_assert_on_failure) {
        Assert(error == EGL_SUCCESS);
    }
}
