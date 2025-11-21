#include "texture.h"

#include <stdbool.h>
#include <epoxy/gl.h>

#include "logger.h"
#include "utils.h"

void create_texture(Texture_t *target, int width, int height,
                    int gl_internal_format, TextureConfiguration_t conf) {
    Assert(target != NULL && "Invalid texture target pointer!");

    // Texture
    xab_log(LOG_DEBUG, "Creating texture: %dx%dpx\n", width, height);
    unsigned int texture_id;
    glGenTextures(1, &texture_id);

    reconfigure_texture(target, &conf);

    target->width = width;
    target->height = height;
    target->id = texture_id;
    target->gl_internal_format = gl_internal_format;

    clear_texture(target);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void reconfigure_texture(Texture_t *texture, TextureConfiguration_t *conf) {
    Assert(texture != NULL && conf != NULL && "Invalid pointers!");
    bind_texture(texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, conf->min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, conf->mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, conf->wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, conf->wrap_t);
}
void bind_texture(const Texture_t *texture) {
    glBindTexture(GL_TEXTURE_2D, texture->id);
}

void subimage_texture(const Texture_t *texture, int x, int y, void *data,
                      int width, int height) {
    if (data == NULL) {
        xab_log(LOG_WARN, "Filling texture with NULL is invalid!, use "
                          "clear_texture instead!\n");
        return;
    }
    bind_texture(texture);

    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height,
                    texture->gl_internal_format, GL_UNSIGNED_BYTE, data);
}

void clear_texture(const Texture_t *texture) {
    bind_texture(texture);

    glTexImage2D(GL_TEXTURE_2D, 0, texture->gl_internal_format, texture->width,
                 texture->height, 0, texture->gl_internal_format,
                 GL_UNSIGNED_BYTE, NULL);
}

void activate_texture(int slot) { glActiveTexture(GL_TEXTURE0 + slot); }
void unbind_texture(void) { glBindTexture(GL_TEXTURE_2D, 0); }

void destroy_texture(Texture_t *texture) { glDeleteTextures(1, &texture->id); }
