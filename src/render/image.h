#pragma once

#include "texture.h"
#include "render/shader_cache.h"

#include <stdbool.h>

// OFC THEY HAD TO MAKE DIFFERENT VERSIONS OF YUV420P
typedef enum ImageColorStandard {
    IMAGE_CSTD_UNKNOWN = 0,
    IMAGE_CSTD_SRGB = 1,
    IMAGE_CSTD_YUV_UNKNOWN = 2,
    IMAGE_CSTD_YUV_BT601 = 3,
    IMAGE_CSTD_YUV_BT709 = 4,
    IMAGE_CSTD_YUV_BT2020 = 5,
} ImageColorStandard_e;
typedef enum ImageColorRange {
    IMAGE_CRANGE_JPEG = 0,
    IMAGE_CRANGE_MPEG = 1,
} ImageColorRange_e;

typedef struct Image {
        ImageColorStandard_e cstandard;
        ImageColorRange_e crange;
        int texture_count;
        Texture_t *textures;
} Image_t;

void image_create(Image_t *target, ImageColorStandard_e cstandard,
                  ImageColorRange_e crange, int width, int height,
                  bool pixelated);

/// completely black out an image
void image_clear(Image_t *image);

void image_activate_and_bind_textures(const Image_t *image);
void image_set_uniforms(const Image_t *image);

// NOTE: You must unref the shader from the shader cache manually!
Shader_t *image_get_appropriate_wallpaper_shader(Image_t *image,
                                                 ShaderCache_t *scache);

/// call this only if you own the textures!
void image_destroy(Image_t *image);
