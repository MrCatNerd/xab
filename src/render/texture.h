#pragma once

#include <stdbool.h>
#include <epoxy/gl.h>

typedef struct Texture {
        unsigned int id;
        int gl_internal_format;
        int width, height;
} Texture_t;

typedef struct TextureConfiguration {
        int min_filter;
        int mag_filter;
        int wrap_s;
        int wrap_t;
} TextureConfiguration_t;
#define DEFAULT_TEXTURE_CONF                                                   \
    ((TextureConfiguration_t){.min_filter = GL_LINEAR,                         \
                              .mag_filter = GL_LINEAR,                         \
                              .wrap_s = GL_MIRRORED_REPEAT,                    \
                              .wrap_t = GL_MIRRORED_REPEAT})
#define DEFAULT_TEXTURE_CONF_PIX                                               \
    ((TextureConfiguration_t){.min_filter = GL_NEAREST,                        \
                              .mag_filter = GL_NEAREST,                        \
                              .wrap_s = GL_MIRRORED_REPEAT,                    \
                              .wrap_t = GL_MIRRORED_REPEAT})
#define DEFAULT_TEXTURE_CONF_B(b)                                              \
    b ? DEFAULT_TEXTURE_CONF_PIX : DEFAULT_TEXTURE_CONF

void create_texture(Texture_t *target, int width, int height,
                    int gl_internal_format, TextureConfiguration_t conf);
void reconfigure_texture(Texture_t *texture, TextureConfiguration_t *conf);
void bind_texture(const Texture_t *texture);
void subimage_texture(const Texture_t *texture, int x, int y, void *data,
                      int width, int height);
void clear_texture(const Texture_t *texture);
void activate_texture(int slot);
void unbind_texture(void);

void destroy_texture(Texture_t *texture);
