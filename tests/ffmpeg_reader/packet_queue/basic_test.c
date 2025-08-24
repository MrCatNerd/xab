#include "meson_error_codes.h"
#include "ffmpeg_reader/packet_queue.h"

int main(void) {
    packet_queue_t pq = packet_queue_init(60);

    packet_queue_free(&pq);

    return MESON_OK;
}
