#if defined(__linux__) || defined(__GLIBC__)
#define _DEFAULT_SOURCE 1
#endif

#include "hybbx/circuit_tcp.h"
#include "hybbx/circuit.h"
#include "hybbx/link.h"
#include "hybbx/util.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define HYBBX_CIRCUIT_LINK_POLL_MS    50
#define HYBBX_CIRCUIT_AUTH_TIMEOUT_MS 15000

static int set_socket_options(int fd)
{
    int on = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0) {
        return -1;
    }

    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) != 0) {
        return -1;
    }

    return 0;
}

hybbx_result_t hybbx_circuit_link_connect(const char *host, unsigned port,
                                          int *out_fd)
{
    struct sockaddr_in6 addr6;
    struct sockaddr_in addr4;
    int fd;
    int rc;

    if (host == NULL || out_fd == NULL || port == 0) {
        return HYBBX_ERR_INVALID;
    }

    if (strchr(host, ':') != NULL) {
        memset(&addr6, 0, sizeof(addr6));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons((uint16_t)port);
        if (inet_pton(AF_INET6, host, &addr6.sin6_addr) != 1) {
            return HYBBX_ERR_INVALID;
        }

        fd = socket(AF_INET6, SOCK_STREAM, 0);
        if (fd < 0) {
            return HYBBX_ERR_IO;
        }

        rc = connect(fd, (struct sockaddr *)&addr6, sizeof(addr6));
    } else {
        memset(&addr4, 0, sizeof(addr4));
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons((uint16_t)port);
        if (inet_pton(AF_INET, host, &addr4.sin_addr) != 1) {
            return HYBBX_ERR_INVALID;
        }

        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) {
            return HYBBX_ERR_IO;
        }

        rc = connect(fd, (struct sockaddr *)&addr4, sizeof(addr4));
    }

    if (rc != 0) {
        close(fd);
        return HYBBX_ERR_IO;
    }

    (void)set_socket_options(fd);
    *out_fd = fd;
    return HYBBX_OK;
}

hybbx_result_t hybbx_circuit_link_write(int fd, const uint8_t *frame,
                                        size_t len)
{
    ssize_t sent;
    size_t off = 0;

    if (fd < 0 || frame == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    while (off < len) {
        sent = send(fd, frame + off, len - off, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return HYBBX_ERR_IO;
        }
        if (sent == 0) {
            return HYBBX_ERR_IO;
        }
        off += (size_t)sent;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_circuit_link_read(int fd, uint8_t *buf, size_t buf_len,
                                       size_t *read_len)
{
    ssize_t n;

    if (fd < 0 || buf == NULL || read_len == NULL) {
        return HYBBX_ERR_INVALID;
    }

    n = recv(fd, buf, buf_len, 0);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *read_len = 0;
            return HYBBX_OK;
        }
        return HYBBX_ERR_IO;
    }

    *read_len = (size_t)n;
    return HYBBX_OK;
}

typedef struct link_ack_wait {
    int done;
    int ok;
} link_ack_wait_t;

static void on_link_auth_ack(hybbx_circuit_proto_t proto, uint16_t flags,
                             const uint8_t *payload, size_t len,
                             void *userdata)
{
    link_ack_wait_t *wait = (link_ack_wait_t *)userdata;
    size_t i;

    (void)flags;

    if (wait == NULL || wait->done || proto != HYBBX_CIRCUIT_PROTO_LINK_AUTH_ACK) {
        return;
    }

    if (len == 0 || payload == NULL) {
        return;
    }

    for (i = 0; i + 6 <= len; i++) {
        if (memcmp(payload + i, "ok=yes", 6) == 0) {
            wait->ok = 1;
            break;
        }
    }
    wait->done = 1;
}

hybbx_result_t hybbx_circuit_link_authenticate(int fd,
                                               const char *password,
                                               const char *role,
                                               const char *id)
{
    hybbx_link_auth_t auth;
    char payload[HYBBX_LINK_AUTH_PAYLOAD_MAX];
    uint8_t frame[HYBBX_CIRCUIT_MAX_FRAME];
    size_t payload_len;
    size_t frame_len;
    hybbx_circuit_decoder_t dec;
    link_ack_wait_t wait;
    uint8_t buf[256];
    unsigned elapsed = 0;

    if (fd < 0 || password == NULL || id == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_link_auth_clear(&auth);
    hybbx_strlcpy(auth.password, password, sizeof(auth.password));
    hybbx_strlcpy(auth.id, id, sizeof(auth.id));
    if (role != NULL && role[0] != '\0') {
        hybbx_strlcpy(auth.role, role, sizeof(auth.role));
    } else {
        hybbx_strlcpy(auth.role, "link", sizeof(auth.role));
    }

    payload_len = hybbx_link_auth_format(&auth, payload, sizeof(payload));
    if (payload_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    frame_len = hybbx_circuit_encode_link_msg(HYBBX_CIRCUIT_PROTO_LINK_AUTH,
                                              payload, payload_len,
                                              frame, sizeof(frame));
    if (frame_len == 0) {
        return HYBBX_ERR_IO;
    }

    if (hybbx_circuit_link_write(fd, frame, frame_len) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    hybbx_circuit_decoder_init(&dec);
    memset(&wait, 0, sizeof(wait));

    while (!wait.done && elapsed < HYBBX_CIRCUIT_AUTH_TIMEOUT_MS) {
        struct pollfd pfd;
        int pr;
        size_t read_len;

        pfd.fd = fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        pr = poll(&pfd, 1, HYBBX_CIRCUIT_LINK_POLL_MS);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            return HYBBX_ERR_IO;
        }
        if (pr == 0) {
            elapsed += HYBBX_CIRCUIT_LINK_POLL_MS;
            continue;
        }
        if ((pfd.revents & POLLIN) == 0) {
            return HYBBX_ERR_IO;
        }

        if (hybbx_circuit_link_read(fd, buf, sizeof(buf), &read_len) != HYBBX_OK) {
            return HYBBX_ERR_IO;
        }
        if (read_len == 0) {
            elapsed += HYBBX_CIRCUIT_LINK_POLL_MS;
            continue;
        }

        hybbx_circuit_decoder_feed(&dec, buf, read_len, on_link_auth_ack, &wait);
    }

    return wait.ok ? HYBBX_OK : HYBBX_ERR_DENIED;
}
