#include "image.h"

#include <epoxy/gl.h>
#include <stdlib.h>
#include <stdbool.h>

#include "logger.h"
#include "texture.h"
#include "shader_cache.h"
#include "utils.h"

void image_create(Image_t *target, ImageColorStandard_e cstandard,
                  ImageColorRange_e crange, int width, int height,
                  bool pixelated) {
    Assert(target != NULL && "Invalid target image pointer!");

    target->cstandard = cstandard;
    target->crange = crange;
    const TextureConfiguration_t tconf = DEFAULT_TEXTURE_CONF_B(pixelated);
    switch (cstandard) {
    case IMAGE_CSTD_UNKNOWN:
    case IMAGE_CSTD_SRGB: {
        xab_log(LOG_TRACE, "Creating an RGB %dx%d %s image", width, height,
                pixelated ? "pixelated" : "");
        target->texture_count = 1;
        target->textures = calloc(1, sizeof(Texture_t));
        create_texture(target->textures, width, height, GL_RGB, tconf);
    } break;
    case IMAGE_CSTD_YUV_UNKNOWN:
    case IMAGE_CSTD_YUV_BT601:
    case IMAGE_CSTD_YUV_BT709:
    case IMAGE_CSTD_YUV_BT2020: {
        xab_log(LOG_TRACE, "Creating a YUV420P %dx%d %s image\n", width, height,
                pixelated ? "pixelated" : "");
        target->texture_count = 3;
        target->textures = calloc(3, sizeof(Texture_t));
        // clang-format off
        // YUV420P has the Y full size and the UV half size
        // GL_R is just ragebait especially when GL_RG is valid
        create_texture(target->textures + 0,       width       ,       height       , GL_RED, tconf); // Y
        create_texture(target->textures + 1, (int)(width * 0.5), (int)(height * 0.5), GL_RED, tconf); // U
        create_texture(target->textures + 2, (int)(width * 0.5), (int)(height * 0.5), GL_RED, tconf); // V
        // clang-format on
    } break;
    }
}

void image_clear(Image_t *image) {
    for (int i = 0; i < image->texture_count; i++)
        clear_texture(&image->textures[i]);
}

void image_activate_and_bind_textures(const Image_t *image) {
    // TODO: query GL_MAX_IMAGE_UNITS / GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS
    for (int i = 0; i < image->texture_count; i++) {
        activate_texture(i);
        bind_texture(&image->textures[i]);
    }
}

void image_destroy_textures(Image_t *image) {
    Assert(image != NULL && "Invalid image pointer!");
    if (image->textures && image->texture_count > 0) {
        for (int i = 0; i < image->texture_count; i++) {
            destroy_texture(&image->textures[i]);
        }
        free(image->textures);
        image->textures = NULL;
        image->texture_count = 0;
    } else
        xab_log(LOG_WARN, "Invalid image - nothing to destroy!\n");
}
