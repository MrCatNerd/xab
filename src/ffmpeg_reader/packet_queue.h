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

// TODO: multithreading unit tests
// TODO: maybe packet chucks?
// TODO: maybe switch to ffmpeg allocation functions for packet_node_t

typedef struct packet_node {
        AVPacket *packet;
        struct packet_node *next;
} packet_node_t;

typedef struct packet_queue {
        packet_node_t *first_packet;
        /// actual last packet, not the last used one
        packet_node_t *last_packet;
        /// used packets
        int packet_count;
        /// actual size
        int size;

        float load_factor;
        int max_packets;

        pthread_mutex_t mutex;
} packet_queue_t;

packet_queue_t packet_queue_init(short load_factor, int max_packets);

bool packet_queue_put(packet_queue_t *pq, AVPacket *src_packet);
/// NOTE: dest_packet must be unrefed using av_packet_unref when ur done
bool packet_queue_get(packet_queue_t *pq, AVPacket *dest_packet);

void packet_queue_free(packet_queue_t *pq);

// -- internal --

// TODO: store this as a variable in the packet_queue struct and update it

/// get the last USED node from the packet queue
/// @warning not mt safe, you must manually lock the packet_queue_t->mutex
/// before using this function
packet_node_t *packet_queue_get_last_used_node(packet_queue_t *pq);

/// returns true if any unused nodes were freed
/// @warning not mt safe, you must manually lock the packet_queue_t->mutex
/// before using this function
bool packet_queue_handle_load_factor(packet_queue_t *pq);
