#include "ipc.h"
#include "context.h"
#include "logger.h"
#include "ipc_utils.h"

#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "ipc_spec.h"

static int set_nonblocking(int sockfd);
static int negotiate_with_new_client(IPC_client_t *client);
static void ipc_client_close(IPC_client_t *client, int epoll_fd);

int xab_ipc_capabilities(void) {
    return IpcXabCapabilitiesNone
#ifdef HAVE_LIBXRANDR
           | IpcXabCapabilitiesMultimonitor
#endif
        ;
}

IPC_handle_t ipc_init(const char *name) {
    Assert(name != NULL);
    IPC_handle_t handle = {
        .server_fd = -1,
        .clients = NULL,
        .client_count = 0,
        .path = name,
        .server_addr = {0},
        .epoll_fd = 0,
    };

    // create socket
    handle.server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    // set address
    handle.server_addr.sun_family = AF_UNIX;
    strncpy(handle.server_addr.sun_path, handle.path,
            sizeof(handle.server_addr.sun_path) - 1);

    struct stat st = {0};
    xab_log(LOG_DEBUG, "Creating IPC directory if missing: `%s`\n", IPC_DIR);
    if (stat("/tmp/xab", &st) == -1) {
        if (mkdir(IPC_DIR, 1777) < 0)
            xab_log(LOG_ERROR, "Failed creating IPC directory: `%s`\n",
                    IPC_DIR);
    }

    // bind socket
    if (bind(handle.server_fd, (struct sockaddr *)&handle.server_addr,
             sizeof(handle.server_addr)) < 0) {
        xab_log(LOG_ERROR, "Failed binding to unix domain socket: `%s`\n",
                handle.path);
        // TODO: handle error
    }

    // listen for clients
    xab_log(LOG_INFO, "Listening for unix domian socket clients...\n");
    if (listen(handle.server_fd, 5) < 0) {
        xab_log(LOG_ERROR,
                "An error occured while listening to incoming connections\n");
        // TODO: handle error
    }

    // edge-triggered epoll and stuff
    if ((handle.epoll_fd = epoll_create1(0)) < 0) {
        xab_log(LOG_ERROR, "Failed to create epoll!\n");
        // TODO: handle the error
    }

    struct epoll_event events = {
        .events = EPOLLIN,
        .data.fd = handle.server_fd,
    };

    if (epoll_ctl(handle.epoll_fd, EPOLL_CTL_ADD, handle.server_fd, &events)) {
        xab_log(LOG_ERROR, "epoll_ctl EPOLL_CTL_ADD failed!\n");
        // TODO: handle error
    }

    // set the epoll and server file descriptors to non-blocknig
    set_nonblocking(handle.epoll_fd);
    set_nonblocking(handle.server_fd);

    return handle;
}

static int negotiate_with_new_client(IPC_client_t *client) {
    Assert(client != NULL && "Client pointer is NULL!");

    // send xab IPC protocl version
    {
        xab_log(LOG_DEBUG, "Sending xab IPC protocol version\n");
        int ipc_proto_version = htonl(IPC_PROTO_VERSION);
        if (send(client->fd, &ipc_proto_version, sizeof(ipc_proto_version), 0) <
            0) {
            xab_log(LOG_ERROR, "An error occured while sending the xab IPC "
                               "protocol version to the IPC client!\n");
            // TODO: handle error
        }
    }

    // verify client version matches - disconnect if it doesn't
    {
        int client_version = -1;
        Assert(sizeof(client_version) != sizeof(ipc_proto_version));
        xab_log(LOG_DEBUG, "Waiting for client version...\n");
        if (recv_exact(client->fd, &client_version, sizeof(client_version), 0) <
            0) {
            xab_log(LOG_ERROR, "An error occured while recieving the client's "
                               "xab IPC protocol version\n");
            // TODO: handle error
        }
        client_version = ntohl(client_version);

        // if version is mismatched then disconnect from the client
        if (client_version != IPC_PROTO_VERSION) {
            xab_log(LOG_ERROR,
                    "Mismatch between client and server xab IPC protocol "
                    "version! (server: v%d | client: v%d)\n",
                    IPC_PROTO_VERSION, client_version);
            ipc_client_close(client,
                             -1); // use -1 as epoll fd cuz we the client's not
                                  // in the epoll yet
            return -1;
        } else
            xab_log(LOG_DEBUG, "IPC protocol versions match! (v%d)\n",
                    IPC_PROTO_VERSION);
    }

    // send capabilities
    {
        int capabilities = htonl(xab_ipc_capabilities());
        send(client->fd, &capabilities, sizeof(capabilities), 0);
    }

    return 0;
}

void ipc_poll_events(IPC_handle_t *handle, context_t *context) {
    Assert(handle != NULL && "IPC handle pointer is NULL!");
    struct epoll_event events[8] = {0};
    int n = epoll_wait(handle->epoll_fd, events, 8, 0);
    if (n > 0)
        xab_log(LOG_VERBOSE, "%d epoll events ready\n", n);
    for (int i = 0; i < n; i++) {
        struct epoll_event event = events[i];
        if (event.data.fd == handle->server_fd) {
            // accept client connections
            int client_fd = -1;
            xab_log(LOG_DEBUG, "Accepting IPC client connections...\n");
            while ((client_fd = accept(handle->server_fd, NULL, NULL)) >= 0) {
                // allocate space for client
                if (handle->clients == NULL)
                    handle->clients = calloc(1, (++handle->client_count) *
                                                    sizeof(IPC_client_t));
                else {
                    handle->clients =
                        realloc(handle->clients, (++handle->client_count) *
                                                     sizeof(IPC_client_t));
                    Assert(handle->clients != NULL &&
                           "realloc failed miserably!");
                    // zero out the new memory
                    memset(&handle->clients[handle->client_count - 1], 0,
                           sizeof(IPC_client_t));
                }

                Assert(client_fd >= 0 && "Invalid client fd!\n");

                // copy over the client
                IPC_client_t *client =
                    &handle->clients[handle->client_count - 1];
                client->fd = client_fd;
                if (negotiate_with_new_client(client) < 0) {
                    memset(&handle->clients[handle->client_count - 1], 0,
                           sizeof(IPC_client_t)); // TODO: proper memory sizing
                    continue;
                }

                struct epoll_event events = {
                    .events = EPOLLIN,
                    .data.fd = client->fd,
                };

                if (epoll_ctl(handle->epoll_fd, EPOLL_CTL_ADD, client->fd,
                              &events)) {
                    xab_log(LOG_ERROR, "epoll_ctl EPOLL_CTL_ADD failed!\n");
                    // TODO: handle error
                }

                xab_log(LOG_INFO, "Connected to client (fd %d)\n", client->fd);
            }

            if (client_fd == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                xab_log(LOG_ERROR,
                        "An error occured while accepting a "
                        "client connection with errno: `%d`\n",
                        errno);
            }
        } else {
            // check each client
            // TODO: use a hasmap instead
            Assert(handle->clients != NULL && "clients pointer is NULL!");
            for (int i = 0; i < handle->client_count; i++) {
                IPC_client_t *client = &handle->clients[i];

                if (event.data.fd == client->fd) {
                    IpcCommands_e data = IPC_NONE;
                    ssize_t n = recv_exact(client->fd, &data, sizeof(data),
                                           MSG_DONTWAIT);
                    if (n == 0)
                        ipc_client_close(client, handle->epoll_fd);
                    else if (n < 0) {
                        // if the error is not relating to MSG_DONTWAIT
                        if (errno != EAGAIN && errno != EWOULDBLOCK) {
                            xab_log(
                                LOG_WARN,
                                "Recieved a packet errored with errno: `%d`\n",
                                errno);
                        }
                    } else if (n != sizeof(data)) {
                        xab_log(LOG_DEBUG,
                                "Recieved partial packet, skipping for now\n");
                        return;
                    }

                    // remember kids! never forget to ntohl your recvs!
                    data = ntohl(data);

                    switch (data) {
                    default:
                        xab_log(LOG_WARN, "Invalid IPC command from client!\n");
                        break;
                        // im too lazy to send an invalid back to the client
                    case IPC_INVALID:
                        break;
                    case IPC_NONE:
                        break;
                    case IPC_RESTART:
                        // TODO: this
                        break;
                    case IPC_XAB_SHUTDOWN:
                        // TODO: this
                        break;
                    case IPC_CLIENT_DISCONNECT:
                        xab_log(LOG_INFO, "Client %d issued a disconnect...\n",
                                client->fd);
                        ipc_client_close(client, handle->epoll_fd);
                        break;
                    case IPC_CHANGE_BACKGROUNDS:
                        // TODO: this
                        break;
                    case IPC_GET_MONITORS:
                        // TODO: this
                        break;
                    }
                }
            }
        }
    }
}

static void ipc_client_close(IPC_client_t *client, int epoll_fd) {
    Assert(client != NULL && "Client pointer is NULL!");

    // close client fd
    if (client->fd >= 0) {
        // remove client fd from epoll
        if (epoll_fd >= 0) {
            xab_log(LOG_TRACE, "Deleting client from epoll\n");
            if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL)) {
                xab_log(LOG_ERROR, "epoll_ctl EPOLL_CTL_DEL failed!\n");
                // TODO: handle error
            }
        }

        xab_log(LOG_TRACE, "Closing client FD %d\n", client->fd);
        if (shutdown(client->fd, SHUT_RDWR) < 0)
            xab_log(LOG_ERROR, "Shutting down client FD failed\n");
        if (close(client->fd) < 0) {
            xab_log(LOG_ERROR, "Failed closing IPC client FD %d\n", client->fd);
        }
        client->fd = -1;
    }
}

void ipc_close(IPC_handle_t *handle) {
    Assert(handle != NULL);

    xab_log(LOG_DEBUG, "Closing IPC connection/s...\n");

    // close epoll fd
    if (handle->epoll_fd >= 0) {
        if (close(handle->epoll_fd) < 0) {
            xab_log(LOG_ERROR, "Failed closing IPC epoll FD %d\n",
                    handle->epoll_fd);
        }
        handle->epoll_fd = -1;
    }

    // close client fds
    if (handle->client_count > 0 && handle->clients != NULL)
        for (int i = 0; i < handle->client_count; i++)
            ipc_client_close(&handle->clients[i], handle->epoll_fd);
    if (handle->clients) {
        xab_log(LOG_TRACE, "Freeing clients' memeory\n");
        free(handle->clients);
        handle->clients = NULL;
        handle->client_count = 0;
    }

    // close server fd
    if (handle->server_fd >= 0) {
        xab_log(LOG_TRACE, "Closing server FD\n");
        if (shutdown(handle->server_fd, SHUT_RDWR) < 0)
            xab_log(LOG_ERROR, "Shutting down server FD failed\n");
        if (close(handle->server_fd) < 0) {
            xab_log(LOG_ERROR, "Failed closing IPC server FD %d\n",
                    handle->server_fd);
        }
        handle->server_fd = -1;
    }

    // unlink the socket file
    if (handle->path) {
        xab_log(LOG_TRACE, "Unlinking socket file\n");
        unlink(handle->server_addr.sun_path);
        handle->path = NULL;
    }

    xab_log(LOG_DEBUG, "IPC connection has shut down gracefully\n");
}

static int set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        xab_log(LOG_ERROR, "fcntl(F_GETFL) returned -1\n");
        return -1;
    }
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        xab_log(LOG_ERROR, "fcntl(F_SETFL) returned -1\n");
        return -1;
    }
    return 0;
}
