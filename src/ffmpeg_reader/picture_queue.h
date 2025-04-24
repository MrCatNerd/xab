#pragma once

#include <bits/pthreadtypes.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <stdbool.h>

typedef struct picture_queue_item {
        AVFrame *picture;
} picture_queue_item_t;

// we don't need a mutex, since we either access two different indexes at the
// same time, or the queue is empty, which means we can only push, all of the
// number stuff is handled automatically by atomic variables
typedef struct picture_queue {
        picture_queue_item_t *queue;
        _Atomic int picture_count; // idc this is constant, we must have atomic
                                   // variables EVERYWHERE, this is totally not
                                   // a bad idea
        _Atomic int front_idx;
        _Atomic int rear_idx;
} picture_queue_t;

picture_queue_t picture_queue_init(const int picture_count);

// TODO: wait suffix for functions and functions without wait that don't wait if
// they're full

bool picture_queue_put(picture_queue_t *pq, AVFrame *src_picture);
/// NOTE: dest_picture must be unrefed using av_picture_unref when ur done
bool picture_queue_get(picture_queue_t *pq, AVFrame *dest_picture);

void picture_queue_free(picture_queue_t *pq);
