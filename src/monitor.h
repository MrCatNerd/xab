#pragma once

#include "pch.h"

typedef struct monitor {
        char *name;
        int id;
        bool primary;
        int width, height;
        int x, y;
} monitor_t;

monitor_t *create_monitor(char *name, int id, bool primary, int x, int y,
                          int width, int height);

monitor_t *find_monitor_by_id(monitor_t **monitors, int count, int id);

typedef struct {
        monitor_t **monitors;
        int monitor_count;
} get_monitors_t;

#ifdef HAVE_LIBXRANDR
get_monitors_t get_monitors(xcb_connection_t *connection, xcb_screen_t *screen);
#endif /* HAVE_LIBXRANDR */

void cleanup_monitors(int count, monitor_t **monitors);
