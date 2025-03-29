#include <pthread.h>
#include <stdio.h>
#include "picture_queue.h"
#include "logger.h"

static inline float bytes_to_mbs(unsigned int bytes) {
    return bytes / (1024.0 * 1024.0);
}

picture_queue_t picture_queue_init(short load_factor, float max_size_mb) {
    if (max_size_mb <= 0)
        max_size_mb = 64; // default

    picture_queue_t pq = {.first_picture = NULL,
                          .last_picture = NULL,
                          .picture_count = 0,
                          .load_factor = (float)load_factor / 100,
                          .max_size_mb = max_size_mb,
                          .mutex = PTHREAD_MUTEX_INITIALIZER,
                          .size = 0};

    pthread_mutex_init(&pq.mutex, NULL);

    return pq;
}

bool picture_queue_put(picture_queue_t *pq, AVFrame *src_picture,
                       unsigned int frame_size_bytes) {
    if (!src_picture || !pq)
        return false;
    pthread_mutex_lock(&pq->mutex);

    // convert frame size to mbs
    const float frame_size_mb = bytes_to_mbs(frame_size_bytes);
    printf("frames %f/%f mbs + %f mbs\n", pq->sum_framesize_mb, pq->max_size_mb,
           frame_size_mb);
    // return if the size is greater or equal to the max size
    if (pq->sum_framesize_mb + frame_size_mb > pq->max_size_mb) {
        pthread_mutex_unlock(&pq->mutex);
        return false;
    }

    // if set to true and refing the picture goes wrong, the picture will be
    // cleaned up
    bool new_picture = false;

    // if there is an unused picture, use it
    picture_node_t *pnode = NULL;
    if (pq->picture_count < pq->size) {
        new_picture = false;
        pnode = pq->first_picture;
        for (int i = 0; i < pq->picture_count - 1; i++) {
            if (!pnode) {
                break;
            }
            pnode = pnode->next;
        }
    }
    // if there isn't create allocate a new node with a picture
    if (!pnode) {
        new_picture = true;
        pnode = calloc(1, sizeof(picture_node_t));
        if (!pnode) {
            pthread_mutex_unlock(&pq->mutex);
            return false;
        }
        pnode->picture = av_frame_alloc();
    }

    // ref dat picture
    if (!pnode->picture) {
        pthread_mutex_unlock(&pq->mutex);
        return false;
    } else if (av_frame_ref(pnode->picture, src_picture) != 0) {
        if (new_picture) {
            av_frame_free(&pnode->picture);
            free(pnode);
        }
        pthread_mutex_unlock(&pq->mutex);
        return false;
    }

    // set the picture size in the pnode
    pnode->frame_size_mb = frame_size_mb;

    picture_node_t *last_used_node = picture_queue_get_last_used_node(pq);
    if (last_used_node) {
        last_used_node->next = pnode;
        // increase used picture count
        pq->picture_count++;
    } else // this means there's probably no head, or im kinda dumb
    {
        pq->first_picture = pnode;
        pq->last_picture = pnode;
        pq->picture_count = 1;
    }
    pq->size++;

    // add the frame size to the sum
    pq->sum_framesize_mb += pnode->frame_size_mb;

    pthread_mutex_unlock(&pq->mutex);
    return true;
}

bool picture_queue_get(picture_queue_t *pq, AVFrame *dest_picture) {
    if (!pq || !dest_picture)
        return false;
    pthread_mutex_lock(&pq->mutex);

    // get the first picture
    picture_node_t *pnode = pq->first_picture;
    if (pnode == NULL) {
        pthread_mutex_unlock(&pq->mutex);
        return false;
    }

    // copy the dest picture to the first picture
    if (av_frame_ref(dest_picture, pnode->picture) != 0) {
        pthread_mutex_unlock(&pq->mutex);
        return false;
    }

    // set the head to the next pnode
    pq->first_picture = pnode->next;

    // set the node to the end, only if it's not already at the end so it won't
    // create a circular link
    if (pnode != pq->last_picture) {
        pnode->next = pq->last_picture->next;
        pq->last_picture->next = pnode;
    }
    pq->picture_count--;
    pq->size--;

    // subtract the frame size from the sum
    pq->sum_framesize_mb -= pnode->frame_size_mb;

    // unref dat node
    av_frame_unref(pnode->picture);

    // handle load factor so the pictures will be fred
    picture_queue_handle_load_factor(pq);

    // the user is responsible for unrefing the picture with av_picture_unref

    pthread_mutex_unlock(&pq->mutex);
    return true;
}

void picture_queue_free(picture_queue_t *pq) {
    if (!pq)
        return;
    pthread_mutex_lock(&pq->mutex);

    picture_node_t *pnode = pq->first_picture;
    picture_node_t *next = NULL;

    int i = 0;
    while (pnode != NULL) {
        if (i < pq->picture_count && pnode->picture)
            av_frame_unref(pnode->picture);

        next = pnode->next;
        av_frame_free(&pnode->picture);
        free(pnode);
        pnode = next;
        i++;
    }
    pthread_mutex_unlock(&pq->mutex);

    // i must destroy the mutex only when it's unlocked :(, so all of the
    // threads that use the picture queue must be joined/destroyed before
    // calling this function... sucks to suck
    pthread_mutex_destroy(&pq->mutex);
}

picture_node_t *picture_queue_get_last_used_node(picture_queue_t *pq) {
    picture_node_t *picture = pq->first_picture;
    for (int i = 0; i < pq->picture_count - 1; i++) {
        if (!picture) {
            xab_log(LOG_WARN,
                    "picture queue: invalid picture node pointer! (%d/%d), "
                    "fixing picture count...\n",
                    i, pq->picture_count);
            // im not sure if i need i-1 or i but think its i
            pq->picture_count = i;
            pq->size = i;
            break;
        }
        // if (i < pq->picture_count)
        //     continue;

        picture = picture->next;
    }

    return picture;
}

bool picture_queue_handle_load_factor(picture_queue_t *pq) {
    float load = (float)(pq->size - pq->picture_count) / pq->size;
    if (load <= pq->load_factor)
        return false;

    // deallocate unused pictures

    int i = 0;
    picture_node_t *pnode = pq->first_picture;
    picture_node_t *next = NULL;
    picture_node_t *prev = NULL;
    while (i < pq->size && pnode != NULL) {
        next = pnode->next;
        if (i >= pq->picture_count) {
            // clean the picture
            av_frame_unref(pnode->picture);
            av_frame_free(&pnode->picture);
            free(pnode);

            pnode = NULL;
            if (prev)
                prev->next = next;
        }

        if (pnode)
            prev = pnode;
        pnode = next;
        i++;
    }

    return true;
}
