#include "meson_error_codes.h"
#include "ffmpeg_reader/packet_queue.h"
#include <assert.h>
#include <libavcodec/packet.h>
#include <libavutil/mem.h>

static void init_fake_packet(AVPacket *pkt, int metadata_pts);

int main(void) {
    int ret_code = MESON_OK;

    // allocate packet queue
    packet_queue_t pq =
        packet_queue_init(60, 128); // have at least 100 packets in max packets

    // allocate destination packet
    AVPacket *dst = av_packet_alloc();
    assert(dst != NULL);

    // allocate 100 source packets with fake data and put them in the queue
    AVPacket *srcs[100];
    const int srcs_size = (int)(sizeof(srcs) / sizeof(*srcs));
    for (int i = 0; i < srcs_size; i++) {
        srcs[i] = av_packet_alloc();
        init_fake_packet(srcs[i], i);
        if (!packet_queue_put(&pq, srcs[i]))
            ret_code = MESON_FAIL;
    }

    // get them and unref them
    int i = 0;
    while (!packet_queue_get(&pq, dst)) {
        if (dst->pts != i)
            ret_code = MESON_FAIL;
        av_packet_unref(dst);
        i++;
    }

    // clean and free
    packet_queue_free(&pq);

    av_packet_free(&dst);

    for (int i = 0; i < srcs_size; i++) {
        AVPacket *src = srcs[i];
        av_packet_unref(src);
        av_packet_free(&src);
    };

    return ret_code;
}

static void init_fake_packet(AVPacket *pkt, int metadata_pts) {
    assert(pkt != NULL);

    // manually allocate data buffer for the packet
    pkt->size = 1024;                 // set packet size
    pkt->data = av_malloc(pkt->size); // allocate memory for data
    assert(pkt->data != NULL);

    // fill data with dummy values
    memset(pkt->data, 0xAA, pkt->size); // Example: Fill with 0xAA bytes

    // set some metadata (optional)
    pkt->pts = metadata_pts;
    pkt->dts = 90;
    pkt->stream_index = 1;
}
