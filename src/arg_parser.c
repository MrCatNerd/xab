#include "pch.h"

#include "arg_parser.h"
#include "video_reader_interface.h"
#include "wallpaper.h"
#include "logger.h"

// spaghetti code go bRRR

static void usage(const char *program_name) {
    printf("Usage: %s <path/to/file.mp4> [options]\n", program_name);
    printf("Use -h for help\n");
}

static void help(const char *program_name) {
    printf(
        "xab - X11 Animated Background\n\n"
        "Usage: %s <path/to/file.mp4> [options]\n\n"
        "options:\n"
        "* -h, --help                  | print this help\n"
        "* -V, --version               | print version\n"
        "* -M, --monitor=n             | which monitor to use             "
        "                                           (default: -1)\n"
        "* --vsync=0|1                 | synchronize framerate to monitor "
        "framerate                                  (default: 0)\n"
        "* --max_framerate=0|n         | limit framerate to n fps (overrides "
        "vsync)                                  (default: 0)\n"
        "\nper video/monitor options:\n"
        "* -p=0|1, --pixelated=0|1     | use point instead of bilinear "
        "filtering for rendering the background        (default: 0 - "
        "bilinear)\n"
        "* --hw_accel=yes,no,auto      | use hardware acceleration for "
        "video decoding (hardware needs to support it) (default: auto)\n",
        program_name);
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
    opts.wallpaper_options = NULL; // sanity check
    for (int i = 1; i < argc; i++) {
        char *token = argv[i];
        const char *key = strtok(token, "=");
        const char *value = strtok(NULL, "=");

        if (!strcmp(token, "--version") || !strcmp(token, "-V")) {
#ifdef XAB_VERSION
            printf("%s\n", XAB_VERSION);
#else
#warning "XAB_VERSION macro is not set"
            printf("no version was specified while building\n");
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
                opts.wallpaper_options =
                    realloc( // realloc cuz this is not performancce
                             // critical code, there is no need to count the
                             // backgrounds and reserve space, also my
                             // implementation kinda relies on this to get the
                             // current background (opts.n_wallpaper_options-1)
                        opts.wallpaper_options,
                        sizeof(struct wallpaper_options) *
                            opts.n_wallpaper_options);
            else
                opts.wallpaper_options = calloc(
                    opts.n_wallpaper_options, sizeof(struct wallpaper_options));

            const int current_background = opts.n_wallpaper_options - 1;
            opts.wallpaper_options[current_background].video_path = strdup(key);
            opts.wallpaper_options[current_background].hw_accel =
                VR_HW_ACCEL_AUTO;

            opts.wallpaper_options[current_background].monitor = -1;

        } else if (!strcmp(key, "--vsync") || !strcmp(key, "-v")) {
            opts.vsync = atoi(value) != 0;
        } else if (!strcmp(key, "--max_framerate") || !strcmp(key, "-m")) {
            opts.max_framerate = atoi(value) != 0;
            // NOTE: any if statement below is a per-video statement, if there
            // is no video assigned, the option will be ignored
        } else if (opts.n_wallpaper_options < 1) {
            xab_log(LOG_WARN,
                    "ignoring option '%s' since there is no video assigned\n",
                    key);
        } else if (!strcmp(key, "--hw_accel")) {
            // hmmm switch statement of the first character?, nahhhh

            // TODO: decide if i want hw_accel global or per video

            const int current_background = opts.n_wallpaper_options - 1;

            if (!strcmp(value, "no"))
                opts.wallpaper_options[current_background].hw_accel =
                    VR_HW_ACCEL_NO;
            else if (!strcmp(value, "yes"))
                opts.wallpaper_options[current_background].hw_accel =
                    VR_HW_ACCEL_YES;
            else if (!strcmp(value, "auto"))
                opts.wallpaper_options[current_background].hw_accel =
                    VR_HW_ACCEL_AUTO;
            else // use auto
                opts.wallpaper_options[current_background].hw_accel =
                    VR_HW_ACCEL_AUTO;
        } else if (!strcmp(key, "--monitor") || !strcmp(key, "-M")) {
            const int current_background = opts.n_wallpaper_options - 1;
#ifdef HAVE_LIBXRANDR
#ifdef HAVE_LIBCGLM
            opts.wallpaper_options[current_background].monitor = atoi(value);
#else
            xab_log(LOG_ERROR,
                    "'%s' was not compiled with cglm support. "
                    "'--monitor' is not supported.\n",
                    argv[0]);
#endif /* HAVE_LIBCGLM */
#else
            xab_log(LOG_ERROR,
                    "'%s' was not compiled with xcb-randr support. "
                    "'--monitor' is not supported.\n",
                    argv[0]);
#endif /* HAVE_LIBXRANDR */

            // set every option thats smaller than 0 to -1, for consistency
            if (opts.wallpaper_options[current_background].monitor < 0)
                opts.wallpaper_options[current_background].monitor = -1;
        } else if (!strcmp(key, "--pixelated") || !strcmp(key, "-p")) {
            opts.wallpaper_options[opts.n_wallpaper_options - 1].pixelated =
                atoi(value) != 0;
        }
    }

    // TODO: if verbose
    xab_log(LOG_VERBOSE, "[arg_parser] video count: %d\n",
            opts.n_wallpaper_options);
    for (int i = 0; i < opts.n_wallpaper_options; i++) {
        xab_log(LOG_VERBOSE, "[arg parser] opts: %s:%d\n",
                opts.wallpaper_options[i].video_path,
                opts.wallpaper_options[i].monitor);
    }

    return opts;
}

void clean_opts(struct argument_options *opts) {
    xab_log(LOG_TRACE, "Cleaning argument options\n");
    for (int i = 0; i < opts->n_wallpaper_options; i++) {
        if (opts->wallpaper_options[i].video_path != NULL) {
            free(opts->wallpaper_options[i].video_path);
            opts->wallpaper_options[i].video_path = NULL;
        }
    }
    free(opts->wallpaper_options);
    opts->wallpaper_options = NULL;
}
