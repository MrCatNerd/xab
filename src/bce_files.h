#pragma once
#ifndef DISABLE_BCE
#include "length_string.h"

typedef struct bce_file {
        const char *path;
        const length_string_t contents;
} bce_file_t;

// clang-format off

const char RES_SHADERS_FRAMEBUFFER_FRAGMENT_STR[] = 
#include "framebuffer_fragment.h"

const char RES_SHADERS_FRAMEBUFFER_VERTEX_STR[] = 
#include "framebuffer_vertex.h"

const char RES_SHADERS_MOUSE_DISTANCE_THINGY_STR[] = 
#include "mouse_distance_thingy.h"

const char RES_SHADERS_WALLPAPER_FRAGMENT_STR[] = 
#include "wallpaper_fragment.h"

const char RES_SHADERS_WALLPAPER_VERTEX_STR[] = 
#include "wallpaper_vertex.h"

//////////////////////////////////////////////////////////////////////////////

// the paths are relative
const bce_file_t RES_SHADERS_WALLPAPER_VERTEX = {"res/shaders/wallpaper_vertex.glsl", {(char*)RES_SHADERS_WALLPAPER_VERTEX_STR, sizeof(RES_SHADERS_WALLPAPER_VERTEX_STR) / sizeof(*RES_SHADERS_FRAMEBUFFER_FRAGMENT_STR)}};
const bce_file_t RES_SHADERS_FRAMEBUFFER_FRAGMENT = {"res/shaders/framebuffer_fragment.glsl", {(char*)RES_SHADERS_FRAMEBUFFER_FRAGMENT_STR, sizeof(RES_SHADERS_FRAMEBUFFER_FRAGMENT_STR) / sizeof(*RES_SHADERS_FRAMEBUFFER_FRAGMENT_STR)}};
const bce_file_t RES_SHADERS_FRAMEBUFFER_VERTEX = {"res/shaders/framebuffer_vertex.glsl", {(char*)RES_SHADERS_FRAMEBUFFER_VERTEX_STR, sizeof(RES_SHADERS_FRAMEBUFFER_VERTEX_STR) / sizeof(*RES_SHADERS_FRAMEBUFFER_VERTEX_STR)}};
const bce_file_t RES_SHADERS_MOUSE_DISTANCE_THINGY = {"res/shaders/mouse_distance_thingy.glsl", { (char*)RES_SHADERS_MOUSE_DISTANCE_THINGY_STR, sizeof(RES_SHADERS_MOUSE_DISTANCE_THINGY_STR) / sizeof(*RES_SHADERS_MOUSE_DISTANCE_THINGY_STR)}};
const bce_file_t RES_SHADERS_WALLPAPER_FRAGMENT = {"res/shaders/wallpaper_fragment.glsl", {(char*)RES_SHADERS_WALLPAPER_FRAGMENT_STR, sizeof(RES_SHADERS_WALLPAPER_FRAGMENT_STR) / sizeof(*RES_SHADERS_WALLPAPER_FRAGMENT_STR)}};

const bce_file_t bce_files[] = {
    RES_SHADERS_WALLPAPER_VERTEX,   RES_SHADERS_FRAMEBUFFER_FRAGMENT,
    RES_SHADERS_FRAMEBUFFER_VERTEX, RES_SHADERS_MOUSE_DISTANCE_THINGY,
    RES_SHADERS_WALLPAPER_FRAGMENT,
};
const unsigned int bce_files_len = sizeof(bce_files) / sizeof(*bce_files);

// clang-format on

#endif
