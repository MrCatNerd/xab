#pragma once

#include <stdbool.h>

typedef struct Texture {
        unsigned int id;
        int gl_internal_format;
        int width, height;
} Texture_t;

void create_texture(Texture_t *target, int width, int height,
                    int gl_internal_format, bool pixelated);
void bind_texture(const Texture_t *texture);
void subimage_texture(const Texture_t *texture, int x, int y, void *data,
                      int width, int height);
void clear_texture(const Texture_t *texture);
void activate_texture(int slot);
void unbind_texture(void);

void destroy_texture(Texture_t *texture);
