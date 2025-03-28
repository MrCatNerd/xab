#include "meson_error_codes.h"
#include "ffmpeg_reader/packet_queue.h"
#include <assert.h>
#include <libavcodec/packet.h>
#include <libavutil/mem.h>

int main(void) {
    int ret_code = MESON_OK;

    packet_queue_t pq = packet_queue_init(60, 0);

    AVPacket *src = av_packet_alloc();
    assert(src != NULL);

    // manually allocate data buffer for the packet
    src->size = 1024;                 // set packet size
    src->data = av_malloc(src->size); // allocate memory for data
    assert(src->data != NULL);

    // fill data with dummy values
    memset(src->data, 0xAA, src->size); // Example: Fill with 0xAA bytes

    // set some metadata (optional)
    src->pts = 100;
    src->dts = 90;
    src->stream_index = 1;

    // allocate destination packet
    AVPacket *dst = av_packet_alloc();
    assert(dst != NULL);

    if (!packet_queue_put(&pq, src))
        ret_code = MESON_FAIL;

    if (!packet_queue_get(&pq, dst))
        ret_code = MESON_FAIL;

    // test some of the metadata
    if (dst->pts != src->pts)
        ret_code = MESON_FAIL;

    // unref and free
    av_packet_unref(src);
    av_packet_unref(dst);

    packet_queue_free(&pq);
    av_packet_free(&src);
    av_packet_free(&dst);

    return ret_code;
}
