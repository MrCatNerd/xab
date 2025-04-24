#pragma once

#include "pch.h"

struct wallpaper_argument_options {
        char *video_path;
        int monitor;

        int offset_x;
        int offset_y;
        bool pixelated;
        int hw_accel;
};

struct argument_options {
        struct wallpaper_argument_options *wallpaper_options;
        int n_wallpaper_options;

        bool vsync;
        int max_framerate; // unfinished
};

struct argument_options parse_args(int argc, char **argv);
void clean_opts(struct argument_options *opts);
