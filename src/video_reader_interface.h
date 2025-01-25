#pragma once

#include <stdbool.h>

#include "shader_cache.h"

// TODO: maybe expose the actual video with and height from the struct (not the
// target width and height from VideoReaderRenderConfig)

enum VR_HW_ACCEL {
    VR_HW_ACCEL_NO = 0,
    VR_HW_ACCEL_YES = 1,
    VR_HW_ACCEL_AUTO = 2,
};

/**
 * @class VideoReaderRenderConfig
 * @brief Configuration for video rendering
 *
 */
typedef struct VideoReaderRenderConfig {
        /**
         * @brief width and height of the video
         */
        int width, height;
        /**
         * @brief scale of the video
         */
        float scale;
        /**
         * @brief true: point flitering for rendering, false: bilinear filtering
         */
        bool pixelated;
        /**
         * @brief gl internal format of the video framebuffer/texture
         */
        int gl_internal_format;
        /**
         * @brief harwdare acceleration: use the enum 'VR_HW_ACCEL' to specify
         */
        enum VR_HW_ACCEL hw_accel;
} VideoReaderRenderConfig_t;

/**
 * @class VideoReaderState
 * @brief the state of the video reader
 *
 */
typedef struct VideoReaderState {
        /**
         * @brief path to the video
         */
        const char *path;
        /**
         * @brief video rendering config, do not change after initialization
         */
        VideoReaderRenderConfig_t vrc;

        /**
         * @brief pointer to video reader specific implementation (such as
         * libmpv) internal data
         */
        void *internal;
} VideoReaderState_t;

/**
 * @brief Open a video
 *
 * @param path - path to the video
 * @param vr_config - video rendering config
 * @return video reader state
 */
VideoReaderState_t open_video(const char *path,
                              VideoReaderRenderConfig_t vr_config,
                              ShaderCache_t *scache);

/**
 * @brief Render a video to the VideoReaderState's framebuffer/texture
 *
 * @param state - video reader state
 */
void render_video(VideoReaderState_t *state);

/**
 * @brief Get an id to the video reader's opengl texture
 *
 * @param state - video reader state
 * @return id to the video reader's opengl texture
 */
unsigned int get_video_ogl_texture(VideoReaderState_t *state);

/**
 * @brief Pause a video
 *
 * @param state - video reader state
 */
void pause_video(VideoReaderState_t *state);

/**
 * @brief Unpause a video
 *
 * @param state - video reader state
 */
void unpause_video(VideoReaderState_t *state);

/**
 * @brief Close down a video
 *
 * @param state - video reader state
 */
void close_video(VideoReaderState_t *state, ShaderCache_t *scache);

/// (optional implementation)
/// don't worry about it
void report_swap_video(VideoReaderState_t *state);
