#pragma once

#include <libavutil/frame.h>

typedef struct picture_node {
        int frame_size_mb;
        AVFrame *picture;
        struct picture_node *next;
} picture_node_t;

typedef struct picture_queue {
        picture_node_t *first_picture;
        /// actual last picture, not the last used one
        picture_node_t *last_picture;
        /// used pictures
        int picture_count;
        /// actual size
        int size;

        float load_factor;
        float max_size_mb;
        float sum_framesize_mb; // im bad at naming things

        pthread_mutex_t mutex;
} picture_queue_t;

picture_queue_t picture_queue_init(short load_factor, float max_size_mb);

bool picture_queue_put(picture_queue_t *pq, AVFrame *src_picture,
                       unsigned int frame_size_bytes);
/// NOTE: dest_frame must be unrefed using av_frame_unref when ur done
bool picture_queue_get(picture_queue_t *pq, AVFrame *dest_picture);

void picture_queue_free(picture_queue_t *pq);

// -- internal --

// TODO: store this as a variable in the picture_queue struct and update it

/// get the last USED node from the picture queue
/// @warning not mt safe, you must manually lock the picture_queue_t->mutex
/// before using this function
picture_node_t *picture_queue_get_last_used_node(picture_queue_t *pq);

/// returns true if any unused nodes were freed
/// @warning not mt safe, you must manually lock the picture_queue_t->mutex
/// before using this function
bool picture_queue_handle_load_factor(picture_queue_t *pq);
