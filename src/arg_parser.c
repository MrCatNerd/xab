#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arg_parser.h"
#include "wallpaper.h"
#include "logging.h"
#include "utils.h"

// spaghetti code go bRRR

static void usage(const char *program_name) {
    printf("Usage: %s <path/to/file.mp4> [options]\n", program_name);
    printf("Use -h for help\n");
}

static void help(const char *program_name) {
    printf("xab - X11 Animated Background\n\n"
           "Usage: %s <path/to/file.mp4> [options]\n\n"
           "--monitor=n         | which monitor to use                       "
           "                                 "
           "options:\n"
           "* -V, --version       | print version\n"
           "* -M, --monitor=n     | which monitor to use (requires xrandr "
           "and cglm dependencies)                (default: 0)\n"
           "* --vsync=0|1         | synchronize framerate to monitor "
           "framerate                                  (default: 0)\n"
           "* --max_framerate=0|n | limit framerate to n fps (overrides "
           "vsync)                                  (default: 0)\n\n"
           "per video/monitor options:\n"
           "* -p, --pixelated     | use bilinear instead of point filtering "
           "for rendering the background        (default: bilnear)\n"
           "* --hw_accel=0|1      | use hardware acceleration for video "
           "decoding (hardware needs to support it) (default: 1)\n",
           program_name);
}

void clean_opts(struct argument_options *opts) {
    for (int i = 0; i < opts->n_wallpaper_options; i++) {
        if (opts->wallpaper_options[i].video_path != NULL) {
            free(opts->wallpaper_options[i].video_path);
            opts->wallpaper_options[i].video_path = NULL;
        }
    }
    free(opts->wallpaper_options);
    opts->wallpaper_options = NULL;
}

struct argument_options parse_args(int argc, char *argv[]) {
    struct argument_options opts = {
        .wallpaper_options = NULL,
        .n_wallpaper_options = 0,
        .vsync = true,
        .max_framerate = 0,
    };

    const char *program_name = argv[0];
    if (argc <= 1) {
        usage(program_name);
        exit(EXIT_SUCCESS);
    }

    opts.n_wallpaper_options = 0;
    opts.wallpaper_options = NULL; // IDC IF ITS ALREADY SET TO NULL
    for (int i = 1; i < argc; i++) {
        char *token = argv[i];
        const char *key = strtok(token, "=");
        const char *value = strtok(NULL, "=");

        if (!strcmp(token, "--version") || !strcmp(token, "-V")) {
#ifdef VERSION
            printf("%s\n", VERSION);
#else
#warning "VERSION macro is not set"
            printf("no version specified while building\n");
#endif
            exit(EXIT_SUCCESS);
        }
        if (!strcmp(token, "--help") || !strcmp(token, "-h")) {
            help(program_name);
            exit(EXIT_SUCCESS);
        } else if (!strcmp(token, "--usage") || !strcmp(token, "-u")) {
            usage(program_name);
            exit(EXIT_SUCCESS);
        } else if (value == NULL) { // no "=" means its a video path, not a
                                    // perfect solution?, shush
            opts.n_wallpaper_options++;
            if (opts.wallpaper_options != NULL)
                opts.wallpaper_options = realloc(
                    opts.wallpaper_options, sizeof(struct wallpaper_options) *
                                                opts.n_wallpaper_options);
            else
                opts.wallpaper_options = calloc(
                    opts.n_wallpaper_options, sizeof(struct wallpaper_options));

            opts.wallpaper_options[opts.n_wallpaper_options - 1].video_path =
                strdup(key);

        } else if (!strcmp(key, "--monitor") || !strcmp(key, "-M")) {
#ifdef HAVE_LIBXRANDR
#ifdef HAVE_LIBCGLM
            opts.wallpaper_options[opts.n_wallpaper_options - 1].monitor =
                atoi(value);

            if (opts.wallpaper_options[opts.n_wallpaper_options - 1].monitor <
                0)
                opts.wallpaper_options[opts.n_wallpaper_options - 1].monitor =
                    -1;
#else
            program_error("'%s' was not compiled with cglm support. "
                          "'--monitor' is not supported.\n",
                          argv[0]);
#endif /* HAVE_LIBCGLM */
#else
            program_error("'%s' was not compiled with xcb-randr support. "
                          "'--monitor' is not supported.\n",
                          argv[0]);
#endif /* HAVE_LIBXRANDR */
        } else if (!strcmp(key, "--pixelated") || !strcmp(key, "-p")) {
            opts.wallpaper_options[opts.n_wallpaper_options - 1].pixelated =
                atoi(value) != 0;
        } else if (!strcmp(key, "--vsync") || !strcmp(key, "-v")) {
            opts.vsync = atoi(value) != 0;
        } else if (!strcmp(key, "--max_framerate") || !strcmp(key, "-m")) {
            opts.max_framerate = atoi(value) != 0;
        }
    }

#ifndef NVERBOSE
    VLOG("[arg_parser] video count: %d\n", opts.n_wallpaper_options);
    for (int i = 0; i < opts.n_wallpaper_options; i++) {
        VLOG("[arg parser] opts: %s:%d\n", opts.wallpaper_options[i].video_path,
             opts.wallpaper_options[i].monitor);
    }
#endif /* ifndef NVLOG */

    return opts;
}
