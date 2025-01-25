#include <string.h>

#include <xcb/xproto.h>
#include <stdio.h>
#include <stdlib.h>
#include <xcb/randr.h>

#include "monitor.h"
#include "logger.h"

monitor_t *create_monitor(char *name, int id, bool primary, int x, int y,
                          int width, int height) {
    xab_log(LOG_DEBUG, "Creating monitor %s size %dx%dpx\n", name, width,
            height);
    // TODO: remove malloc from the init function
    monitor_t *monitor = malloc(sizeof(monitor_t));
    if (!monitor) {
        xab_log(LOG_FATAL, "Failed to allocate memory for monitor\n");
        exit(EXIT_FAILURE);
    }

    monitor->name = name;
    monitor->id = id;
    monitor->primary = primary;
    monitor->x = x;
    monitor->y = y;
    monitor->width = width;
    monitor->height = height;

    return monitor;
}

monitor_t *find_monitor_by_id(monitor_t **monitors, int count, int id) {
    for (int i = 0; i < count; i++) {
        if (monitors[i]->id == id)
            return monitors[i];
    }

    return NULL;
}

monitor_t *find_monitor_by_name(monitor_t **monitors, int count,
                                const char *name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(monitors[i]->name, name) == 0)
            return monitors[i];
    }

    return NULL;
}

#ifdef HAVE_LIBXRANDR
get_monitors_t get_monitors(xcb_connection_t *connection,
                            xcb_screen_t *screen) {
    get_monitors_t ret = {.monitor_count = -1, .monitors = NULL};

    monitor_t **monitors = NULL;

    // get monitors
    xcb_randr_get_monitors_reply_t *monitors_reply =
        xcb_randr_get_monitors_reply(
            connection, xcb_randr_get_monitors(connection, screen->root, 0),
            NULL);

    if (!monitors_reply)
        return ret;

    int monitor_count = xcb_randr_get_monitors_monitors_length(monitors_reply);
    monitors = calloc(sizeof(monitor_t *), monitor_count);

    // iterate through monitors and set the appropriate values
    xcb_randr_monitor_info_iterator_t monitors_iter =
        xcb_randr_get_monitors_monitors_iterator(monitors_reply);
    int i = 0;
    while (monitors_iter.rem) {
        xcb_randr_monitor_info_t *monitor_info = monitors_iter.data;
        xcb_get_atom_name_reply_t *name_reply = xcb_get_atom_name_reply(
            connection, xcb_get_atom_name(connection, monitor_info->name),
            NULL);
        xcb_randr_monitor_info_next(&monitors_iter);

        char *name = xcb_get_atom_name_name(name_reply);
        name[xcb_get_atom_name_name_length(name_reply)] = '\0';

        monitors[i] = create_monitor(name, i, monitor_info->primary > 0,
                                     monitor_info->x, monitor_info->y,
                                     monitor_info->width, monitor_info->height);

        free(name_reply);
        i++;
    }

    free(monitors_reply);

    ret.monitors = monitors;
    ret.monitor_count = monitor_count;

    return ret;
}
#endif /* HAVE_LIBXRANDR */

void cleanup_monitors(int count, monitor_t **monitors) {
    xab_log(LOG_DEBUG, "Freeing %d monitors\n", count);
    for (int i = 0; i < count; i++)
        free(monitors[i]);
    free(monitors); // free the array
}
