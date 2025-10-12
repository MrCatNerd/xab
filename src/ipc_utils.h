#pragma once

#include <unistd.h>
#include <errno.h>
#include <stdint.h>

/**
 * Reads exactly `len` bytes from `fd` into `buf`.
 * Returns:
 *   0  -> EOF (socket closed)
 *   >0 -> success, number of bytes read (should be len)
 *   -1 -> error (check errno)
 *
 *  So it's basically recv_exact from rustc
 */
ssize_t recv_exact(int fd, void *buf, size_t len, int flags) {
    uint8_t *p = buf;
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t n = recv(fd, p, remaining, flags);
        if (n > 0) {
            remaining -= n;
            p += n;
        } else if (n == 0) {
            // connection closed
            return 0;
        } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // socket temporarily has no data - return how much we got so
                // far
                return len - remaining;
            } else {
                // real error
                return -1;
            }
        }
    }

    return len; // success
}
