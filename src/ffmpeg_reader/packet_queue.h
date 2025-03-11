#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/codec_par.h>
#include <libavcodec/packet.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
#include <libavformat/avio.h>
#include <libavutil/pixfmt.h>
#include <stdbool.h>

// TODO: unit tests
// TODO: maybe packet chucks?
// TODO: maybe switch to ffmpeg allocation functions for packet_node_t

// high priority:
// TODO: mutexes and stuff

#define PACKET_QUEUE_MAX_PACKETS 128
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
} packet_queue_t;

packet_queue_t packet_queue_init(short load_factor);

bool packet_queue_put(packet_queue_t *pq, AVPacket *src_packet);
/// NOTE: dest_packet must be unrefed using av_packet_unref when ur done
bool packet_queue_get(packet_queue_t *pq, AVPacket *dest_packet);

void packet_queue_free(packet_queue_t *pq);

// -- internal --

// TODO: store this as a variable in the packet_queue struct and update it
packet_node_t *packet_queue_get_last_used_node(packet_queue_t *pq);

/// returns true if any unused nodes were freed
bool packet_queue_handle_load_factor(packet_queue_t *pq);
