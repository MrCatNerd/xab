#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>

#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <xcb/xcb.h>
#include <xcb/xcb_aux.h>
#include <xcb/xproto.h>
#include <xcb/xcb_atom.h>

#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/glcorearb.h>
#include <EGL/eglplatform.h>

#include "logging.h"
#include "egl_stuff.h"
#include "utils.h"
#include "video.h"
#include "vertex.h"

// remember kids! set your pointers to NULL
static Video video;

static void program_error(const char *format, ...);

static void setup() {
    LOG("-- Initializing...\n");

#ifdef NEDBUG
    LOG("-- Currently in release mode");
#else
    LOG("-- Currently in debug mode");
#endif

    LOG("-- Checking egl extensions...\n");
    if (!eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS) ||
        !strstr(eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS),
                "EGL_EXT_platform_xcb")) {
        program_error("-- ERROR: EGL_EXT_platform_xcb is not supported.");
        exit(1);
    } else
        LOG("-- EGL_EXT_platform_xcb supported.\n");

    LOG("-- Connecting to the xcb server\n");
    // open the connection to the X server
    video.connection = xcb_connect(NULL, &video.screen_nbr);

    if (video.connection == NULL || xcb_connection_has_error(video.connection))
        program_error("Error opening display.\n");

    Assert(0 >= video.screen_nbr && "Invalid screen number.");

    LOG("-- Getting the first screen\n");
    // get the first screen
    video.screen =
        xcb_setup_roots_iterator(xcb_get_setup(video.connection)).data;

    if (video.screen == NULL)
        program_error("Unable to get the screen.\n");

    LOG("\n");
    LOG("Informations of screen %" PRIu32 ":\n", video.screen->root);
    LOG("  screen number.: %u\n", video.screen_nbr);
    LOG("  width.........: %" PRIu16 "px\n", video.screen->width_in_pixels);
    LOG("  height........: %" PRIu16 "px\n", video.screen->height_in_pixels);
    LOG("  depth.........: %" PRIu8 " bits\n", video.screen->root_depth);
    LOG("  white pixel...: 0x%08x\n", video.screen->white_pixel);
    LOG("  black pixel...: 0x%08x\n", video.screen->black_pixel);
    LOG("\n");

    // create a window
    video.window = xcb_generate_id(video.connection);
    {
        uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        uint32_t values[] = {
            video.screen->white_pixel,

            XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_POINTER_MOTION};
        xcb_void_cookie_t cookie_window = xcb_create_window_checked(
            video.connection, video.screen->root_depth, video.window,
            video.screen->root, 20, 200, video.screen->width_in_pixels,
            video.screen->height_in_pixels, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
            video.screen->root_visual, mask, values);

        xcb_void_cookie_t cookie_map =
            xcb_map_window_checked(video.connection, video.window);

        xcb_generic_error_t *error = NULL;

        error = xcb_request_check(video.connection, cookie_window);
        if (error) {
            fprintf(stderr, "ERROR: can't create window : %d\n",
                    error->error_code);
            xcb_disconnect(video.connection);
            exit(-1);
        }

        error = xcb_request_check(video.connection, cookie_map);
        if (error) {
            fprintf(stderr, "ERROR: can't map window : %d\n",
                    error->error_code);
            xcb_disconnect(video.connection);
            exit(-1);
        }
    }

    // sync everything up (i forgor why)
    xcb_aux_sync(video.connection);

    // Load EGL function pointers
#define X(type, name)                                                          \
    type name = (type)eglGetProcAddress(#name);                                \
    Assert(name != NULL && #name "function pointer is NULL.");

    EGL_FUNCTIONS(X)
#undef X

    // initialize EGL
    video.display =
        eglGetPlatformDisplayEXT(EGL_PLATFORM_XCB_EXT, video.connection,
                                 (const EGLint[]){
                                     EGL_PLATFORM_XCB_SCREEN_EXT,
                                     video.screen_nbr,
                                     EGL_NONE,
                                 });

    Assert(video.display != EGL_NO_DISPLAY && "Failed to get EGL display.");

    {

        EGLint major, minor;

        if (!eglInitialize(video.display, &major, &minor)) {
            program_error("Cannot initialize EGL display.");
        }
        if (major < 1 || minor < 5) {
            program_error("EGL version 1.5 or higher required. you are "
                          "currently on version: %d.%d",
                          major, minor);
        } else
            LOG("-- EGL version %d.%d\n", major, minor);

            // assert only on debug
#ifndef NDEBUG
        pretty_print_egl_check(false, "-- eglInitialize call");
#else
        pretty_print_egl_check(true, "-- eglInitialize call");
#endif
    }

    // choose OpenGL API for EGL, by default it uses OpenGL ES
    EGLBoolean ok = eglBindAPI(EGL_OPENGL_API);
    Assert(ok && "Failed to select OpenGL API for EGL");

    // get the background window id
    {
        xcb_intern_atom_cookie_t xroot_cookie = xcb_intern_atom(
            video.connection, true, strlen("_XROOTPMAP_ID"), "_XROOTPMAP_ID");
        xcb_intern_atom_cookie_t esetroot_cookie =
            xcb_intern_atom(video.connection, true, strlen("ESETROOT_PMAP_ID"),
                            "ESETROOT_PMAP_ID");

        xcb_intern_atom_reply_t *xroot_reply =
            xcb_intern_atom_reply(video.connection, xroot_cookie, NULL);

        xcb_intern_atom_reply_t *esetroot_reply =
            xcb_intern_atom_reply(video.connection, esetroot_cookie, NULL);

        Assert(xroot_reply != NULL && "Failed to get the _XROOTPMAP_ID atom");
        Assert(esetroot_reply != NULL &&
               "Failed to get the ESETROOT_PMAP_ID atom");

        xcb_get_property_reply_t *xroot_value = xcb_get_property_reply(
            video.connection,
            xcb_get_property(video.connection, 0, video.screen->root,
                             xroot_reply->atom, XCB_GET_PROPERTY_TYPE_ANY, 0,
                             ~0),
            NULL);

        xcb_get_property_reply_t *esetroot_value = xcb_get_property_reply(
            video.connection,
            xcb_get_property(video.connection, 0, video.screen->root,
                             esetroot_reply->atom, XCB_GET_PROPERTY_TYPE_ANY, 0,
                             ~0),
            NULL);

        video.xroot_window =
            (xcb_window_t *)xcb_get_property_value(xroot_value);
        video.esetroot_window =
            (xcb_window_t *)xcb_get_property_value(esetroot_value);

        free(xroot_value);
        free(xroot_reply);
        free(esetroot_value);
        free(esetroot_reply);
    }

    LOG("-- _XROOTPMAP_ID....: 0x%" PRIx32 "\n", *video.xroot_window);
    LOG("-- ESETROOT_PMAP_ID.: 0x%" PRIx32 "\n", *video.esetroot_window);

    // choose EGL configuration
    EGLConfig config;
    {
        // todo: framebuffer and to check the depth and color and stencil stuff
        // with the screen->root_depth thingy

        // clang-format off
        EGLint attr[] = {
            EGL_SURFACE_TYPE,      EGL_WINDOW_BIT,
            EGL_CONFORMANT,        EGL_OPENGL_BIT,
            EGL_RENDERABLE_TYPE,   EGL_OPENGL_BIT,
            EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,

            EGL_RED_SIZE,      8,
            EGL_GREEN_SIZE,    8,
            EGL_BLUE_SIZE,     8,
            EGL_DEPTH_SIZE,   24,

            EGL_ALPHA_SIZE,    8,
            EGL_STENCIL_SIZE,  8,

            // uncomment for multisampled framebuffer
            //EGL_SAMPLE_BUFFERS, 1,
            //EGL_SAMPLES,        4, // 4x MSAA

            EGL_NONE,
        };

        // clang-format on

        EGLint count;
        if (!eglChooseConfig(video.display, attr, &config, 1, &count) ||
            count != 1) {
            program_error("Cannot choose EGL config");
        }
    }

    // create EGL surface
    {
        // clang-format off
        EGLint attr[] = {
            EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_LINEAR, // or use EGL_GL_COLORSPACE_SRGB for sRGB framebuffer
            EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
            EGL_NONE,
        };
        // clang-format on

        video.surface =
            eglCreateWindowSurface(video.display, config, video.window, attr);
        if (video.surface == EGL_NO_SURFACE) {
            program_error("Cannot create EGL surface");
        }
    }

    // create EGL context
    {
        // clang-format off
        EGLint attr[] = {
            EGL_CONTEXT_MAJOR_VERSION, 3,
            EGL_CONTEXT_MINOR_VERSION, 3,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
#ifndef NDEBUG
            // ask for debug context for non "Release" builds
            // this is so we can enable debug callback
            EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
#endif
            EGL_NONE,
        };
        // clang-format on

        video.context =
            eglCreateContext(video.display, config, EGL_NO_CONTEXT, attr);
        if (video.context == EGL_NO_CONTEXT) {
            program_error(
                "Cannot create EGL context, OpenGL 3.3 not supported?");
        }
    }

    ok = eglMakeCurrent(video.display, video.surface, video.surface,
                        video.context);
    Assert(ok && "Failed to make context current");

    // load OpenGL functions
#define X(type, name)                                                          \
    name = (type)eglGetProcAddress(#name);                                     \
    Assert(name != NULL && #name "returned NULL");
    GL_FUNCTIONS(X)
#undef X

#ifndef NDEBUG
    // enable debug callback
    glDebugMessageCallback(&DebugCallback, NULL);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif

    // time to suffer with some bitmaps
    xcb_atom_t prop_root, prop_esetroot, type;

    int format, i;
    unsigned long length, after;
    unsigned char *data_root = NULL, *data_esetroot = NULL;
    EGLNativePixmapType pmap_d1 = 0, pmap_d2 = 0;
    // xcb_pixmap_t pmap_d1, pmap_d2;

    EGLSurface pmap_surf =
        eglCreatePixmapSurface(video.display, config, pmap_d1, NULL);
    // Assert(pmap_surf != EGL_NO_SURFACE && "Failed to create pixmap surface");
    //
}

static void mainloop() {
    LOG("-- Runninng main loop...\n");

    unsigned int vbo, ebo;

    // clang-format off
    // rectangle
    Vertex_t data[] = {
            { {0.5f ,  0.5f, 0.0f}, { 1.0f, 1.0f} ,{1.0f , 0.0f, 0.0f} }, // top right    
            { {0.5f , -0.5f, 0.0f}, { 1.0f, 0.0f} ,{ 0.0f, 1.0f, 0.0f} }, // bottom right 
            { {-0.5f, -0.5f, 0.0f}, { 0.0f, 0.0f} ,{ 0.0f, 0.0f, 1.0f} }, // bottom left  
            { {-0.5f,  0.5f, 0.0f}, { 0.0f, 1.0f} ,{ 1.0f, 1.0f, 0.0f} }, // top left     
    };

    int indices[] = {
        0, 1, 2,
        0, 3, 2
    };

    // clang-format on

    // vbo
    glCreateBuffers(1, &vbo);
    glNamedBufferStorage(vbo, sizeof(data), data, 0);

    // ebo
    glCreateBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    // vertex input
    GLuint vao;
    {
        glCreateVertexArrays(1, &vao);

        GLint vbuf_index = 0;
        glVertexArrayVertexBuffer(vao, vbuf_index, vbo, 0, sizeof(Vertex_t));

        GLint a_pos = 0;
        glVertexArrayAttribFormat(vao, a_pos, 3, GL_FLOAT, GL_FALSE,
                                  offsetof(Vertex_t, position));
        glVertexArrayAttribBinding(vao, a_pos, vbuf_index);
        glEnableVertexArrayAttrib(vao, a_pos);

        GLint a_uv = 1;
        glVertexArrayAttribFormat(vao, a_uv, 2, GL_FLOAT, GL_FALSE,
                                  offsetof(Vertex_t, uv));
        glVertexArrayAttribBinding(vao, a_uv, vbuf_index);
        glEnableVertexArrayAttrib(vao, a_uv);

        GLint a_color = 2;
        glVertexArrayAttribFormat(vao, a_color, 3, GL_FLOAT, GL_FALSE,
                                  offsetof(Vertex_t, color));
        glVertexArrayAttribBinding(vao, a_color, vbuf_index);
        glEnableVertexArrayAttrib(vao, a_color);
    }

    // checkerboard texture, with 50% transparency on black colors
    GLuint texture;
    {
        unsigned int pixels[] = {
            0x80000000,
            0xffffffff,
            0xffffffff,
            0x80000000,
        };

        glCreateTextures(GL_TEXTURE_2D, 1, &texture);
        glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_REPEAT);

        GLsizei width = 2;
        GLsizei height = 2;
        glTextureStorage2D(texture, 1, GL_RGBA8, width, height);
        glTextureSubImage2D(texture, 0, 0, 0, width, height, GL_RGBA,
                            GL_UNSIGNED_BYTE, pixels);
    }

    // fragment & vertex shaders for drawing triangle
    GLuint pipeline, vshader, fshader;
    {
        const char *glsl_vshader = ReadFile("res/shaders/vertex.glsl");

        const char *glsl_fshader = ReadFile("res/shaders/fragment.glsl");

        vshader = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &glsl_vshader);
        fshader = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &glsl_fshader);

        GLint linked;
        glGetProgramiv(vshader, GL_LINK_STATUS, &linked);
        if (!linked) {
            char message[1024];
            glGetProgramInfoLog(vshader, sizeof(message), NULL, message);
            fprintf(stderr, "%s\n", message);
            Assert(!"Failed to create vertex shader!");
        }

        glGetProgramiv(fshader, GL_LINK_STATUS, &linked);
        if (!linked) {
            char message[1024];
            glGetProgramInfoLog(fshader, sizeof(message), NULL, message);
            fprintf(stderr, "%s\n", message);
            Assert(!"Failed to create fragment shader!");
        }

        glGenProgramPipelines(1, &pipeline);
        glUseProgramStages(pipeline, GL_VERTEX_SHADER_BIT, vshader);
        glUseProgramStages(pipeline, GL_FRAGMENT_SHADER_BIT, fshader);

        free((void *)glsl_vshader);
        free((void *)glsl_fshader);
    }

    // setup global GL state
    {
        // enable alpha blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // disble depth testing
        glDisable(GL_DEPTH_TEST);

        // disable culling
        glDisable(GL_CULL_FACE);
    }

    bool vsync = true;
    EGLBoolean ok = eglSwapInterval(video.display, (int)vsync);
    xcb_generic_error_t *error;
    Assert(ok && "Failed to set vsync for EGL");

    xcb_void_cookie_t cookie =
        xcb_map_window_checked(video.connection, video.screen->root);
    if ((error = xcb_request_check(video.connection, cookie)) != NULL) {
        free(error);
        exit(-1);
    }

    struct timespec c1;
    clock_gettime(CLOCK_MONOTONIC, &c1);

    float angle = 0;

    while (true) {
        // get size
        xcb_get_geometry_cookie_t geom_cookie;
        xcb_get_geometry_reply_t *geometry;

        geom_cookie = xcb_get_geometry(video.connection, video.screen->root);
        geometry = xcb_get_geometry_reply(video.connection, geom_cookie, NULL);
        const int width = geometry->width;
        const int height = geometry->height;

        // delta time
        struct timespec c2;
        clock_gettime(CLOCK_MONOTONIC, &c2);
        float deltaTime =
            (float)(c2.tv_sec - c1.tv_sec) + 1e-9f * (c2.tv_nsec - c1.tv_nsec);
        c1 = c2;

        // render only if window size is non-zero
        if (width != 0 && height != 0) {
            // setup output size covering all client area of window
            glViewport(0, 0, width, height);

            // clear screen
            glClearColor(0.392f, 0.584f, 0.929f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                    GL_STENCIL_BUFFER_BIT);

            // setup rotation matrix in uniform
            {
                angle += deltaTime * 2.0f * (float)M_PI /
                         20.0f; // full rotation in 20 seconds
                angle = fmodf(angle, 2.0f * (float)M_PI);

                float aspect = (float)height / width;

                // clang-format off
                float matrix[] = {
                    cosf(angle) * aspect, -sinf(angle), 0.0f,
                    sinf(angle) * aspect, cosf(angle) , 0.0f,
                    1.0f                ,        0.0f , 0.0f,
                };
                // clang-format on

                GLint u_matrix = 0;
                glProgramUniformMatrix3fv(vshader, u_matrix, 1, GL_FALSE,
                                          matrix);
            }

            // activate shaders for next draw call
            glBindProgramPipeline(pipeline);

            // provide vertex input
            glBindVertexArray(vao);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

            // bind texture to texture unit
            GLint s_texture =
                0; // texture unit that sampler2D will use in GLSL code
            glBindTextureUnit(s_texture, texture);

            // draw 3 vertices as triangle
            glDrawElements(GL_TRIANGLES,
                           (unsigned int)(sizeof(indices) / sizeof(*indices)),
                           GL_UNSIGNED_INT, 0);

            // swap the buffers to show output
            if (!eglSwapBuffers(video.display, video.surface)) {
                program_error("Failed to swap OpenGL buffers!");
            }
        } else {
            // window is minimized, cannot vsync - instead sleep a bit
            if (vsync) {
                usleep(10 * 1000);
            }
        }
    }
}

static void cleanup() {
    LOG("-- Cleaning up...\n");

    eglTerminate(video.display);

    xcb_void_cookie_t unmap_window_cookie =
        xcb_unmap_window_checked(video.connection, video.window);
    if (xcb_request_check(video.connection, unmap_window_cookie))
        LOG("Unable to unmap window... deal with it lol\n");

    xcb_disconnect(video.connection);
}

int main(void) {
    setup();
    mainloop();
    cleanup();

    return EXIT_SUCCESS;
}

static void program_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "-- ERROR: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

/* int root_set_wallpaper_by_pixmap_id(xcb_pixmap_t *pixmap_id) {

    // make the server set the pixmap by this id as the root window's
    // background pixmap
    xcb_change_window_attributes(connection, screen->root,
                                 XCB_CW_BACK_PIXMAP, // value mask
                                 pixmap_id           // value
    );

    // make the drawing actually show up
    // the equivalent of cairo's `cr:reset_clip(); cr:paint()`
    xcb_clear_area(connection,
                   NULL, // exposures
                   screen->root,
                   // draw the whole root window
                   0, 0, 0, 0 // x, y, width, height
    );

    return 0;
} */
