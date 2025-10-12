#pragma once

#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "context.h"

// go see ipc_spec.h for ipc commands and stuff

typedef struct IPC_client {
        int fd;
} IPC_client_t;

typedef struct IPC_handle {
        const char *path;
        int server_fd;
        struct sockaddr_un server_addr;

        int client_count;
        IPC_client_t *clients;

        // epoll stuff
        int epoll_fd;
} IPC_handle_t;

IPC_handle_t ipc_init(const char *name);
void ipc_poll_events(IPC_handle_t *handle, context_t *context);
void ipc_close(IPC_handle_t *handle);
