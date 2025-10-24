#include <pthread.h>
#include <stdlib.h>
#include "picture_queue.h"
#include "logger.h"

picture_queue_t picture_queue_init(const int picture_count) {
    picture_queue_t pq = {
        .queue = calloc(picture_count, sizeof(picture_queue_item_t)),
        .picture_count = picture_count,
        .rear_idx = 0,
        .front_idx = 0,
    };

    // av_picture_alloc all of the pictures, since putting the pictures is
    // simply refing them
    for (int i = 0; i < pq.picture_count; i++) {
        pq.queue[i].picture = av_frame_alloc();
    }

    return pq;
}

bool picture_queue_put(picture_queue_t *pq, AVFrame *src_picture) {
    if (!pq || !src_picture)
        return false;

    // return false if the queue is full
    if ((pq->front_idx + 1) % pq->picture_count == pq->rear_idx)
        return false;

    // ref dat picture

    if (av_frame_ref(pq->queue[pq->front_idx].picture, src_picture) < 0) {
        xab_log(LOG_WARN, "picture queue: refing the picture failed...\n");
        return false;
    }

    // move front
    pq->front_idx = (pq->front_idx + 1) % pq->picture_count;

    return true;
}

bool picture_queue_get(picture_queue_t *pq, AVFrame *dest_picture) {
    if (!pq || !dest_picture)
        return false;

    // if the queue is empty, return false
    if (pq->front_idx == pq->rear_idx)
        return false;

    // ref the dest picture and unref the queue picture
    if (av_frame_ref(dest_picture, pq->queue[pq->rear_idx].picture) != 0) {
        xab_log(LOG_WARN,
                "picture_queue (get): unable te ref the dest picture!\n");
        // i don't want to return because this will stuck our queue
    }

    av_frame_unref(pq->queue[pq->rear_idx].picture);

    // move rear
    pq->rear_idx = (pq->rear_idx + 1) % pq->picture_count;

    return true;
}

void picture_queue_free(picture_queue_t *pq) {
    if (!pq)
        return;

    if (pq->queue) {
        for (int i = 0; i < pq->picture_count; i++) {
            // i assume all of the pictures are unerefed
            av_frame_free(&pq->queue[i].picture);
        }

        free(pq->queue);
    }
}
