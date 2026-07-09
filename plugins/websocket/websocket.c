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
#include "hybbx/security_ban.h"
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
#include <time.h>
#include <unistd.h>

#define WS_CLIENT_POLL_MS 5000
#define WS_CLIENT_PING_IDLE_SEC 20

typedef struct ws_client {
    hybbx_ws_connection_t ws;
    hybbx_session_t *hbx_session;
    hybbx_result_t last_rc;
    int slot_held;
    time_t last_io_at;
    uint8_t tx_buf[HYBBX_WS_FRAME_PAYLOAD_MAX];
    size_t tx_len;
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
static unsigned g_active_clients;
static pthread_mutex_t g_client_lock = PTHREAD_MUTEX_INITIALIZER;

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
    int err = errno;

    hybbx_socket_log_bind_failure("websocket", bind_addr, port);
    if (err == EACCES && port < 1024u) {
        fprintf(stderr,
                "[websocket] port %u is privileged — run as root, grant "
                "bind permission, or set port >= 1024 in hybbx.ini\n",
                port);
    }
}

static size_t utf8_char_span(const uint8_t *buf, size_t len, size_t pos)
{
    uint8_t b;
    size_t n;
    size_t i;

    if (pos >= len) {
        return 0;
    }

    b = buf[pos];
    if (b < 0x80u) {
        return 1;
    }
    if ((b & 0xE0u) == 0xC0u) {
        n = 2;
    } else if ((b & 0xF0u) == 0xE0u) {
        n = 3;
    } else if ((b & 0xF8u) == 0xF0u) {
        n = 4;
    } else {
        return 1;
    }

    if (pos + n > len) {
        return 0;
    }

    for (i = 1; i < n; i++) {
        if ((buf[pos + i] & 0xC0u) != 0x80u) {
            return 1;
        }
    }

    return n;
}

static size_t utf8_complete_prefix(const uint8_t *buf, size_t len)
{
    size_t pos = 0;

    while (pos < len) {
        size_t span = utf8_char_span(buf, len, pos);

        if (span == 0) {
            break;
        }
        pos += span;
    }

    return pos;
}

static hybbx_result_t ws_tx_flush(ws_client_t *client, int force_tail)
{
    size_t n;
    hybbx_result_t rc;

    if (client == NULL || client->tx_len == 0) {
        return HYBBX_OK;
    }

    n = utf8_complete_prefix(client->tx_buf, client->tx_len);
    if (n == 0 && force_tail) {
        client->tx_buf[0] = '?';
        n = 1;
    }
    if (n == 0) {
        return HYBBX_OK;
    }

    rc = hybbx_ws_write_text(&client->ws, (const char *)client->tx_buf, n);
    if (rc != HYBBX_OK) {
        return rc;
    }
    client->last_io_at = time(NULL);

    if (n < client->tx_len) {
        memmove(client->tx_buf, client->tx_buf + n, client->tx_len - n);
    }
    client->tx_len -= n;

    return HYBBX_OK;
}

static hybbx_result_t ws_plugin_write(hybbx_session_t *session,
                                      const char *data, size_t len)
{
    ws_client_t *client;
    size_t i;
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
            if (client->tx_len + 2 > sizeof(client->tx_buf)) {
                rc = ws_tx_flush(client, 1);
                if (rc != HYBBX_OK) {
                    return rc;
                }
            }
            client->tx_buf[client->tx_len++] = '\r';
        }

        if (client->tx_len >= sizeof(client->tx_buf)) {
            rc = ws_tx_flush(client, 1);
            if (rc != HYBBX_OK) {
                return rc;
            }
        }

        client->tx_buf[client->tx_len++] = (uint8_t)ch;

        rc = ws_tx_flush(client, 0);
        if (rc != HYBBX_OK) {
            return rc;
        }
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

static void ws_send_limit(ws_client_t *client)
{
    static const char msg[] =
        "All WebSocket connections in use. Try later.\r\n";

    if (client == NULL) {
        return;
    }

    (void)hybbx_ws_write_text(&client->ws, msg, sizeof(msg) - 1u);
}

static int ws_client_acquire_slot(ws_client_t *client)
{
    if (client == NULL) {
        return 0;
    }

    pthread_mutex_lock(&g_client_lock);
    if (g_active_clients >= g_config.max_connections) {
        pthread_mutex_unlock(&g_client_lock);
        return 0;
    }

    g_active_clients++;
    client->slot_held = 1;
    pthread_mutex_unlock(&g_client_lock);
    return 1;
}

static void ws_client_release_slot(ws_client_t *client)
{
    if (client == NULL || !client->slot_held) {
        return;
    }

    pthread_mutex_lock(&g_client_lock);
    if (g_active_clients > 0) {
        g_active_clients--;
    }
    client->slot_held = 0;
    pthread_mutex_unlock(&g_client_lock);
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

    if (!ws_client_acquire_slot(client)) {
        ws_send_limit(client);
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
    client->last_io_at = time(NULL);

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
            time_t now = time(NULL);

            if (now != (time_t)-1 &&
                (client->last_io_at == 0 ||
                 now - client->last_io_at >= WS_CLIENT_PING_IDLE_SEC)) {
                rc = hybbx_ws_ping(&client->ws);
                if (rc != HYBBX_OK) {
                    break;
                }
                client->last_io_at = now;
            }

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
        client->last_io_at = time(NULL);

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

    ws_client_release_slot(client);

    (void)ws_tx_flush(client, 1);

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
                    if (!hybbx_security_ban_accept_fd(client_fd)) {
                        close(client_fd);
                        continue;
                    }
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
    g_active_clients = 0;

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
    printf(" path=%s max_connections=%u (%s, forward-proxy)\n", g_config.path,
           g_config.max_connections,
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
