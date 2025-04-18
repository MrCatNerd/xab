#include <libavcodec/packet.h>
#include <pthread.h>
#include <stdlib.h>
#include "packet_queue.h"
#include "logger.h"

packet_queue_t packet_queue_init(const int packet_count) {
    packet_queue_t pq = {
        .queue = calloc(packet_count, sizeof(packet_queue_item_t)),
        .packet_count = packet_count,
        .rear_idx = 0,
        .front_idx = 0,
    };

    // av_packet_alloc all of the packets, since putting the packets is simply
    // refing them
    for (int i = 0; i < pq.packet_count; i++) {
        pq.queue[i].packet = av_packet_alloc();
    }

    return pq;
}

bool packet_queue_put(packet_queue_t *pq, AVPacket *src_packet) {
    if (!pq || !src_packet)
        return false;

    // return false if the queue is full
    if ((pq->front_idx + 1) % pq->packet_count == pq->rear_idx)
        return false;

    // ref dat packet

    if (av_packet_ref(pq->queue[pq->front_idx].packet, src_packet) < 0) {
        xab_log(LOG_WARN, "packet queue: refing the packet failed...\n");
        return false;
    }

    // move front
    pq->front_idx = (pq->front_idx + 1) % pq->packet_count;

    return true;
}

bool packet_queue_get(packet_queue_t *pq, AVPacket *dest_packet) {
    if (!pq || !dest_packet)
        return false;

    // if the queue is empty, return false
    if (pq->front_idx == pq->rear_idx)
        return false;

    // ref the dest packet and unref the queue packet
    if (av_packet_ref(dest_packet, pq->queue[pq->rear_idx].packet) != 0) {
        xab_log(LOG_WARN,
                "packet_queue (get): unable te ref the dest packet!\n");
        // i don't want to return because this will stuck our queue
    }

    av_packet_unref(pq->queue[pq->rear_idx].packet);

    // move rear
    pq->rear_idx = (pq->rear_idx + 1) % pq->packet_count;

    return true;
}

void packet_queue_free(packet_queue_t *pq) {
    if (!pq)
        return;

    if (pq->queue) {
        for (int i = 0; i < pq->packet_count; i++) {
            // i assume all of the packets are unerefed
            av_packet_free(&pq->queue[i].packet);
        }

        free(pq->queue);
    }
}
