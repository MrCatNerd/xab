#include "ipc.h"
#include "ipc_spec.h"

#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <hashmap.h>

#include "context.h"
#include "logger.h"
#include "ipc_utils.h"
#include "utils.h"
#include "tracy.h"

static int set_nonblocking(int sockfd);
static int negotiate_with_new_client(IPC_client_t *client);
static void ipc_client_close(const IPC_client_t *client, int epoll_fd);

static int client_compare(const void *a, const void *b, void *udata) {
    (void)udata;
    const IPC_client_t *ca = a;
    const IPC_client_t *cb = b;
    // -1,0,1 for strcmp styled return
    return (ca->fd > cb->fd) - (ca->fd < cb->fd);
}

static uint64_t client_hash(const void *item, uint64_t seed0, uint64_t seed1) {
    const IPC_client_t *client = item;
    return hashmap_sip(&client->fd, sizeof(client->fd), seed0, seed1);
}

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
        .clients = hashmap_new(sizeof(IPC_client_t), 0, 0, 0, client_hash,
                               client_compare, NULL, NULL),
        .path = name,
        .server_addr = {0},
        .epoll_fd = 0,
    };

    Assert(handle.clients != NULL && "Failed to create clients hashmap");

    // create socket
    handle.server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    // set address
    handle.server_addr.sun_family = AF_UNIX;
    strncpy(handle.server_addr.sun_path, handle.path,
            sizeof(handle.server_addr.sun_path) - 1);

    struct stat st = {0};
    xab_log(LOG_DEBUG, "Creating directory if missing: `%s`\n", IPC_DIR);
    if (stat(IPC_DIR, &st) == -1) {
        if (mkdir(IPC_DIR, 1777) < 0)
            xab_log(LOG_ERROR, "Failed creating directory: `%s`\n", IPC_DIR);
    }

    // bind socket
    if (bind(handle.server_fd, (struct sockaddr *)&handle.server_addr,
             sizeof(handle.server_addr)) < 0) {
        xab_log(LOG_ERROR, "Failed binding to unix domain socket: `%s`\n",
                handle.path);
        // TODO: handle error
    }

    // listen for clients
    xab_log(LOG_INFO, "Listening for unix domain socket clients...\n");
    if (listen(handle.server_fd, 5) < 0) {
        xab_log(LOG_ERROR,
                "An error occurred while listening to incoming connections\n");
        // TODO: handle error
    }

    // epoll and stuff
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

    // set the server file descriptor to non-blocknig
    set_nonblocking(handle.server_fd);

    return handle;
}

static int negotiate_with_new_client(IPC_client_t *client) {
    Assert(client != NULL && "Client pointer is NULL!");

    // send xab IPC protocl version
    xab_log(LOG_DEBUG, "Sending xab IPC protocol version\n");
    const int ipc_proto_version = htonl(IPC_PROTO_VERSION);
    if (send_all(client->fd, &ipc_proto_version, sizeof(ipc_proto_version), 0) <
        0) {
        xab_log(LOG_ERROR, "An error occured while sending the xab IPC "
                           "protocol version to the IPC client!\n");
        // TODO: handle error
    }

    // verify client version matches - disconnect if it doesn't
    {
        int client_version = -1;
        Assert(sizeof(client_version) == sizeof(ipc_proto_version));
        xab_log(LOG_DEBUG, "Waiting for client version...\n");
        if (recv_exact(client->fd, &client_version, sizeof(client_version), 0) <
            0) {
            xab_log(LOG_ERROR, "An error occured while recieving the client's "
                               "xab IPC protocol version\n");
            ipc_client_close(client,
                             -1); // use -1 as epoll fd cuz we the client's not
                                  // in the epoll yet
            return -1;
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
        send_all(client->fd, &capabilities, sizeof(capabilities), 0);
    }

    return 0;
}

void ipc_poll_events(IPC_handle_t *handle, context_t *context) {
    TracyCZoneNC(tracy_ctx, "IPC event poll", TRACY_COLOR_BLUE, true);
    Assert(handle != NULL && "IPC handle pointer is NULL!");
    struct epoll_event events[8] = {0};
    const int n = epoll_wait(handle->epoll_fd, events, 8, 0);
    if (n > 0)
        xab_log(LOG_VERBOSE, "%d epoll events ready\n", n);
    for (int i = 0; i < n; i++) {
        struct epoll_event event = events[i];
        if (event.data.fd == handle->server_fd) {
            // accept client connections
            int client_fd = -1;
            xab_log(LOG_DEBUG, "Accepting IPC client connections...\n");
            while ((client_fd = accept(handle->server_fd, NULL, NULL)) >= 0) {
                Assert(client_fd >= 0 && "Invalid client fd!\n");
                // set the client file descriptor to non-blocknig
                set_nonblocking(client_fd);

                // copy over the client
                IPC_client_t client = {.fd = client_fd};
                {
                    const IPC_client_t *ret =
                        hashmap_set(handle->clients, &client);
                    if (ret) {
                        Assert(ret->fd == client.fd &&
                               "Hashmap set got clients with different FD's "
                               "replaced");
                        xab_log(LOG_WARN,
                                "Client connection %d got reset in the hashmap",
                                client.fd);
                    }
                    if (hashmap_oom(handle->clients))
                        xab_log(LOG_ERROR,
                                "IPC clients hashmap Out-Of-Memory!\n");
                }
                if (negotiate_with_new_client(&client) < 0) {
                    hashmap_delete(handle->clients, &client);
                    continue;
                }

                struct epoll_event events = {
                    .events = EPOLLIN,
                    .data.fd = client.fd,
                };

                if (epoll_ctl(handle->epoll_fd, EPOLL_CTL_ADD, client.fd,
                              &events)) {
                    xab_log(LOG_ERROR, "epoll_ctl EPOLL_CTL_ADD failed!\n");
                    // TODO: handle error
                }

                xab_log(LOG_INFO, "Connected to client (fd %d)\n", client.fd);
            }

            if (client_fd == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                xab_log(LOG_ERROR,
                        "An error occured while accepting a "
                        "client connection with errno: `%d`\n",
                        errno);
            }
        } else {
            // check the clients
            Assert(handle->clients != NULL &&
                   "clients hashmap pointer is NULL!");
            const IPC_client_t *client = hashmap_get(
                handle->clients, &(IPC_client_t){.fd = event.data.fd});
            if (client == NULL)
                continue;
            IpcCommands_e data = IPC_NONE;
            ssize_t n =
                recv_exact(client->fd, &data, sizeof(data), MSG_DONTWAIT);
            if (n == 0) {
                ipc_client_close(client, handle->epoll_fd);
                hashmap_delete(handle->clients, client);
            } else if (n < 0) {
                // if the error is not relating to MSG_DONTWAIT
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    xab_log(LOG_WARN,
                            "Recieved a packet errored with errno: `%d`\n",
                            errno);
                }
            } else if (n != sizeof(data)) {
                xab_log(LOG_DEBUG,
                        "Recieved partial packet, skipping for now\n");
                continue;
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
                hashmap_delete(handle->clients, client);
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
    TracyCZoneEnd(tracy_ctx);
}

static void ipc_client_close(const IPC_client_t *client, int epoll_fd) {
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

        xab_log(LOG_INFO, "Closing client FD %d\n", client->fd);
        if (shutdown(client->fd, SHUT_RDWR) < 0)
            xab_log(LOG_ERROR, "Shutting down client FD failed\n");
        if (close(client->fd) < 0) {
            xab_log(LOG_ERROR, "Failed closing IPC client FD %d\n", client->fd);
        }
    }
}

static bool client_hashmap_disconnect(const void *item, void *udata) {
    if (!udata)
        return false;
    const int epoll_fd = *(int *)udata;
    const IPC_client_t *client = item;
    ipc_client_close(client, epoll_fd);
    return true;
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
    if (handle->clients) {
        hashmap_scan(handle->clients, client_hashmap_disconnect,
                     &handle->epoll_fd);
        hashmap_free(handle->clients);
        handle->clients = NULL;
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

    xab_log(LOG_DEBUG, "IPC handle has shut down gracefully\n");
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
