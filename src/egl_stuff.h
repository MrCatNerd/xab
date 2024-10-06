#pragma once

#include "utils.h"

#include <stdio.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>

void pretty_print_egl_check(int do_assert_on_failure, const char *message);

#ifndef NDEBUG

// _Pragma cuz it works inside macros
_Pragma("GCC diagnostic push")
    _Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")

        static void APIENTRY
    DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                  GLsizei length, const GLchar *message, const void *user) {
    fprintf(stderr, "%s\n", message);
    if (severity == GL_DEBUG_SEVERITY_HIGH ||
        severity == GL_DEBUG_SEVERITY_MEDIUM) {
        Assert(!"OpenGL API usage error! Use debugger to examine call stack!");
    }
}

_Pragma("GCC diagnostic pop")
#endif
