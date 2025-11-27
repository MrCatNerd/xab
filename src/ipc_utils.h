#pragma once

#include <errno.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * Reads exactly `len` bytes from `fd` into `buf`.
 * Returns:
 *   0  -> EOF (socket closed)
 *   >0 -> success, number of bytes read (should be len)
 *   -1 -> error (check errno)
 *
 *  So it's basically recv_exact from rustc
 */
ssize_t recv_exact(int fd, const void *buf, size_t len, int flags) {
    uint8_t *p = (void *)buf;
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t n = recv(fd, p, remaining, flags);
        if (n == 0)
            return 0;
        else if (n < 0) {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // return how much we got so far
                return len - remaining;
            } else
                return -1;
        }
        remaining -= n;
        p += n;
    }

    return (ssize_t)len; // success
}

/**
 * Sends exactly `len` bytes from `buf` into `fd`.
 * Returns:
 *   0  -> EOF (socket closed)
 *   >0 -> success, number of bytes sent (should be len)
 *   -1 -> error (check errno)
 *
 *  So it's basically write_all from rustc
 */
ssize_t send_all(int fd, const void *buf, size_t len, int flags) {
    uint8_t *p = (void *)buf;
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t n = send(fd, p, remaining, flags);
        if (n == 0)
            return 0;
        else if (n < 0) {
            if (errno == EINTR)
                continue;
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
                // return how much we got se far
                return len - remaining;
            else
                return -1;
        }
        p += n;
        remaining -= n;
    }
    return (ssize_t)len; // success
}
