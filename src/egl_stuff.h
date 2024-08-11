#pragma once

#include "utils.h"

#include <stdio.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>

void pretty_print_egl_check(int do_assert_on_failure, const char *message);

// Function lists for EGL and OpenGL

// This is needed to get EGL native display from xcb connection
// eglGetDisplay(EGL_DEFAULT_DISPLAY) and eglGetDisplay(conn) work on my maching
// (Intel graphics), but this makes EGL guess what kind of pointer you pass to
// it, so I guess might not work in some cases.

////////// I HATE MY LIFE WHO TF DECIDED TO NAME THESE FUNCTIONS

#define EGL_FUNCTIONS(X)                                                       \
    X(PFNEGLGETPLATFORMDISPLAYEXTPROC, eglGetPlatformDisplayEXT)

// make sure you use functions that are valid for selected GL version (specified \
        // when context is created)
#define GL_FUNCTIONS(X)                                                        \
    X(PFNGLENABLEPROC, glEnable)                                               \
    X(PFNGLDISABLEPROC, glDisable)                                             \
    X(PFNGLBLENDFUNCPROC, glBlendFunc)                                         \
    X(PFNGLVIEWPORTPROC, glViewport)                                           \
    X(PFNGLCLEARCOLORPROC, glClearColor)                                       \
    X(PFNGLCLEARPROC, glClear)                                                 \
    X(PFNGLDRAWARRAYSPROC, glDrawArrays)                                       \
    X(PFNGLDRAWELEMENTSPROC, glDrawElements)                                   \
    X(PFNGLCREATEBUFFERSPROC, glCreateBuffers)                                 \
    X(PFNGLNAMEDBUFFERSTORAGEPROC, glNamedBufferStorage)                       \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray)                             \
    X(PFNGLBINDBUFFERPROC, glBindBuffer)                                       \
    X(PFNGLBUFFERDATAPROC, glBufferData)                                       \
    X(PFNGLCREATEVERTEXARRAYSPROC, glCreateVertexArrays)                       \
    X(PFNGLVERTEXARRAYATTRIBBINDINGPROC, glVertexArrayAttribBinding)           \
    X(PFNGLVERTEXARRAYVERTEXBUFFERPROC, glVertexArrayVertexBuffer)             \
    X(PFNGLVERTEXARRAYATTRIBFORMATPROC, glVertexArrayAttribFormat)             \
    X(PFNGLENABLEVERTEXARRAYATTRIBPROC, glEnableVertexArrayAttrib)             \
    X(PFNGLCREATESHADERPROGRAMVPROC, glCreateShaderProgramv)                   \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                                   \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog)                         \
    X(PFNGLGENPROGRAMPIPELINESPROC, glGenProgramPipelines)                     \
    X(PFNGLUSEPROGRAMSTAGESPROC, glUseProgramStages)                           \
    X(PFNGLBINDPROGRAMPIPELINEPROC, glBindProgramPipeline)                     \
    X(PFNGLPROGRAMUNIFORMMATRIX2FVPROC, glProgramUniformMatrix2fv)             \
    X(PFNGLPROGRAMUNIFORMMATRIX3FVPROC, glProgramUniformMatrix3fv)             \
    X(PFNGLBINDTEXTUREUNITPROC, glBindTextureUnit)                             \
    X(PFNGLCREATETEXTURESPROC, glCreateTextures)                               \
    X(PFNGLTEXTUREPARAMETERIPROC, glTextureParameteri)                         \
    X(PFNGLTEXTURESTORAGE2DPROC, glTextureStorage2D)                           \
    X(PFNGLTEXTURESUBIMAGE2DPROC, glTextureSubImage2D)                         \
    X(PFNGLDEBUGMESSAGECALLBACKPROC, glDebugMessageCallback)

#define X(type, name) static type name;
GL_FUNCTIONS(X)
#undef X

#ifndef NDEBUG
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
static void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id,
                                   GLenum severity, GLsizei length,
                                   const GLchar *message, const void *user) {
    fprintf(stderr, "%s\n", message);
    if (severity == GL_DEBUG_SEVERITY_HIGH ||
        severity == GL_DEBUG_SEVERITY_MEDIUM) {
        Assert(!"OpenGL API usage error! Use debugger to examine call stack!");
    }
}
#pragma GCC diagnostic pop
#endif
