#include "meson_error_codes.h"
#include "arg_parser.h"

#include <assert.h>
#include <string.h>

int main(void) {
    int argc = 1;
    char *argv[] = {"~/Videos/iamavideo.mp4",
                    "--monitor=1",
                    "--pixelated=1",
                    "--vsync=0",
                    "--max_framerate=360",
                    "--hw_accel=no",
                    "--ipc=1",
                    "--offset_x=50",
                    "--offset_y=-50",
                    NULL};

    struct argument_options opts = parse_args(argc, argv);

    // clang-format off
    if(
        opts.hw_accel != false ||
        opts.max_framerate != 360 ||
        opts.vsync != false ||
        opts.ipc != true ||
        opts.n_wallpaper_options != 1 ||
        opts.wallpaper_options != NULL
      )
        return MESON_FAIL;


    if (
        opts.wallpaper_options[0].monitor != 1 ||
        opts.wallpaper_options[0].pixelated != true ||
        opts.wallpaper_options[0].offset_x != 50 ||
        opts.wallpaper_options[0].offset_y != -50 ||
        strcmp(opts.wallpaper_options[0].video_path, "~/Videos/iamavideo.mp4") != 0
    )
        return MESON_FAIL;
    // clang-format on

    clean_opts(&opts);

    return MESON_OK;
}
