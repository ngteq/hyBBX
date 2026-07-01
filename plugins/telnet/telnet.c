/*
 * telnet — TCP transport plugin (port 2323). INI: [transport.telnet].
 */
#include "hybbx/plugin.h"
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "hybbx/telnet.h"
#include "telnet_proto.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct telnet_client {
    int fd;
    hybbx_telnet_parser_t parser;
} telnet_client_t;

typedef struct telnet_client_ctx {
    hybbx_session_t *session;
    hybbx_result_t last_rc;
} telnet_client_ctx_t;

extern const hybbx_transport_plugin_t hybbx_plugin_telnet;

static hybbx_service_t *g_service;
static hybbx_telnet_config_t g_config;
static pthread_t g_accept_thread;
static int g_listen_v4 = -1;
static int g_listen_v6 = -1;
static volatile int g_telnet_running = 0;

static hybbx_result_t telnet_stop(void);

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

    if (listen(fd, 16) != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static hybbx_result_t telnet_send_byte(int fd, char ch)
{
    ssize_t sent;
    const char *p = &ch;

    sent = send(fd, p, 1, MSG_NOSIGNAL);
    if (sent < 0) {
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}

static hybbx_result_t telnet_write(hybbx_session_t *session,
                                   const char *data, size_t len)
{
    telnet_client_t *client;
    size_t i;
    hybbx_result_t rc;

    if (session == NULL || data == NULL) {
        return HYBBX_ERR_INVALID;
    }

    client = (telnet_client_t *)session->transport_data;
    if (client == NULL || client->fd < 0) {
        return HYBBX_ERR_INVALID;
    }

    for (i = 0; i < len; i++) {
        char ch = data[i];

        if (ch == '\n' && (i == 0 || data[i - 1] != '\r')) {
            rc = telnet_send_byte(client->fd, '\r');
            if (rc != HYBBX_OK) {
                return rc;
            }
        }

        {
            ssize_t sent = send(client->fd, &ch, 1, MSG_NOSIGNAL);

            if (sent < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return HYBBX_ERR_IO;
            }
            if (sent == 0) {
                return HYBBX_ERR_IO;
            }
        }
    }

    return HYBBX_OK;
}

static void telnet_on_user_data(void *ctx, const uint8_t *data, size_t len)
{
    telnet_client_ctx_t *tctx = (telnet_client_ctx_t *)ctx;
    hybbx_result_t rc;

    if (tctx == NULL || tctx->session == NULL || data == NULL || len == 0) {
        return;
    }

    rc = hybbx_session_handle_input(tctx->session, data, len);
    if (rc != HYBBX_OK) {
        tctx->last_rc = rc;
    }
}

#define TELNET_CLIENT_POLL_MS 30000

static void telnet_send_busy(int fd)
{
    static const char msg[] = "All nodes in use. Try later.\r\n";

    if (fd < 0) {
        return;
    }

    (void)send(fd, msg, sizeof(msg) - 1u, 0);
}

static void *telnet_client_thread(void *arg)
{
    telnet_client_t *client = (telnet_client_t *)arg;
    hybbx_session_t *session = NULL;
    telnet_client_ctx_t tctx;
    uint8_t buf[256];
    ssize_t n;
    hybbx_result_t rc;

    if (client == NULL || g_service == NULL) {
        free(client);
        return NULL;
    }

    (void)set_socket_options(client->fd);

    rc = hybbx_telnet_send_greeting(client->fd);
    if (rc != HYBBX_OK) {
        close(client->fd);
        free(client);
        return NULL;
    }

    rc = hybbx_session_open(g_service, &hybbx_plugin_telnet, client, &session);
    if (rc != HYBBX_OK || session == NULL) {
        if (rc == HYBBX_ERR_BUSY) {
            telnet_send_busy(client->fd);
        }
        close(client->fd);
        free(client);
        return NULL;
    }

    hybbx_telnet_parser_init(&client->parser);
    tctx.session = session;
    tctx.last_rc = HYBBX_OK;

    while (g_telnet_running) {
        struct pollfd pfd;
        int pr;

        pfd.fd = client->fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        pr = poll(&pfd, 1, TELNET_CLIENT_POLL_MS);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (pr == 0) {
            rc = hybbx_session_tick(session);
            if (rc == HYBBX_SESSION_END) {
                tctx.last_rc = HYBBX_SESSION_END;
                break;
            }
            continue;
        }
        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            break;
        }
        if ((pfd.revents & POLLIN) == 0) {
            continue;
        }

        n = recv(client->fd, buf, sizeof(buf), 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (n == 0) {
            break;
        }

        rc = hybbx_telnet_parser_feed(&client->parser, client->fd, buf, (size_t)n,
                                      telnet_on_user_data, &tctx);
        if (rc != HYBBX_OK) {
            break;
        }

        if (tctx.last_rc == HYBBX_SESSION_END) {
            break;
        }
    }

    hybbx_session_close(session);
    shutdown(client->fd, SHUT_RDWR);
    close(client->fd);
    free(client);
    return NULL;
}

static void accept_client(int client_fd)
{
    telnet_client_t *client;
    pthread_t thread;
    pthread_attr_t attr;

    client = calloc(1, sizeof(*client));
    if (client == NULL) {
        close(client_fd);
        return;
    }

    client->fd = client_fd;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&thread, &attr, telnet_client_thread, client) != 0) {
        close(client_fd);
        free(client);
    }

    pthread_attr_destroy(&attr);
}

static void *telnet_accept_thread(void *arg)
{
    (void)arg;

    while (g_telnet_running) {
        struct pollfd fds[2];
        nfds_t nfds = 0;
        int i;
        int ready;

        memset(fds, 0, sizeof(fds));

        if (g_listen_v4 >= 0) {
            fds[nfds].fd = g_listen_v4;
            fds[nfds].events = POLLIN;
            nfds++;
        }

        if (g_listen_v6 >= 0) {
            fds[nfds].fd = g_listen_v6;
            fds[nfds].events = POLLIN;
            nfds++;
        }

        if (nfds == 0) {
            break;
        }

        ready = poll(fds, nfds, 500);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (ready == 0) {
            continue;
        }

        for (i = 0; i < (int)nfds; i++) {
            if (fds[i].revents & POLLIN) {
                int client_fd = accept(fds[i].fd, NULL, NULL);

                if (client_fd >= 0) {
                    accept_client(client_fd);
                }
            }
        }
    }

    return NULL;
}

static hybbx_result_t telnet_init(hybbx_service_t *service)
{
    g_service = service;
    return HYBBX_OK;
}

static void telnet_shutdown(void)
{
    telnet_stop();
}

static hybbx_result_t telnet_start(const char *config)
{
    hybbx_result_t rc;

    if (g_telnet_running) {
        return HYBBX_ERR_BUSY;
    }

    rc = hybbx_telnet_config_parse(config, &g_config);
    if (rc != HYBBX_OK) {
        return rc;
    }

    g_listen_v4 = -1;
    g_listen_v6 = -1;

    if (g_config.ipv4) {
        g_listen_v4 = create_listen_socket(AF_INET, g_config.bind_v4, g_config.port);
        if (g_listen_v4 < 0) {
            fprintf(stderr, "[telnet] failed to bind IPv4 %s:%u (%s)\n",
                    g_config.bind_v4, g_config.port, strerror(errno));
            return HYBBX_ERR_IO;
        }
    }

    if (g_config.ipv6) {
        g_listen_v6 = create_listen_socket(AF_INET6, g_config.bind_v6, g_config.port);
        if (g_listen_v6 < 0) {
            fprintf(stderr, "[telnet] failed to bind IPv6 [%s]:%u (%s)\n",
                    g_config.bind_v6, g_config.port, strerror(errno));
            if (g_listen_v4 >= 0) {
                close(g_listen_v4);
                g_listen_v4 = -1;
            }
            return HYBBX_ERR_IO;
        }
    }

    if (g_listen_v4 < 0 && g_listen_v6 < 0) {
        return HYBBX_ERR_INVALID;
    }

    g_telnet_running = 1;

    if (pthread_create(&g_accept_thread, NULL, telnet_accept_thread, NULL) != 0) {
        g_telnet_running = 0;
        if (g_listen_v4 >= 0) {
            close(g_listen_v4);
            g_listen_v4 = -1;
        }
        if (g_listen_v6 >= 0) {
            close(g_listen_v6);
            g_listen_v6 = -1;
        }
        return HYBBX_ERR_IO;
    }

    printf("[telnet] listening");
    if (g_listen_v4 >= 0) {
        printf(" IPv4 %s:%u", g_config.bind_v4, g_config.port);
    }
    if (g_listen_v6 >= 0) {
        printf(" IPv6 [%s]:%u", g_config.bind_v6, g_config.port);
    }
    printf("\n");

    return HYBBX_OK;
}

static hybbx_result_t telnet_stop(void)
{
    if (!g_telnet_running) {
        return HYBBX_OK;
    }

    g_telnet_running = 0;

    if (g_listen_v4 >= 0) {
        shutdown(g_listen_v4, SHUT_RDWR);
        close(g_listen_v4);
        g_listen_v4 = -1;
    }

    if (g_listen_v6 >= 0) {
        shutdown(g_listen_v6, SHUT_RDWR);
        close(g_listen_v6);
        g_listen_v6 = -1;
    }

    pthread_join(g_accept_thread, NULL);
    printf("[telnet] stop\n");
    return HYBBX_OK;
}

const hybbx_transport_plugin_t hybbx_plugin_telnet = {
    .name = "telnet",
    .kind = HYBBX_TRANSPORT_TELNET,
    .version = 1,
    .init = telnet_init,
    .shutdown = telnet_shutdown,
    .start = telnet_start,
    .stop = telnet_stop,
    .write = telnet_write,
};
