#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "window.h"
#include "egl_stuff.h"
#include "logger.h"
#include "setbg.h"

static int count_bits(uint32_t mask) {
    int bits = 0;
    while (mask) {
        bits += mask & 1;
        mask >>= 1;
    }
    return bits;
}

Window_t init_window(WindowType_e window_type, EGLDisplay display,
                     x_data_t *xdata) {
    xab_log(LOG_DEBUG, "Initializing a window...\n");
    Assert(xdata != NULL && display != NULL && "Invalid pointers!");

    Window_t win = {0};
    win.window_type = window_type;

    // choose EGL configuration
    xab_log(LOG_DEBUG, "Choosing window's EGL configuration\n");
    EGLConfig config;
    {
        const unsigned int red_size = count_bits(xdata->visual->red_mask);
        const unsigned int green_size = count_bits(xdata->visual->green_mask);
        const unsigned int blue_size = count_bits(xdata->visual->blue_mask);
        Assert(red_size + green_size + blue_size == xdata->screen->root_depth &&
               "Mismatched visual root depth");

        // clang-format off
        EGLint attr[] = {
            EGL_SURFACE_TYPE,      EGL_WINDOW_BIT,
            EGL_CONFORMANT,        EGL_OPENGL_BIT,
            EGL_RENDERABLE_TYPE,   EGL_OPENGL_BIT,
            EGL_COLOR_BUFFER_TYPE, EGL_RGB_BUFFER,

            EGL_RED_SIZE,      red_size,
            EGL_GREEN_SIZE,    green_size,
            EGL_BLUE_SIZE,     blue_size,
            EGL_ALPHA_SIZE,    0,
            EGL_DEPTH_SIZE,   xdata->screen->root_depth,

            EGL_CONFIG_CAVEAT, EGL_NONE,
            EGL_NONE,
        };
        // clang-format on

        EGLint count;
        if (eglChooseConfig(display, attr, &config, 1, &count) != EGL_TRUE ||
            count != 1) {
            xab_log(LOG_FATAL, "Cannot choose EGL config\n");
        }
    }

    // create EGL context
    xab_log(LOG_DEBUG, "Creating window's EGL context\n");
    {

        const struct {
                int major, minor;
        } gl_versions[] = {{4, 6}, {4, 5}, {4, 4}, {4, 3},
                           {4, 2}, {4, 1}, {4, 0}, {3, 3}};
        for (unsigned int i = 0; i < sizeof(gl_versions) / sizeof(*gl_versions);
             i++) {
            // clang-format off
        EGLint attr[] = {
            EGL_CONTEXT_MAJOR_VERSION, gl_versions[i].major,
            EGL_CONTEXT_MINOR_VERSION, gl_versions[i].minor,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
#ifdef ENABLE_OPENGL_DEBUG_CALLBACK 
            // ask for debug context for non "Release" builds
            // this is so we can enable debug callback
            EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
#endif
            EGL_NONE,
        };
            // clang-format on

            win.context =
                eglCreateContext(display, config, EGL_NO_CONTEXT, attr);

            if (win.context != EGL_NO_CONTEXT) {
                xab_log(LOG_DEBUG, "Created an EGL OpenGL %d.%d Context\n",
                        gl_versions[i].major, gl_versions[i].minor);
                break;
            }
        }

        if (win.context == EGL_NO_CONTEXT) {
            xab_log(LOG_FATAL,
                    "Cannot create EGL context, OpenGL 3.3+ not supported?\n");
            // TODO: handle the error
        }
    }

    // create/get the window's pixmap
    switch (win.window_type) {
    default:
        xab_log(LOG_WARN, "Unknown window type! defaulting to XWINDOW\n");
    case XWINDOW:;
        xab_log(LOG_DEBUG, "Creating window's xcb window\n");
        win.xwindow = xcb_generate_id(xdata->connection);
        const uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        const uint32_t vals[] = {
            xdata->screen->black_pixel,
            XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS,
        };

        xcb_create_window(
            xdata->connection, XCB_COPY_FROM_PARENT, win.xwindow,
            xdata->screen->root, 0, 0, xdata->screen->width_in_pixels,
            xdata->screen->height_in_pixels, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
            xdata->screen->root_visual, mask, vals);
        xcb_map_window(xdata->connection, win.xwindow);
        // TODO: gracefully shut down when pressing the X button
        // (WM_DELETE_WINDOW?)
        break;
    case XBACKGROUND:
        xab_log(LOG_DEBUG, "Creating window's xcb pixmap\n");
        win.xpixmap = xcb_generate_id(xdata->connection);
        xcb_create_pixmap(xdata->connection, xdata->screen->root_depth,
                          win.xpixmap, xdata->screen->root,
                          xdata->screen->width_in_pixels,
                          xdata->screen->height_in_pixels);
        xcb_clear_area(xdata->connection, 0, win.xpixmap, 0, 0,
                       xdata->screen->width_in_pixels,
                       xdata->screen->height_in_pixels);

        xab_log(LOG_DEBUG, "Setting up and finding background window\n");
        win.desktop_window = setup_background(win.xpixmap, xdata);
        break;
    }
    Assert(win.xwindow != NULL &&
           "Window is still NULL even after window Initialization!");

    // create EGL surface
    xab_log(LOG_DEBUG, "Creating window's EGL surface\n");
    {
        // clang-format off
        const EGLint attr[] = {
#ifdef EGL_KHR_gl_colorspace
                EGL_GL_COLORSPACE, EGL_GL_COLORSPACE_SRGB,
#endif
            EGL_RENDER_BUFFER, EGL_BACK_BUFFER,
            EGL_NONE,
        };
        // clang-format on

        Assert(win.xpixmap != NULL ||
               win.xwindow != NULL && "Invalid xcb window/pixmap");
        switch (win.window_type) {
        case XBACKGROUND:
            win.surface =
                eglCreateWindowSurface(display, config, win.xpixmap, attr);
            break;
        case XWINDOW:
        default:
            win.surface =
                eglCreateWindowSurface(display, config, win.xwindow, attr);
            break;
        }
        if (win.surface == EGL_NO_SURFACE) {
            xab_log(LOG_FATAL, "Cannot create EGL surface\n");
            // TODO: handle the error
        }
    }

    xab_log(LOG_DEBUG, "Making window's EGL surface current\n");
    if (!eglMakeCurrent(display, win.surface, win.surface, win.context))
        xab_log(LOG_FATAL, "Failed to make EGL surface current: %s",
                get_EGL_error_string(eglGetError())); // TODO: handle the error

    return win;
}

void destroy_window(Window_t *win, EGLDisplay display, x_data_t *xdata) {
    Assert(win != NULL && display != NULL && xdata != NULL &&
           "Invalid pointers");

    switch (win->window_type) {
    default:
        break;
    case XWINDOW:
        break;
    case XBACKGROUND:
        xcb_free_pixmap(xdata->connection, win->xpixmap);
        break;
    }

    if (win->context != EGL_NO_CONTEXT) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, win->context);
    }
    if (win->surface != EGL_NO_SURFACE) {
        eglDestroySurface(display, win->surface);
    }
}
