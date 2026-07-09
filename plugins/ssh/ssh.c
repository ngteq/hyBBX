/*
 * ssh — libssh transport plugin (port 3232). INI: [transport.ssh].
 *
 * Links dynamically against libssh (LGPL-2.1+). See share/THIRD_PARTY_NOTICES.txt.
 */
#include "hybbx/plugin.h"
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "hybbx/socket.h"
#include "hybbx/security_ban.h"
#include "hybbx/ssh.h"
#include "hybbx/traffic.h"
#include "hybbx/util.h"

#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/server.h>

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

#define SSH_CLIENT_POLL_MS 30000
#define SSH_READ_BUF 512

typedef struct ssh_client {
    int fd;
    ssh_session session;
    ssh_channel channel;
    ssh_event event;
    hybbx_session_t *hbx_session;
    int ssh_authenticated;
    int auth_attempts;
    hybbx_result_t last_rc;
    volatile int shell_ready;
    struct ssh_server_callbacks_struct server_cb;
    struct ssh_channel_callbacks_struct channel_cb;
} ssh_client_t;

typedef struct ssh_client_ctx {
    ssh_client_t *client;
} ssh_client_ctx_t;

extern const hybbx_transport_plugin_t hybbx_plugin_ssh;

static hybbx_service_t *g_service;
static hybbx_ssh_config_t g_config;
static char g_hostkey_path[HYBBX_PATH_MAX];
static pthread_t g_accept_thread;
static int g_listen_v4 = -1;
static int g_listen_v6 = -1;
static volatile int g_ssh_running = 0;

static hybbx_result_t ssh_plugin_stop(void);

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

/*
 * SSH wire authentication only — accepts any username/password so the client
 * can open a shell channel. HyBBX login (guest auto-login or /login prompt)
 * follows [auth] in hybbx.ini, same as telnet.
 */
static int ssh_auth_password(ssh_session session, const char *user,
                             const char *password, void *userdata)
{
    ssh_client_t *client = (ssh_client_t *)userdata;

    (void)session;
    (void)user;
    (void)password;

    if (client == NULL) {
        return SSH_AUTH_DENIED;
    }

    client->auth_attempts++;
    client->ssh_authenticated = 1;
    return SSH_AUTH_SUCCESS;
}

static hybbx_result_t ssh_plugin_stop(void);

static int ssh_channel_pty_request(ssh_session session, ssh_channel channel,
                                   const char *term, int cols, int rows,
                                   int py, int px, void *userdata)
{
    (void)session;
    (void)channel;
    (void)term;
    (void)cols;
    (void)rows;
    (void)py;
    (void)px;
    (void)userdata;
    return SSH_OK;
}

static int ssh_channel_shell_request(ssh_session session, ssh_channel channel,
                                     void *userdata)
{
    ssh_client_t *client = (ssh_client_t *)userdata;

    (void)session;
    (void)channel;

    if (client == NULL) {
        return SSH_ERROR;
    }

    client->shell_ready = 1;
    return SSH_OK;
}

static ssh_channel ssh_channel_open(ssh_session session, void *userdata)
{
    ssh_client_t *client = (ssh_client_t *)userdata;

    (void)session;

    if (client == NULL) {
        return NULL;
    }

    client->channel = ssh_channel_new(session);
    if (client->channel == NULL) {
        return NULL;
    }

    memset(&client->channel_cb, 0, sizeof(client->channel_cb));
    client->channel_cb.userdata = client;
    client->channel_cb.channel_pty_request_function = ssh_channel_pty_request;
    client->channel_cb.channel_shell_request_function = ssh_channel_shell_request;
    ssh_callbacks_init(&client->channel_cb);
    ssh_set_channel_callbacks(client->channel, &client->channel_cb);

    return client->channel;
}

static hybbx_result_t ssh_plugin_write(hybbx_session_t *session,
                                const char *data, size_t len)
{
    ssh_client_t *client;
    size_t i;
    int rc;

    if (session == NULL || data == NULL) {
        return HYBBX_ERR_INVALID;
    }

    client = (ssh_client_t *)session->transport_data;
    if (client == NULL || client->channel == NULL) {
        return HYBBX_ERR_INVALID;
    }

    for (i = 0; i < len; i++) {
        char out[2];
        size_t out_len = 1;

        out[0] = data[i];
        if (data[i] == '\n' && (i == 0 || data[i - 1] != '\r')) {
            out[0] = '\r';
            out[1] = '\n';
            out_len = 2;
        }

        rc = ssh_channel_write(client->channel, out, out_len);
        if (rc < 0) {
            return HYBBX_ERR_IO;
        }
    }

    return HYBBX_OK;
}

static void ssh_send_busy(ssh_channel channel)
{
    static const char msg[] = "All nodes in use. Try later.\r\n";

    if (channel == NULL) {
        return;
    }

    (void)ssh_channel_write(channel, msg, sizeof(msg) - 1u);
}

static int ssh_wait_auth_and_shell(ssh_client_t *client)
{
    int ticks = 0;

    while (g_ssh_running) {
        if (client->ssh_authenticated && client->channel != NULL &&
            client->shell_ready) {
            return 0;
        }

        if (client->auth_attempts >= 5 || ticks >= 300) {
            return -1;
        }

        if (ssh_event_dopoll(client->event, 100) == SSH_ERROR) {
            return -1;
        }
        ticks++;
    }

    return -1;
}

static void ssh_on_user_data(const uint8_t *data, size_t len, void *ctx)
{
    ssh_client_ctx_t *cctx = (ssh_client_ctx_t *)ctx;
    hybbx_result_t rc;

    if (cctx == NULL || cctx->client == NULL ||
        cctx->client->hbx_session == NULL || data == NULL || len == 0) {
        return;
    }

    rc = hybbx_session_handle_input(cctx->client->hbx_session, data, len);
    if (rc != HYBBX_OK) {
        cctx->client->last_rc = rc;
    }
}

static int ssh_plugin_service_request(ssh_session session, const char *service,
                                      void *userdata)
{
    (void)session;
    (void)userdata;

    if (service == NULL) {
        return SSH_ERROR;
    }

    if (strcmp(service, "ssh-userauth") == 0 ||
        strcmp(service, "ssh-connection") == 0) {
        return SSH_OK;
    }

    return SSH_ERROR;
}

static void *ssh_client_thread(void *arg)
{
    ssh_client_t *client = (ssh_client_t *)arg;
    ssh_bind sshbind = NULL;
    ssh_client_ctx_t cctx;
    uint8_t buf[SSH_READ_BUF];
    hybbx_result_t rc;
    char remote[64];

    if (client == NULL || g_service == NULL) {
        free(client);
        return NULL;
    }

    remote[0] = '\0';
    (void)hybbx_socket_peer_name(client->fd, remote, sizeof(remote));

    sshbind = ssh_bind_new();
    if (sshbind == NULL) {
        goto cleanup;
    }

    if (ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_HOSTKEY,
                             g_hostkey_path) != SSH_OK) {
        fprintf(stderr, "[ssh] host key load failed: %s\n", g_hostkey_path);
        goto cleanup;
    }

    client->session = ssh_new();
    if (client->session == NULL) {
        goto cleanup;
    }

    if (ssh_bind_accept_fd(sshbind, client->session, client->fd) != SSH_OK) {
        goto cleanup;
    }

    memset(&client->server_cb, 0, sizeof(client->server_cb));
    client->server_cb.userdata = client;
    client->server_cb.auth_password_function = ssh_auth_password;
    client->server_cb.service_request_function = ssh_plugin_service_request;
    client->server_cb.channel_open_request_session_function = ssh_channel_open;
    ssh_callbacks_init(&client->server_cb);
    ssh_set_server_callbacks(client->session, &client->server_cb);
    ssh_set_auth_methods(client->session, SSH_AUTH_METHOD_PASSWORD);

    if (ssh_server_init_kex(client->session) != SSH_OK) {
        goto cleanup;
    }

    if (ssh_handle_key_exchange(client->session) != SSH_OK) {
        goto cleanup;
    }

    client->event = ssh_event_new();
    if (client->event == NULL) {
        goto cleanup;
    }

    if (ssh_event_add_session(client->event, client->session) != SSH_OK) {
        goto cleanup;
    }

    if (ssh_wait_auth_and_shell(client) != 0) {
        goto cleanup;
    }

    rc = hybbx_session_open(g_service, &hybbx_plugin_ssh, client,
                            &client->hbx_session);
    if (rc != HYBBX_OK || client->hbx_session == NULL) {
        if (rc == HYBBX_ERR_BUSY) {
            ssh_send_busy(client->channel);
        }
        goto cleanup;
    }

    if (remote[0] != '\0') {
        (void)hybbx_session_set_remote(client->hbx_session, remote);
    }

    /*
     * Telnet clients echo locally when the server sends WONT ECHO; SSH PTY
     * clients do not. Mirror telnet UX by enabling HyBBX input echo unless
     * [traffic] input_echo=yes already turned it on.
     */
    {
        const hybbx_traffic_config_t *traffic = hybbx_traffic_config_get();

        if (traffic == NULL || !traffic->input_echo) {
            (void)hybbx_session_set_input_echo(client->hbx_session, 1);
        }
    }

    cctx.client = client;
    client->last_rc = HYBBX_OK;

    while (g_ssh_running && ssh_channel_is_open(client->channel)) {
        int n;
        int ev;

        ev = ssh_event_dopoll(client->event, SSH_CLIENT_POLL_MS);
        if (ev == SSH_ERROR) {
            break;
        }

        if (ev == 0) {
            rc = hybbx_session_tick(client->hbx_session);
            if (rc == HYBBX_SESSION_END) {
                client->last_rc = HYBBX_SESSION_END;
                break;
            }
        }

        while ((n = ssh_channel_read(client->channel, buf, sizeof(buf), 0)) > 0) {
            ssh_on_user_data((const uint8_t *)buf, (size_t)n, &cctx);
            if (client->last_rc == HYBBX_SESSION_END) {
                break;
            }
        }
        if (client->last_rc == HYBBX_SESSION_END) {
            break;
        }
        if (n < 0 && !ssh_channel_is_open(client->channel)) {
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

    if (client->channel != NULL) {
        if (ssh_channel_is_open(client->channel)) {
            ssh_channel_send_eof(client->channel);
            ssh_channel_close(client->channel);
        }
        ssh_channel_free(client->channel);
        client->channel = NULL;
    }

    if (client->event != NULL) {
        ssh_event_free(client->event);
        client->event = NULL;
    }

    if (client->session != NULL) {
        ssh_disconnect(client->session);
        ssh_free(client->session);
        client->session = NULL;
    }

    if (sshbind != NULL) {
        ssh_bind_free(sshbind);
        sshbind = NULL;
    }

    if (client->fd >= 0) {
        client->fd = -1;
    }

    free(client);
    return NULL;
}

static void accept_client(int client_fd)
{
    ssh_client_t *client;
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

    if (pthread_create(&thread, &attr, ssh_client_thread, client) != 0) {
        close(client_fd);
        free(client);
    }

    pthread_attr_destroy(&attr);
}

static void *ssh_accept_thread(void *arg)
{
    (void)arg;

    while (g_ssh_running) {
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

static hybbx_result_t ssh_plugin_init(hybbx_service_t *service)
{
    int rc;

    g_service = service;

    rc = ssh_init();
    if (rc < 0) {
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}

static void ssh_plugin_shutdown(void)
{
    ssh_plugin_stop();
    ssh_finalize();
}

static hybbx_result_t ssh_plugin_start(const char *config)
{
    char keys_resolved[HYBBX_PATH_MAX];
    hybbx_result_t rc;

    if (g_ssh_running) {
        return HYBBX_ERR_BUSY;
    }

    rc = hybbx_ssh_config_parse(config, &g_config);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (hybbx_path_resolve(keys_resolved, sizeof(keys_resolved),
                           g_config.hostkey_dir) != HYBBX_OK) {
        hybbx_strlcpy(keys_resolved, g_config.hostkey_dir,
                      sizeof(keys_resolved));
    }

    rc = hybbx_ssh_keys_ensure(keys_resolved, g_hostkey_path,
                               sizeof(g_hostkey_path));
    if (rc != HYBBX_OK) {
        return rc;
    }

    g_listen_v4 = -1;
    g_listen_v6 = -1;

    if (g_config.ipv4) {
        g_listen_v4 = create_listen_socket(AF_INET, g_config.bind_v4,
                                           g_config.port);
        if (g_listen_v4 < 0) {
            fprintf(stderr, "[ssh] failed to bind IPv4 %s:%u (%s)\n",
                    g_config.bind_v4, g_config.port, strerror(errno));
            return HYBBX_ERR_IO;
        }
    }

    if (g_config.ipv6) {
        g_listen_v6 = create_listen_socket(AF_INET6, g_config.bind_v6,
                                           g_config.port);
        if (g_listen_v6 < 0) {
            fprintf(stderr, "[ssh] IPv6 bind [%s]:%u skipped (%s)\n",
                    g_config.bind_v6, g_config.port, strerror(errno));
        }
    }

    if (g_listen_v4 < 0 && g_listen_v6 < 0) {
        return HYBBX_ERR_IO;
    }

    g_ssh_running = 1;

    if (pthread_create(&g_accept_thread, NULL, ssh_accept_thread, NULL) != 0) {
        g_ssh_running = 0;
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

    printf("[ssh] listening");
    if (g_listen_v4 >= 0) {
        printf(" IPv4 %s:%u", g_config.bind_v4, g_config.port);
    }
    if (g_listen_v6 >= 0) {
        printf(" IPv6 [%s]:%u", g_config.bind_v6, g_config.port);
    }
    printf(" (libssh)\n");

    return HYBBX_OK;
}

static hybbx_result_t ssh_plugin_stop(void)
{
    if (!g_ssh_running) {
        return HYBBX_OK;
    }

    g_ssh_running = 0;

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
    printf("[ssh] stop\n");
    return HYBBX_OK;
}

const hybbx_transport_plugin_t hybbx_plugin_ssh = {
    .name = "ssh",
    .kind = HYBBX_TRANSPORT_SSH,
    .version = 1,
    .init = ssh_plugin_init,
    .shutdown = ssh_plugin_shutdown,
    .start = ssh_plugin_start,
    .stop = ssh_plugin_stop,
    .write = ssh_plugin_write,
};
