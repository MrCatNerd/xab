#pragma once

#include <bits/pthreadtypes.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
#include <stdbool.h>

typedef struct packet_queue_item {
        AVPacket *packet;
} packet_queue_item_t;

// we don't need a mutex, since we either access two different indexes at the
// same time, or the queue is empty, which means we can only push, all of the
// number stuff is handled automatically by atomic variables
typedef struct packet_queue {
        packet_queue_item_t *queue;
        _Atomic int
            packet_count; // idc this is constant, we must have atomic variables
                          // EVERYWHERE, this is totally not a bad idea
        _Atomic int front_idx;
        _Atomic int rear_idx;
} packet_queue_t;

packet_queue_t packet_queue_init(const int packet_count);

// TODO: wait suffix for functions and functions without wait that don't wait if
// they're full

bool packet_queue_put(packet_queue_t *pq, AVPacket *src_packet);
/// NOTE: dest_packet must be unrefed using av_packet_unref when ur done
bool packet_queue_get(packet_queue_t *pq, AVPacket *dest_packet);

void packet_queue_free(packet_queue_t *pq);
