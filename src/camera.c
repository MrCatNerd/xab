#ifdef HAVE_LIBCGLM
#include <cglm/call.h>
#include <cglm/cam.h>
#include <cglm/version.h>
#include <cglm/mat4.h>
#include <cglm/util.h>
#endif
#include <epoxy/gl.h>

#include "camera.h"

camera_t create_camera(float x, float y, float rotation, ViewPortConfig_t vpc) {
    camera_t camera = {
        .x = x,
        .y = y,
        .rotation = rotation,
        .vpc = vpc,
    };

    recalculate_view_matrix(&camera);
    recalculate_ortho_matrix(&camera);

    return camera;
}

void camera_move_and_rotate(camera_t *camera, float x, float y, float angle) {
    camera->x = x;
    camera->y = y;
    camera->rotation = angle;
    recalculate_view_matrix(camera);
}

void camera_move(camera_t *camera, float x, float y) {
    camera->x = x;
    camera->y = y;
    recalculate_view_matrix(camera);
}

void camera_rotate(camera_t *camera, float angle) {
    camera->rotation = angle;
    recalculate_view_matrix(camera);
}

void camera_reset_gl_viewport(camera_t *camera) {
    // im not sure if this is accurate but it gets the job done
    glViewport(camera->vpc.left, camera->vpc.top,
               camera->vpc.right - camera->vpc.left,
               camera->vpc.bottom - camera->vpc.top);
}

void camera_change_viewport_config(camera_t *camera, ViewPortConfig_t vpc) {
    camera->vpc = vpc;
    recalculate_ortho_matrix(camera);
}

void recalculate_ortho_matrix(camera_t *camera) {
#ifdef HAVE_LIBCGLM
    glm_ortho(camera->vpc.left, camera->vpc.right, camera->vpc.bottom,
              camera->vpc.top, camera->vpc.near, camera->vpc.far,
              camera->ortho);
#else
    mat4_identity_nocglm(camera->ortho);
#endif
}

void recalculate_view_matrix(camera_t *camera) {
#ifdef HAVE_LIBCGLM
    mat4 transform;

    vec4 position = {camera->x, camera->y, 0.0f, 1.0f};
    vec3 axis = {0.0f, 0.0f, 1.0f};

    // position
    mat4 position_mat = GLM_MAT4_IDENTITY_INIT;
    glm_translate(position_mat, position);

    // rotation
    mat4 rotation_mat = GLM_MAT4_IDENTITY_INIT;
    glm_rotate(rotation_mat, glm_rad(camera->rotation), axis);

    // multiply both
    glm_mul(position_mat, rotation_mat, transform);

    glm_mat4_inv(transform, camera->view);
#else
    mat4_identity_nocglm(camera->view);
#endif
}

void mat4_identity_nocglm(float *dest) {
    dest[0] = 1;
    dest[1] = 0;
    dest[2] = 0;
    dest[3] = 0;

    dest[4] = 0;
    dest[5] = 1;
    dest[6] = 0;
    dest[7] = 0;

    dest[8] = 0;
    dest[9] = 0;
    dest[10] = 1;
    dest[11] = 0;

    dest[12] = 0;
    dest[13] = 0;
    dest[14] = 0;
    dest[15] = 1;
}
