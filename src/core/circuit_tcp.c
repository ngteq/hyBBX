#if defined(__linux__) || defined(__GLIBC__)
#define _DEFAULT_SOURCE 1
#endif

#include "hybbx/circuit_tcp.h"
#include "hybbx/session.h"
#include "hybbx/traffic.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HYBBX_CIRCUIT_LINK_POLL_MS 50

struct hybbx_circuit_hub {
    hybbx_service_t *service;
    hybbx_circuit_config_t config;
    int listen_v4;
    int listen_v6;
    int link_fd;
    pthread_t accept_thread;
    pthread_t link_thread;
    pthread_mutex_t lock;
    volatile int running;
    hybbx_session_t *session;
    hybbx_circuit_decoder_t decoder;
};

static hybbx_circuit_hub_t *g_active_hub;

void hybbx_circuit_config_defaults(hybbx_circuit_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->bind4, sizeof(cfg->bind4), "127.0.0.1");
    snprintf(cfg->bind6, sizeof(cfg->bind6), "::1");
    cfg->port = HYBBX_CIRCUIT_DEFAULT_PORT;
    cfg->ipv4 = 1;
    cfg->ipv6 = 1;
}

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

static int create_listen_socket(int family, const char *bind_addr, unsigned port)
{
    int fd;
    int rc;

    fd = socket(family, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    if (set_socket_options(fd) != 0) {
        close(fd);
        return -1;
    }

    if (family == AF_INET6) {
        struct sockaddr_in6 addr6;

        memset(&addr6, 0, sizeof(addr6));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons((uint16_t)port);

        if (inet_pton(AF_INET6, bind_addr, &addr6.sin6_addr) != 1) {
            close(fd);
            return -1;
        }

        rc = bind(fd, (struct sockaddr *)&addr6, sizeof(addr6));
    } else {
        struct sockaddr_in addr4;

        memset(&addr4, 0, sizeof(addr4));
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons((uint16_t)port);

        if (inet_pton(AF_INET, bind_addr, &addr4.sin_addr) != 1) {
            close(fd);
            return -1;
        }

        rc = bind(fd, (struct sockaddr *)&addr4, sizeof(addr4));
    }

    if (rc != 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 4) != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

hybbx_result_t hybbx_circuit_hub_send_raw(hybbx_circuit_hub_t *hub,
                                          const uint8_t *frame, size_t len)
{
    ssize_t sent;
    size_t off = 0;

    if (hub == NULL || frame == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    pthread_mutex_lock(&hub->lock);
    if (hub->link_fd < 0) {
        pthread_mutex_unlock(&hub->lock);
        return HYBBX_ERR_BUSY;
    }

    while (off < len) {
        sent = send(hub->link_fd, frame + off, len - off, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            pthread_mutex_unlock(&hub->lock);
            return HYBBX_ERR_IO;
        }
        if (sent == 0) {
            pthread_mutex_unlock(&hub->lock);
            return HYBBX_ERR_IO;
        }
        off += (size_t)sent;
    }

    pthread_mutex_unlock(&hub->lock);
    return HYBBX_OK;
}

static hybbx_result_t circuit_transport_write(hybbx_session_t *session,
                                              const char *data, size_t len)
{
    hybbx_circuit_hub_t *hub;
    uint8_t frame[HYBBX_CIRCUIT_MAX_FRAME];
    size_t frame_len;

    if (session == NULL || data == NULL || len == 0) {
        return HYBBX_OK;
    }

    hub = (hybbx_circuit_hub_t *)session->transport_data;
    if (hub == NULL) {
        hub = g_active_hub;
    }
    if (hub == NULL) {
        return HYBBX_ERR_INVALID;
    }

    frame_len = hybbx_circuit_encode_terminal(data, len, frame, sizeof(frame));
    if (frame_len == 0) {
        return HYBBX_ERR_IO;
    }

    return hybbx_circuit_hub_send_raw(hub, frame, frame_len);
}

static void on_circuit_frame(hybbx_circuit_proto_t proto, uint16_t flags,
                             const uint8_t *payload, size_t len,
                             void *userdata)
{
    hybbx_circuit_hub_t *hub = (hybbx_circuit_hub_t *)userdata;
    uint8_t ui[HYBBX_AX25_PAYLOAD_MAX];
    hybbx_ax25_path_t path;

    (void)flags;

    if (hub == NULL || hub->session == NULL || len == 0) {
        return;
    }

    switch (proto) {
    case HYBBX_CIRCUIT_PROTO_AX25: {
        size_t ui_len = hybbx_ax25_parse_ui(payload, len, &path, ui,
                                            sizeof(ui));
        if (ui_len > 0) {
            (void)hybbx_session_handle_input(hub->session, ui, ui_len);
        }
        break;
    }
    case HYBBX_CIRCUIT_PROTO_AX25_UI: {
        size_t ui_len = hybbx_circuit_unpack_ax25_ui(payload, len, &path,
                                                      ui, sizeof(ui));
        if (ui_len > 0) {
            (void)hybbx_session_handle_input(hub->session, ui, ui_len);
        }
        break;
    }
    case HYBBX_CIRCUIT_PROTO_TERMINAL:
        (void)hybbx_session_handle_input(hub->session, payload, len);
        break;
    default:
        printf("[circuit] ignored proto=%s (%u bytes)\n",
               hybbx_circuit_proto_name(proto), (unsigned)len);
        break;
    }
}

static void circuit_close_link(hybbx_circuit_hub_t *hub)
{
    if (hub == NULL) {
        return;
    }

    pthread_mutex_lock(&hub->lock);
    if (hub->link_fd >= 0) {
        close(hub->link_fd);
        hub->link_fd = -1;
    }
    pthread_mutex_unlock(&hub->lock);

    if (hub->session != NULL) {
        hybbx_session_close(hub->session);
        hub->session = NULL;
    }
}

static void *circuit_link_thread(void *arg)
{
    hybbx_circuit_hub_t *hub = (hybbx_circuit_hub_t *)arg;
    uint8_t buf[512];
    hybbx_result_t rc;

    if (hub == NULL) {
        return NULL;
    }

    hybbx_circuit_decoder_init(&hub->decoder);

    rc = hybbx_session_open(hub->service, &hybbx_plugin_circuit, hub,
                            &hub->session);
    if (rc != HYBBX_OK) {
        fprintf(stderr, "[circuit] session open failed\n");
        circuit_close_link(hub);
        return NULL;
    }

    printf("[circuit] link adapter attached (HBX bridge active)\n");

    while (hub->running) {
        struct pollfd pfd;
        ssize_t n;
        int pr;

        pthread_mutex_lock(&hub->lock);
        pfd.fd = hub->link_fd;
        pthread_mutex_unlock(&hub->lock);

        if (pfd.fd < 0) {
            break;
        }

        pfd.events = POLLIN;
        pfd.revents = 0;
        pr = poll(&pfd, 1, HYBBX_CIRCUIT_LINK_POLL_MS);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (pr == 0) {
            if (hub->session != NULL) {
                rc = hybbx_session_tick(hub->session);
                if (rc == HYBBX_SESSION_END) {
                    break;
                }
            }
            continue;
        }
        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            break;
        }
        if ((pfd.revents & POLLIN) == 0) {
            continue;
        }

        n = recv(pfd.fd, buf, sizeof(buf), 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (n == 0) {
            break;
        }

        hybbx_circuit_decoder_feed(&hub->decoder, buf, (size_t)n,
                                   on_circuit_frame, hub);
    }

    circuit_close_link(hub);
    return NULL;
}

static void *circuit_accept_thread(void *arg)
{
    hybbx_circuit_hub_t *hub = (hybbx_circuit_hub_t *)arg;

    while (hub->running) {
        struct pollfd pfds[2];
        int count = 0;
        int pr;
        int i;

        pfds[0].fd = hub->listen_v4;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        if (hub->listen_v4 >= 0) {
            count = 1;
        }

        pfds[1].fd = hub->listen_v6;
        pfds[1].events = POLLIN;
        pfds[1].revents = 0;
        if (hub->listen_v6 >= 0) {
            count = 2;
        }

        if (count == 0) {
            break;
        }

        pr = poll(pfds, (nfds_t)count, HYBBX_CIRCUIT_LINK_POLL_MS);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (pr == 0) {
            continue;
        }

        for (i = 0; i < count; i++) {
            if ((pfds[i].revents & POLLIN) == 0) {
                continue;
            }

            pthread_mutex_lock(&hub->lock);
            if (hub->link_fd >= 0) {
                pthread_mutex_unlock(&hub->lock);
                {
                    int junk = accept(pfds[i].fd, NULL, NULL);
                    if (junk >= 0) {
                        close(junk);
                    }
                }
                continue;
            }
            pthread_mutex_unlock(&hub->lock);

            {
                int client = accept(pfds[i].fd, NULL, NULL);
                if (client < 0) {
                    continue;
                }

                (void)set_socket_options(client);
                pthread_mutex_lock(&hub->lock);
                hub->link_fd = client;
                pthread_mutex_unlock(&hub->lock);

                if (pthread_create(&hub->link_thread, NULL, circuit_link_thread,
                                   hub) != 0) {
                    fprintf(stderr, "[circuit] link thread failed\n");
                    circuit_close_link(hub);
                } else {
                    pthread_detach(hub->link_thread);
                }
            }
        }
    }

    return NULL;
}

hybbx_circuit_hub_t *hybbx_circuit_hub_create(hybbx_service_t *service)
{
    hybbx_circuit_hub_t *hub;

    hub = calloc(1, sizeof(*hub));
    if (hub == NULL) {
        return NULL;
    }

    hub->service = service;
    hub->listen_v4 = -1;
    hub->listen_v6 = -1;
    hub->link_fd = -1;
    pthread_mutex_init(&hub->lock, NULL);
    return hub;
}

void hybbx_circuit_hub_destroy(hybbx_circuit_hub_t *hub)
{
    if (hub == NULL) {
        return;
    }

    hybbx_circuit_hub_stop(hub);
    pthread_mutex_destroy(&hub->lock);
    free(hub);
}

hybbx_result_t hybbx_circuit_hub_start(hybbx_circuit_hub_t *hub,
                                       const hybbx_circuit_config_t *cfg)
{
    if (hub == NULL || cfg == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_circuit_hub_stop(hub);
    hub->config = *cfg;
    hub->running = 1;
    g_active_hub = hub;

    if (cfg->ipv4) {
        hub->listen_v4 = create_listen_socket(AF_INET, cfg->bind4, cfg->port);
        if (hub->listen_v4 < 0) {
            fprintf(stderr, "[circuit] IPv4 bind %s:%u failed\n",
                    cfg->bind4, cfg->port);
            hybbx_circuit_hub_stop(hub);
            return HYBBX_ERR_IO;
        }
    }

    if (cfg->ipv6) {
        hub->listen_v6 = create_listen_socket(AF_INET6, cfg->bind6, cfg->port);
        if (hub->listen_v6 < 0) {
            fprintf(stderr, "[circuit] IPv6 bind [%s]:%u failed\n",
                    cfg->bind6, cfg->port);
            hybbx_circuit_hub_stop(hub);
            return HYBBX_ERR_IO;
        }
    }

    printf("[circuit] internal TCP hub %s:%u / [%s]:%u (HBX v%u)\n",
           cfg->bind4, cfg->port, cfg->bind6, cfg->port,
           (unsigned)HYBBX_CIRCUIT_VERSION);

    if (pthread_create(&hub->accept_thread, NULL, circuit_accept_thread,
                       hub) != 0) {
        hybbx_circuit_hub_stop(hub);
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}

void hybbx_circuit_hub_stop(hybbx_circuit_hub_t *hub)
{
    if (hub == NULL) {
        return;
    }

    hub->running = 0;

    if (hub->listen_v4 >= 0) {
        close(hub->listen_v4);
        hub->listen_v4 = -1;
    }
    if (hub->listen_v6 >= 0) {
        close(hub->listen_v6);
        hub->listen_v6 = -1;
    }

    circuit_close_link(hub);

    if (hub->accept_thread) {
        pthread_join(hub->accept_thread, NULL);
        hub->accept_thread = (pthread_t)0;
    }

    if (g_active_hub == hub) {
        g_active_hub = NULL;
    }
}

int hybbx_circuit_hub_running(const hybbx_circuit_hub_t *hub)
{
    return hub != NULL && hub->running;
}

unsigned hybbx_circuit_hub_port(const hybbx_circuit_hub_t *hub)
{
    if (hub == NULL) {
        return HYBBX_CIRCUIT_DEFAULT_PORT;
    }

    return hub->config.port;
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

static hybbx_result_t circuit_plugin_init(hybbx_service_t *service)
{
    (void)service;
    return HYBBX_OK;
}

static void circuit_plugin_shutdown(void)
{
}

static hybbx_result_t circuit_plugin_start(const char *config)
{
    (void)config;
    return HYBBX_OK;
}

static hybbx_result_t circuit_plugin_stop(void)
{
    return HYBBX_OK;
}

const hybbx_transport_plugin_t hybbx_plugin_circuit = {
    .name = "circuit",
    .kind = HYBBX_TRANSPORT_CIRCUIT,
    .version = 1,
    .init = circuit_plugin_init,
    .shutdown = circuit_plugin_shutdown,
    .start = circuit_plugin_start,
    .stop = circuit_plugin_stop,
    .write = circuit_transport_write,
};
