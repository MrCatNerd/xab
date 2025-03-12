#include <libavcodec/packet.h>
#include <pthread.h>

#include "packet_queue.h"
#include "logger.h"

packet_queue_t packet_queue_init(short load_factor) {
    packet_queue_t pq = {.first_packet = NULL,
                         .last_packet = NULL,
                         .packet_count = 0,
                         .load_factor = (float)load_factor / 100,
                         .mutex = PTHREAD_MUTEX_INITIALIZER,
                         .size = 0};

    pthread_mutex_init(&pq.mutex, NULL);

    return pq;
}

bool packet_queue_put(packet_queue_t *pq, AVPacket *src_packet) {
    pthread_mutex_lock(&pq->mutex);
    if (!src_packet || !pq)
        return false;

    // return if the size is greater or equal to the max size
    if (pq->size >= PACKET_QUEUE_MAX_PACKETS)
        return false;

    // ref dat packet
    AVPacket *dst_ref_packet = av_packet_alloc();
    if (!dst_ref_packet)
        return false;
    else if (av_packet_ref(dst_ref_packet, src_packet) != 0) {
        av_packet_free(&dst_ref_packet);
        return false;
    }

    // alocate the packet and append it
    packet_node_t *pnode = calloc(1, sizeof(packet_node_t));
    if (!pnode) {
        av_packet_free(&dst_ref_packet);
        return false;
    }
    pnode->packet = dst_ref_packet;

    packet_node_t *last_used_node = packet_queue_get_last_used_node(pq);
    if (last_used_node)
        last_used_node->next = pnode;
    else // this means there's probably no head, or im kinda dumb
    {
        pq->first_packet = pnode;
        pq->last_packet = pnode;
    }

    pthread_mutex_unlock(&pq->mutex);
    return true;
}

bool packet_queue_get(packet_queue_t *pq, AVPacket *dest_packet) {
    if (!pq || !dest_packet)
        return false;
    pthread_mutex_lock(&pq->mutex);

    // get the first packet
    packet_node_t *pnode = pq->first_packet;
    if (pnode == NULL) {
        return false;
    }

    // copy the dest packet to the first packet
    if (av_packet_ref(dest_packet, pnode->packet) != 0)
        return false;

    // set the head to the next pnode
    pq->first_packet = pnode->next;

    // set the node to the end, only if it's not already at the end so it won't
    // create a circular link
    if (pnode != pq->last_packet) {
        pnode->next = pq->last_packet->next;
        pq->last_packet->next = pnode;
    }
    pq->packet_count--;

    // unref dat node
    av_packet_unref(pnode->packet);

    // handle load factor so the packets will be fred
    packet_queue_handle_load_factor(pq);

    // the user is responsible for unrefing the packet with av_packet_unref

    pthread_mutex_unlock(&pq->mutex);
    return true;
}

void packet_queue_free(packet_queue_t *pq) {
    pthread_mutex_lock(&pq->mutex);
    packet_node_t *pnode = pq->first_packet;
    packet_node_t *next = NULL;

    int i = 0;
    while (pnode != NULL) {
        if (i < pq->packet_count && pnode->packet)
            av_packet_unref(pnode->packet);

        next = pnode->next;
        av_packet_free(&pnode->packet);
        free(pnode);
        pnode = next;
        i++;
    }
    pthread_mutex_unlock(&pq->mutex);

    // i must destroy the mutex only when it's unlocked :(, so all of the
    // threads that use the packet queue must be joined/destroyed before calling
    // this function... sucks to suck
    pthread_mutex_destroy(&pq->mutex);
}

packet_node_t *packet_queue_get_last_used_node(packet_queue_t *pq) {
    packet_node_t *packet = pq->first_packet;
    for (int i = 0; i < pq->packet_count; i++) {
        if (!packet) {
            xab_log(LOG_WARN,
                    "packet queue: invalid packet node pointer! (%d/%d), "
                    "fixing packet count\n",
                    i, pq->packet_count);
            pq->packet_count = i;
            pq->size = i;
            break;
        }

        packet = packet->next;
    }

    return packet;
}

bool packet_queue_handle_load_factor(packet_queue_t *pq) {
    float load = (float)(pq->size - pq->packet_count) / pq->size;
    if (load <= pq->load_factor)
        return false;

    // deallocate unused packets

    int i = 0;
    packet_node_t *pnode = pq->first_packet;
    packet_node_t *next = NULL;
    packet_node_t *prev = NULL;
    while (i < pq->size && pnode != NULL) {
        next = pnode->next;
        if (i >= pq->packet_count) {
            // clean the packet
            av_packet_unref(pnode->packet);
            av_packet_free(&pnode->packet);
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
