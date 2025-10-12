#pragma once

#include "x_data.h"
typedef enum WindowType {
    XWINDOW_BACKGROUND = 0,
    XPIXMAP_BACKGROUND = 1,
    XWINDOW = 2,
} WindowType_e;

typedef struct Window {
        WindowType_e window_type;
        union {
                xcb_window_t xwindow;
                xcb_pixmap_t xpixmap;
        };
        /// NOTE: NULL on XWINDOW
        xcb_window_t *desktop_window;
        EGLSurface *surface;
        EGLContext *context;

        int width, height;
} Window_t;

Window_t init_window(WindowType_e window_type, EGLDisplay display,
                     x_data_t *xdata);
void window_handle_xcb_event(Window_t *win, xcb_generic_event_t *event,
                             uint8_t rt);
void destroy_window(Window_t *win, EGLDisplay display, x_data_t *xdata);
