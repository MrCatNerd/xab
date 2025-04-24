#pragma once

#include "pch.h"

struct wallpaper_argument_options {
        char *video_path;
        int monitor;

        int offset_x;
        int offset_y;
        bool pixelated;
};

struct argument_options {
        struct wallpaper_argument_options *wallpaper_options;
        int n_wallpaper_options;

        bool vsync;
        enum VR_HW_ACCEL hw_accel;
        int max_framerate; // unfinished
};

struct argument_options parse_args(int argc, char **argv);
void clean_opts(struct argument_options *opts);
