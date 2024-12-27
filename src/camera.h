#pragma once

#ifdef HAVE_LIBCGLM
#include <cglm/types.h>
#endif

typedef struct ViewPortConfig {
        float left;
        float right;
        float bottom;
        float top;
        float near;
        float far;
} ViewPortConfig_t;

typedef struct camera {
        float x, y;
        float rotation;
        ViewPortConfig_t vpc;

#ifdef HAVE_LIBCGLM
        mat4 ortho;
        mat4 view;
#else
        float ortho[16];
        float view[16];
#endif
} camera_t;

camera_t create_camera(float x, float y, float rotation, ViewPortConfig_t vpc);

void camera_move_and_rotate(camera_t *camera, float x, float y, float angle);
void camera_move(camera_t *camera, float x, float y);
void camera_rotate(camera_t *camera, float angle);

void camera_reset_gl_viewport(camera_t *camera);
void camera_change_viewport_config(camera_t *camera, ViewPortConfig_t vpc);

void recalculate_view_matrix(camera_t *camera);
void recalculate_ortho_matrix(camera_t *camera);

void mat4_identity_nocglm(float *dest);
