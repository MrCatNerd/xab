#include <libavcodec/packet.h>

#include "packet_queue.h"
#include "logger.h"

packet_queue_t packet_queue_init(short load_factor) {
    packet_queue_t pq = {.first_packet = NULL,
                         .last_packet = NULL,
                         .packet_count = 0,
                         .load_factor = load_factor,
                         .size = 0};

    return pq;
}

bool packet_queue_put(packet_queue_t *pq, AVPacket *src_packet) {
    if (!src_packet || !pq)
        return false;

    // return if the size is greater or equal to the max size
    if (pq->size >= PACKET_QUEUE_MAX_PACKETS)
        return false;

    // ref dat packet
    AVPacket *ref_packet = NULL;
    if (av_packet_ref(ref_packet, src_packet) != 0)
        return false;
    else if (!ref_packet)
        return false;

    // alocate the packet and append it
    packet_node_t *pnode = calloc(1, sizeof(packet_queue_t));
    pnode->packet = ref_packet;

    packet_node_t *last_used_node = packet_queue_get_last_used_node(pq);
    if (last_used_node)
        last_used_node->next = pnode;
    else // this means there's probably no head, or im kinda dumb
        pq->first_packet = pnode;

    return true;
}

bool packet_queue_get(packet_queue_t *pq, AVPacket *dest_packet) {
    // get the first packet, link it up to the last used node, and unref it

    // set the first node to the next node
    packet_node_t *pnode = pq->first_packet;
    if (pnode == NULL) {
        return false;
    }
    pq->first_packet = pnode->next;

    // copy the dest packet to the first packet
    *dest_packet = *pnode->packet;

    // set the pnode to the end and reset it
    pnode->next = pq->last_packet->next;
    pnode->packet = NULL;
    pq->last_packet->next = pnode;
    pq->packet_count--;

    // handle load factor so the packets will be fred
    packet_queue_handle_load_factor(pq);

    // the user is responsible for unrefing the packet with av_packet_unref
    return true;
}

void packet_queue_free(packet_queue_t *pq) {
    packet_node_t *pnode = pq->first_packet;
    packet_node_t *next = NULL;

    int i = 0;
    while (pnode != NULL) {
        if (i < pq->packet_count && pnode->packet)
            av_packet_unref(pnode->packet);

        next = pnode->next;
        free(pnode);
        pnode = next;
        i++;
    }
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
    while (i < pq->size && pnode != NULL) {
        if (i >= pq->packet_count) {
            free(pnode);
        }

        i++;
        pnode = pnode->next;
    }

    return true;
}
