/*
 * websocket — RFC6455 forward-proxy transport. INI: [transport.websocket].
 *
 * Raw byte stream to the session core (no HyBBX auth at the wire layer).
 * Intended behind a TLS reverse proxy; see docs/WEBSOCKET.md.
 */
#include "hybbx/plugin.h"
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "hybbx/socket.h"
#include "hybbx/websocket.h"
#include "hybbx/util.h"
#include "ws_proto.h"
#include "ws_tls.h"

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

#define WS_CLIENT_POLL_MS 30000

typedef struct ws_client {
    hybbx_ws_connection_t ws;
    hybbx_session_t *hbx_session;
    hybbx_result_t last_rc;
} ws_client_t;

typedef struct ws_client_ctx {
    ws_client_t *client;
} ws_client_ctx_t;

extern const hybbx_transport_plugin_t hybbx_plugin_websocket;

static hybbx_service_t *g_service;
static hybbx_websocket_config_t g_config;
static pthread_t g_accept_thread;
static int g_listen_v4 = -1;
static int g_listen_v6 = -1;
static volatile int g_ws_running = 0;

static hybbx_result_t ws_plugin_stop(void);

static int set_socket_options(int fd, int family)
{
    int on = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) != 0) {
        return -1;
    }

    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) != 0) {
        return -1;
    }

    hybbx_socket_nosigpipe(fd);

#ifdef IPV6_V6ONLY
    if (family == AF_INET6) {
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)) != 0) {
            return -1;
        }
    }
#else
    (void)family;
#endif

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

    if (set_socket_options(fd, family) != 0) {
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

static void log_bind_failure(const char *bind_addr, unsigned port)
{
    fprintf(stderr, "[websocket] failed to bind IPv4 %s:%u (%s)\n", bind_addr,
            port, strerror(errno));
    if (errno == EACCES && port < 1024u) {
        fprintf(stderr,
                "[websocket] port %u is privileged — run as root, grant "
                "bind permission, or set port >= 1024 in hybbx.ini\n",
                port);
    }
}

static hybbx_result_t ws_plugin_write(hybbx_session_t *session,
                                      const char *data, size_t len)
{
    ws_client_t *client;
    size_t i;
    char chunk[HYBBX_WS_FRAME_PAYLOAD_MAX];
    size_t chunk_len = 0;
    hybbx_result_t rc;

    if (session == NULL || data == NULL) {
        return HYBBX_ERR_INVALID;
    }

    client = (ws_client_t *)session->transport_data;
    if (client == NULL || !client->ws.established) {
        return HYBBX_ERR_INVALID;
    }

    for (i = 0; i < len; i++) {
        char ch = data[i];

        if (ch == '\n' && (i == 0 || data[i - 1] != '\r')) {
            chunk[chunk_len++] = '\r';
        }
        chunk[chunk_len++] = ch;

        if (chunk_len >= sizeof(chunk) - 2) {
            rc = hybbx_ws_write_text(&client->ws, chunk, chunk_len);
            if (rc != HYBBX_OK) {
                return rc;
            }
            chunk_len = 0;
        }
    }

    if (chunk_len > 0) {
        return hybbx_ws_write_text(&client->ws, chunk, chunk_len);
    }

    return HYBBX_OK;
}

static void ws_send_busy(ws_client_t *client)
{
    static const char msg[] = "All nodes in use. Try later.\r\n";

    if (client == NULL) {
        return;
    }

    (void)hybbx_ws_write_text(&client->ws, msg, sizeof(msg) - 1u);
}

static void ws_on_user_data(void *ctx, const uint8_t *data, size_t len)
{
    ws_client_ctx_t *wctx = (ws_client_ctx_t *)ctx;
    hybbx_result_t rc;

    if (wctx == NULL || wctx->client == NULL ||
        wctx->client->hbx_session == NULL || data == NULL || len == 0) {
        return;
    }

    rc = hybbx_session_handle_input(wctx->client->hbx_session, data, len);
    if (rc != HYBBX_OK) {
        wctx->client->last_rc = rc;
    }
}

static void *ws_client_thread(void *arg)
{
    ws_client_t *client = (ws_client_t *)arg;
    ws_client_ctx_t wctx;
    hybbx_result_t rc;
    char remote[64];

    if (client == NULL || g_service == NULL) {
        free(client);
        return NULL;
    }

    remote[0] = '\0';
    (void)hybbx_socket_peer_name(client->ws.fd, remote, sizeof(remote));

    if (hybbx_ws_tls_server_enabled()) {
        void *tls = hybbx_ws_tls_accept(client->ws.fd);

        if (tls == NULL) {
            goto cleanup;
        }
        hybbx_ws_connection_set_tls(&client->ws, tls);
    }

    rc = hybbx_ws_server_handshake(&client->ws, g_config.path);
    if (rc != HYBBX_OK) {
        goto cleanup;
    }

    rc = hybbx_session_open(g_service, &hybbx_plugin_websocket, client,
                            &client->hbx_session);
    if (rc != HYBBX_OK || client->hbx_session == NULL) {
        if (rc == HYBBX_ERR_BUSY) {
            ws_send_busy(client);
        }
        goto cleanup;
    }

    if (remote[0] != '\0') {
        (void)hybbx_session_set_remote(client->hbx_session, remote);
    }

    wctx.client = client;
    client->last_rc = HYBBX_OK;

    while (g_ws_running && client->ws.established) {
        struct pollfd pfd;
        int pr;

        pfd.fd = client->ws.fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        pr = poll(&pfd, 1, WS_CLIENT_POLL_MS);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (pr == 0) {
            rc = hybbx_session_tick(client->hbx_session);
            if (rc == HYBBX_SESSION_END) {
                client->last_rc = HYBBX_SESSION_END;
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

        rc = hybbx_ws_read_frames(&client->ws, ws_on_user_data, &wctx);
        if (rc != HYBBX_OK) {
            if (client->last_rc == HYBBX_SESSION_END) {
                break;
            }
            break;
        }

        if (client->last_rc == HYBBX_SESSION_END) {
            break;
        }

        rc = hybbx_session_tick(client->hbx_session);
        if (rc == HYBBX_SESSION_END) {
            client->last_rc = HYBBX_SESSION_END;
            break;
        }
    }

cleanup:
    if (client->hbx_session != NULL) {
        hybbx_session_close(client->hbx_session);
        client->hbx_session = NULL;
    }

    if (client->ws.established) {
        (void)hybbx_ws_close(&client->ws);
    }

    hybbx_ws_connection_cleanup(&client->ws);

    if (client->ws.fd >= 0) {
        shutdown(client->ws.fd, SHUT_RDWR);
        close(client->ws.fd);
        client->ws.fd = -1;
    }

    free(client);
    return NULL;
}

static void accept_client(int client_fd)
{
    ws_client_t *client;
    pthread_t thread;
    pthread_attr_t attr;

    client = calloc(1, sizeof(*client));
    if (client == NULL) {
        close(client_fd);
        return;
    }

    hybbx_ws_connection_init(&client->ws, client_fd);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&thread, &attr, ws_client_thread, client) != 0) {
        close(client_fd);
        free(client);
    }

    pthread_attr_destroy(&attr);
}

static void *ws_accept_thread(void *arg)
{
    (void)arg;

    while (g_ws_running) {
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

static hybbx_result_t ws_plugin_init(hybbx_service_t *service)
{
    g_service = service;
    return HYBBX_OK;
}

static void ws_plugin_shutdown(void)
{
    ws_plugin_stop();
}

static hybbx_result_t ws_plugin_start(const char *config)
{
    hybbx_result_t rc;

    if (g_ws_running) {
        return HYBBX_ERR_BUSY;
    }

    rc = hybbx_websocket_config_parse(config, &g_config);
    if (rc != HYBBX_OK) {
        return rc;
    }

    g_listen_v4 = -1;
    g_listen_v6 = -1;

    if (g_config.ipv4) {
        g_listen_v4 = create_listen_socket(AF_INET, g_config.bind_v4,
                                           g_config.port);
        if (g_listen_v4 < 0) {
            log_bind_failure(g_config.bind_v4, g_config.port);
            return HYBBX_ERR_IO;
        }
    }

    if (g_config.ipv6) {
        g_listen_v6 = create_listen_socket(AF_INET6, g_config.bind_v6,
                                           g_config.port);
        if (g_listen_v6 < 0) {
            fprintf(stderr,
                    "[websocket] IPv6 bind [%s]:%u skipped (%s)\n",
                    g_config.bind_v6, g_config.port, strerror(errno));
        }
    }

    if (g_listen_v4 < 0 && g_listen_v6 < 0) {
        return HYBBX_ERR_IO;
    }

    (void)hybbx_ws_publish_proxy_config(&g_config);

    if (hybbx_ws_tls_compiled()) {
        char cert_dir[HYBBX_PATH_MAX];
        hybbx_result_t tls_rc;

        if (hybbx_path_resolve(cert_dir, sizeof(cert_dir),
                               g_config.cert_dir) != HYBBX_OK) {
            hybbx_strlcpy(cert_dir, g_config.cert_dir, sizeof(cert_dir));
        }

        tls_rc = hybbx_ws_tls_ensure_certs(cert_dir);

        if (tls_rc == HYBBX_OK) {
            tls_rc = hybbx_ws_tls_server_start(cert_dir);
        }
        if (tls_rc != HYBBX_OK) {
            fprintf(stderr,
                    "[websocket] TLS init failed (%d), plain ws only\n",
                    (int)tls_rc);
        }
    }

    g_ws_running = 1;

    if (pthread_create(&g_accept_thread, NULL, ws_accept_thread, NULL) != 0) {
        g_ws_running = 0;
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

    printf("[websocket] listening");
    if (g_listen_v4 >= 0) {
        printf(" IPv4 %s:%u", g_config.bind_v4, g_config.port);
    }
    if (g_listen_v6 >= 0) {
        printf(" IPv6 [%s]:%u", g_config.bind_v6, g_config.port);
    }
    printf(" path=%s (%s, forward-proxy)\n", g_config.path,
           hybbx_ws_tls_server_enabled() ? "wss" : "ws");

    return HYBBX_OK;
}

static hybbx_result_t ws_plugin_stop(void)
{
    if (!g_ws_running) {
        return HYBBX_OK;
    }

    g_ws_running = 0;

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
    hybbx_ws_tls_server_stop();
    printf("[websocket] stop\n");
    return HYBBX_OK;
}

const hybbx_transport_plugin_t hybbx_plugin_websocket = {
    .name = "websocket",
    .kind = HYBBX_TRANSPORT_WEBSOCKET,
    .version = 1,
    .init = ws_plugin_init,
    .shutdown = ws_plugin_shutdown,
    .start = ws_plugin_start,
    .stop = ws_plugin_stop,
    .write = ws_plugin_write,
};
