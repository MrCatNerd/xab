#pragma once

#include <stdio.h>

#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include "utils.h"

void pretty_print_egl_check(int do_assert_on_failure, const char *message);

GLenum glCheckError_(const char *file, int line);
void clear_error();

#ifndef NGLCALLDEBUG
// OpenGL calls debug stuff
#define GLCALL(x)                                                              \
    clear_error();                                                             \
    x;                                                                         \
    glCheckError_(__FILE__, __LINE__)
#else
#define GLCALL(x) x
#endif

#ifndef NDEBUG

// OpenGL error handling stuff
_Pragma("GCC diagnostic push");
_Pragma("GCC diagnostic ignored \"-Wunused-parameter\"");

static void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id,
                                   GLenum severity, GLsizei length,
                                   const GLchar *message, const void *user) {
    fprintf(stderr, "%s\n", message);
    if (severity == GL_DEBUG_SEVERITY_HIGH ||
        severity == GL_DEBUG_SEVERITY_MEDIUM) {
        Assert(!"OpenGL API usage error! Use debugger to examine call stack!");
    }
}

_Pragma("GCC diagnostic pop");
#endif
