#pragma once

#include "x_data.h"
typedef enum WindowType {
    XBACKGROUND = 0,
    XWINDOW = 1,
} WindowType_e;

typedef struct Window {
        WindowType_e window_type;
        union {
                xcb_window_t xwindow;
                xcb_pixmap_t xpixmap;
        };
        /// NOTE: NULL on XBACKGROUND
        xcb_window_t *desktop_window;
        EGLSurface *surface;
        EGLContext *context;
} Window_t;

Window_t init_window(WindowType_e window_type, EGLDisplay display,
                     x_data_t *xdata);
void destroy_window(Window_t *win, EGLDisplay display, x_data_t *xdata);
