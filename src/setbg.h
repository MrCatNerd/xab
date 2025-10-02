#pragma once

#include "x_data.h"

void update_background(xcb_pixmap_t *pixmap, x_data_t *xdata,
                       xcb_window_t *desktop_window);

xcb_window_t *setup_background(xcb_pixmap_t window_pixmap, x_data_t *xdata);
