#pragma once

#include <epoxy/egl.h>
#ifdef ENABLE_OPENGL_DEBUG_CALLBACK
#include "logger.h"
#include "utils.h"
#endif /* ENABLE_OPENGL_DEBUG_CALLBACK */

/// Return a description of the specified EGL error
const char *get_EGL_error_string(EGLint error);

void pretty_print_egl_check(int do_assert_on_failure, const char *message);

#ifdef ENABLE_OPENGL_DEBUG_CALLBACK

// OpenGL error handling stuff
_Pragma("GCC diagnostic push")
    _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")

        static void APIENTRY
    DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                  GLsizei length, const GLchar *message, const void *user) {
    xab_log(LOG_ERROR, "%s\n", message);
    if (severity == GL_DEBUG_SEVERITY_HIGH ||
        severity == GL_DEBUG_SEVERITY_MEDIUM) {
        Assert(!"OpenGL API usage error! Use debugger to examine call stack!");
    }
}

_Pragma("GCC diagnostic pop")
#endif /* ENABLE_OPENGL_DEBUG_CALLBACK */
