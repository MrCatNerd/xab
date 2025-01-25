#include "shader_cache.h"
#include <epoxy/gl.h>

#ifdef HAVE_LIBCGLM
#ifdef LOG_LEVEL
#define CGLM_DEFINE_PRINTS
#endif
#include <cglm/affine.h>
#include <cglm/cam.h>
#include <cglm/io.h>
#include <cglm/mat4.h>
#include <cglm/struct.h>
#include <cglm/struct/io.h>
#include <cglm/types.h>
#endif

#include "wallpaper.h"
#include "camera.h"
#include "framebuffer.h"
#include "logger.h"
#include "shader.h"
#include "video_reader_interface.h"
#include "shader_cache.h"

void wallpaper_init(float scale, int width, int height, int x, int y,
                    bool pixelated, const char *video_path, wallpaper_t *dest,
                    int hw_accel, ShaderCache_t *scache) {
    xab_log(LOG_DEBUG, "Creating animated wallpaper: '%s' %dx%dpx at %dx%d\n",
            video_path, width, height, x, y);
    // save wallpaper position
    dest->x = x;
    dest->y = y;

    // load shaders
    dest->shader = shader_cache_create_or_cache_shader(
        "res/shaders/wallpaper_vertex.glsl",
        "res/shaders/wallpaper_fragment.glsl", scache);
    // "res/shaders/mouse_distance_thingy.glsl");

    // create vrc
    VideoReaderRenderConfig_t vrc = {
        .width = width,
        .height = height,
        .scale = scale,
        .pixelated = pixelated,
        .gl_internal_format = GL_RGBA,
        .hw_accel = hw_accel,
    };

    // open video
    dest->video = open_video(video_path, vrc, scache);
}

void wallpaper_render(wallpaper_t *wallpaper, camera_t *camera,
                      FrameBuffer_t *fbo_dest) {
    render_video(&wallpaper->video);

    // TODO: maybe drop the cglm dependency for glviewport (or make it
    // optional so i can stil mess around with the camera and transformations)
    // glViewport(wallpaper->x, 0,
    //            wallpaper->video.vrc.width * wallpaper->video.vrc.scale,
    //            wallpaper->video.vrc.height * wallpaper->video.vrc.scale);
    // glViewport(0, 0, abs((int)(camera->vpc.right - camera->vpc.left)),
    //            abs((int)(camera->vpc.top - camera->vpc.bottom)));

    glViewport(0, 0, fbo_dest->width, fbo_dest->height);

    use_shader(wallpaper->shader);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, get_video_ogl_texture(&wallpaper->video));
    glUniform1i(
        shader_get_uniform_location(wallpaper->shader, "u_wallpaperTexture"),
        0);

#ifdef HAVE_LIBCGLM
    bool all_identity = false;
    if (wallpaper->video.vrc.width <= 0 || wallpaper->video.vrc.height <= 0)
        all_identity = true;

    if (!all_identity) {
        // projection matrix
        glUniformMatrix4fv(
            shader_get_uniform_location(wallpaper->shader, "u_ortho_proj"), 1,
            GL_FALSE, (const GLfloat *)camera->ortho);

        // view matrix
        glUniformMatrix4fv(
            shader_get_uniform_location(wallpaper->shader, "u_view"), 1,
            GL_FALSE, (const GLfloat *)camera->view);

        // model matrix
        mat4 model = GLM_MAT4_IDENTITY_INIT;

        // todo: fullscreen not working
        // xab_log(LOG_INFO, "walx: %d waly: %d width: %d height: %d\n",
        //         wallpaper->x, wallpaper->y, wallpaper->video.vrc.width,
        //         wallpaper->video.vrc.height);
        vec4 move = {wallpaper->x + (wallpaper->video.vrc.width / 2.f),
                     wallpaper->y + (wallpaper->video.vrc.height / 2.f), 0.0f,
                     0.0f};
        glm_translate(model, move);

        vec4 da_scaler = {wallpaper->video.vrc.width / 2.f,
                          wallpaper->video.vrc.height / 2.f, 1.0f, 1.0f};
        glm_scale(model, da_scaler);

        glUniformMatrix4fv(
            shader_get_uniform_location(wallpaper->shader, "u_model"), 1,
            GL_FALSE, (const GLfloat *)model);

        // the projection flips it up
        glUniform1i(shader_get_uniform_location(wallpaper->shader, "u_flip_y"),
                    0);
    }
#else
    bool all_identity = true;
#endif

    if (all_identity) {
        // use identity matrix instead the actual matrices
        float identity_matrix[16];
        mat4_identity_nocglm(identity_matrix);

        // projection matrix
        glUniformMatrix4fv(
            shader_get_uniform_location(wallpaper->shader, "u_ortho_proj"), 1,
            GL_FALSE, (const GLfloat *)identity_matrix);

        // view matrix
        glUniformMatrix4fv(
            shader_get_uniform_location(wallpaper->shader, "u_view"), 1,
            GL_FALSE, (const GLfloat *)identity_matrix);

        // model matrix
        glUniformMatrix4fv(
            shader_get_uniform_location(wallpaper->shader, "u_model"), 1,
            GL_FALSE, (const GLfloat *)identity_matrix);

        // video reader flips once
        glUniform1i(shader_get_uniform_location(wallpaper->shader, "u_flip_y"),
                    1);
    }

    render_framebuffer_borrow_shader(fbo_dest, fbo_dest->fbo_id,
                                     wallpaper->shader);
}

void wallpaper_close(wallpaper_t *wallpaper, ShaderCache_t *scache) {
    xab_log(LOG_DEBUG, "Closing wallpaper: %s\n", wallpaper->video.path);
    close_video(&wallpaper->video, scache);
    shader_cache_unref_shader(wallpaper->shader, scache);
}
