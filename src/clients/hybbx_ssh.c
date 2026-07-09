/**
 * hybbx-ssh — HyBBX-native SSH client (CLI only, no GUI).
 *
 * Encrypted transport to HyBBX [transport.ssh] (default port 3232).
 * Wire SSH username/password satisfy the libssh handshake only; HyBBX
 * accounts use guest auto-login or /login like telnet.
 * Host keys are not verified — no known_hosts prompt.
 */

#if defined(__linux__) || defined(__GLIBC__)
#define _DEFAULT_SOURCE 1
#endif

#include "client_line.h"
#include "client_display.h"
#include "hybbx/limits.h"
#include "hybbx/ssh.h"
#include "hybbx/traffic.h"
#include "hybbx/util.h"

#include <libssh/libssh.h>

#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HYBBX_SSH_POLL_MS       50
#define HYBBX_SSH_WIRE_USER     "hybbx"
#define HYBBX_SSH_WIRE_PASSWORD "hybbx"

typedef struct hybbx_ssh_client_config {
    char host[256];
    unsigned port;
    char wire_user[64];
    char wire_password[128];
    char login_user[64];
    char login_password[128];
    unsigned baud;
    unsigned line_width;
    int pace_output;
    int ansi;
    int verbose;
    unsigned timeout_sec;
    int ipv6;
} hybbx_ssh_client_config_t;

typedef struct ssh_session_ctx {
    ssh_session session;
    ssh_channel channel;
    hybbx_client_display_t display;
    hybbx_client_line_editor_t editor;
    int editor_ready;
} ssh_session_ctx_t;

static void config_defaults(hybbx_ssh_client_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    hybbx_strlcpy(cfg->host, "127.0.0.1", sizeof(cfg->host));
    cfg->port = HYBBX_SSH_DEFAULT_PORT;
    hybbx_strlcpy(cfg->wire_user, HYBBX_SSH_WIRE_USER, sizeof(cfg->wire_user));
    hybbx_strlcpy(cfg->wire_password, HYBBX_SSH_WIRE_PASSWORD,
                  sizeof(cfg->wire_password));
    cfg->baud = HYBBX_BAUD2400;
    cfg->line_width = HYBBX_LINE_WIDTH;
    cfg->pace_output = 1;
    cfg->timeout_sec = 30;
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "hybbx-ssh — HyBBX SSH client (encrypted transport, CLI only)\n"
            "\n"
            "Usage: %s [options] [host [port]]\n"
            "\n"
            "Options:\n"
            "  -H, --host HOST       Remote host (default 127.0.0.1)\n"
            "  -p, --port PORT       SSH port (default 3232)\n"
            "  -u, --user USER       Send /login USER after connect\n"
            "  -P, --password PASS   Password for /login\n"
            "      --wire-user USER  SSH wire username (default hybbx)\n"
            "      --wire-pass PASS  SSH wire password (default hybbx)\n"
            "      --baud N          Pace output like HyBBX (default 2400, 0=off)\n"
            "      --line-width N    Wrap display columns (default 80, 0=off)\n"
            "      --pace yes|no     Output pacing (default yes)\n"
            "      --ansi yes|no     Pass ANSI sequences (default no)\n"
            "  -6, --ipv6            Prefer IPv6 when connecting\n"
            "  -t, --timeout SEC     Connect timeout (default 30)\n"
            "  -v, --verbose         Log SSH progress on stderr\n"
            "  -h, --help            Show help\n"
            "\n"
            "SSH wire credentials are not HyBBX accounts — any value is accepted\n"
            "by the server. Use -u/-P for HyBBX /login after connect.\n"
            "\n"
            "Environment: HYBBX_HOST, HYBBX_PORT, HYBBX_SSH_WIRE_USER,\n"
            "             HYBBX_SSH_WIRE_PASS\n"
            "\n",
            prog);
}

static void log_ssh_error(ssh_session session, const char *what)
{
    const char *msg = ssh_get_error(session);

    if (msg == NULL || msg[0] == '\0') {
        msg = "unknown error";
    }
    fprintf(stderr, "hybbx-ssh: %s: %s\n", what, msg);
}

static int ssh_connect_session(const hybbx_ssh_client_config_t *cfg,
                               ssh_session *out_session,
                               ssh_channel *out_channel)
{
    ssh_session session;
    ssh_channel channel;
    char portbuf[16];
    long timeout_sec;
    int strict = 0;
    int process_config = 0;
    int pubkey_auth = 0;
    int nodelay = 1;
    int rc;

    session = ssh_new();
    if (session == NULL) {
        fprintf(stderr, "hybbx-ssh: ssh_new failed\n");
        return -1;
    }

    snprintf(portbuf, sizeof(portbuf), "%u", cfg->port);
    timeout_sec = (long)cfg->timeout_sec;

    (void)ssh_options_set(session, SSH_OPTIONS_HOST, cfg->host);
    (void)ssh_options_set(session, SSH_OPTIONS_PORT_STR, portbuf);
    (void)ssh_options_set(session, SSH_OPTIONS_USER, cfg->wire_user);
    (void)ssh_options_set(session, SSH_OPTIONS_STRICTHOSTKEYCHECK, &strict);
    (void)ssh_options_set(session, SSH_OPTIONS_PROCESS_CONFIG, &process_config);
    (void)ssh_options_set(session, SSH_OPTIONS_PUBKEY_AUTH, &pubkey_auth);
    (void)ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout_sec);
    (void)ssh_options_set(session, SSH_OPTIONS_NODELAY, &nodelay);

    if (cfg->ipv6) {
        (void)ssh_options_set(session, SSH_OPTIONS_BINDADDR, "::");
    }

    rc = ssh_connect(session);
    if (rc != SSH_OK) {
        log_ssh_error(session, "connect");
        ssh_free(session);
        return -1;
    }

    if (cfg->verbose) {
        fprintf(stderr, "hybbx-ssh: SSH handshake complete with %s:%u\n",
                cfg->host, cfg->port);
    }

    rc = ssh_userauth_password(session, NULL, cfg->wire_password);
    if (rc != SSH_AUTH_SUCCESS) {
        log_ssh_error(session, "wire authentication");
        ssh_disconnect(session);
        ssh_free(session);
        return -1;
    }

    channel = ssh_channel_new(session);
    if (channel == NULL) {
        log_ssh_error(session, "channel_new");
        ssh_disconnect(session);
        ssh_free(session);
        return -1;
    }

    rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        log_ssh_error(session, "channel_open_session");
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
        return -1;
    }

    rc = ssh_channel_request_pty(channel);
    if (rc != SSH_OK) {
        log_ssh_error(session, "channel_request_pty");
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
        return -1;
    }

    rc = ssh_channel_request_shell(channel);
    if (rc != SSH_OK) {
        log_ssh_error(session, "channel_request_shell");
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        ssh_disconnect(session);
        ssh_free(session);
        return -1;
    }

    ssh_set_blocking(session, 0);

    *out_session = session;
    *out_channel = channel;
    return 0;
}

static hybbx_result_t send_line(ssh_channel channel, const char *line)
{
    char buf[HYBBX_LINE_MAX + 2];
    size_t len;
    int rc;

    snprintf(buf, sizeof(buf), "%s\n", line);
    len = strlen(buf);

    rc = ssh_channel_write(channel, buf, (uint32_t)len);
    if (rc < 0 || (size_t)rc != len) {
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}

static void maybe_auto_login(ssh_channel channel,
                             const hybbx_ssh_client_config_t *cfg)
{
    char line[192];

    if (cfg->login_user[0] == '\0') {
        return;
    }

    snprintf(line, sizeof(line), "/login %s", cfg->login_user);
    (void)send_line(channel, line);
    if (cfg->login_password[0] != '\0') {
        (void)send_line(channel, cfg->login_password);
    }
}

static int drain_channel(ssh_session_ctx_t *ctx, int verbose)
{
    uint8_t buf[512];
    int n;

    for (;;) {
        n = ssh_channel_read_nonblocking(ctx->channel, buf, sizeof(buf), 0);
        if (n == SSH_AGAIN) {
            return 0;
        }
        if (n <= 0) {
            if (ssh_channel_is_eof(ctx->channel) || ssh_channel_is_closed(ctx->channel)) {
                return -1;
            }
            return 0;
        }

        if (verbose) {
            fprintf(stderr, "hybbx-ssh: recv %d bytes\n", n);
        }

        hybbx_client_display_write(&ctx->display, buf, (size_t)n);
    }
}

static int run_session(ssh_session_ctx_t *ctx, const hybbx_ssh_client_config_t *cfg)
{
    char inbuf[HYBBX_LINE_MAX + 2];
    int running = 1;

    maybe_auto_login(ctx->channel, cfg);

    while (running) {
        struct pollfd pfds[2];
        socket_t ssh_fd;
        int pr;
        int nready = 1;

        pfds[0].fd = STDIN_FILENO;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;

        ssh_fd = ssh_get_fd(ctx->session);
        pfds[1].fd = (int)ssh_fd;
        pfds[1].events = POLLIN;
        pfds[1].revents = 0;
        nready = 2;

        pr = poll(pfds, (unsigned int)nready, HYBBX_SSH_POLL_MS);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (drain_channel(ctx, cfg->verbose) != 0) {
            break;
        }

        if ((pfds[0].revents & POLLIN) != 0) {
            int have_line = 0;
            int eof_seen = 0;

            if (!ctx->editor_ready ||
                hybbx_client_line_editor_poll(&ctx->editor, inbuf, sizeof(inbuf),
                                              &have_line, &eof_seen) != HYBBX_OK) {
                running = 0;
            } else if (eof_seen) {
                running = 0;
            } else if (have_line) {
                if (send_line(ctx->channel, inbuf) != HYBBX_OK) {
                    break;
                }
            }
        }

        if ((pfds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            break;
        }

        if ((pfds[1].revents & POLLIN) != 0 && drain_channel(ctx, cfg->verbose) != 0) {
            break;
        }
    }

    return 0;
}

static void session_ctx_init(ssh_session_ctx_t *ctx,
                             ssh_session session,
                             ssh_channel channel,
                             const hybbx_ssh_client_config_t *cfg)
{
    hybbx_traffic_config_t traffic;

    memset(ctx, 0, sizeof(*ctx));
    ctx->session = session;
    ctx->channel = channel;

    hybbx_traffic_config_defaults(&traffic);
    traffic.baud = cfg->baud;
    traffic.line_width = cfg->line_width;
    traffic.pace_output = cfg->pace_output;
    traffic.ansi = cfg->ansi;
    hybbx_client_display_init(&ctx->display, &traffic);

    if (hybbx_client_line_editor_start(&ctx->editor, STDIN_FILENO) == 0) {
        ctx->editor_ready = 1;
    }
}

static void session_ctx_fini(ssh_session_ctx_t *ctx)
{
    if (ctx->editor_ready) {
        hybbx_client_line_editor_stop(&ctx->editor);
        ctx->editor_ready = 0;
    }
}

int main(int argc, char *argv[])
{
    hybbx_ssh_client_config_t cfg;
    ssh_session session = NULL;
    ssh_channel channel = NULL;
    ssh_session_ctx_t ctx;
    int opt;
    int argi = 1;
    const char *env;

    static const struct option long_opts[] = {
        {"host", required_argument, NULL, 'H'},
        {"port", required_argument, NULL, 'p'},
        {"user", required_argument, NULL, 'u'},
        {"password", required_argument, NULL, 'P'},
        {"wire-user", required_argument, NULL, 1004},
        {"wire-pass", required_argument, NULL, 1005},
        {"baud", required_argument, NULL, 1000},
        {"line-width", required_argument, NULL, 1001},
        {"pace", required_argument, NULL, 1002},
        {"ansi", required_argument, NULL, 1003},
        {"ipv6", no_argument, NULL, '6'},
        {"timeout", required_argument, NULL, 't'},
        {"verbose", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    config_defaults(&cfg);

    env = getenv("HYBBX_HOST");
    if (env != NULL && env[0] != '\0') {
        hybbx_strlcpy(cfg.host, env, sizeof(cfg.host));
    }
    env = getenv("HYBBX_PORT");
    if (env != NULL && env[0] != '\0') {
        cfg.port = (unsigned)strtoul(env, NULL, 10);
    }
    env = getenv("HYBBX_SSH_WIRE_USER");
    if (env != NULL && env[0] != '\0') {
        hybbx_strlcpy(cfg.wire_user, env, sizeof(cfg.wire_user));
    }
    env = getenv("HYBBX_SSH_WIRE_PASS");
    if (env != NULL && env[0] != '\0') {
        hybbx_strlcpy(cfg.wire_password, env, sizeof(cfg.wire_password));
    }

    while ((opt = getopt_long(argc, argv, "H:p:u:P:6t:vh", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'H':
            hybbx_strlcpy(cfg.host, optarg, sizeof(cfg.host));
            break;
        case 'p':
            cfg.port = (unsigned)strtoul(optarg, NULL, 10);
            break;
        case 'u':
            hybbx_strlcpy(cfg.login_user, optarg, sizeof(cfg.login_user));
            break;
        case 'P':
            hybbx_strlcpy(cfg.login_password, optarg, sizeof(cfg.login_password));
            break;
        case '6':
            cfg.ipv6 = 1;
            break;
        case 't':
            cfg.timeout_sec = (unsigned)strtoul(optarg, NULL, 10);
            break;
        case 'v':
            cfg.verbose = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        case 1000:
            cfg.baud = (unsigned)strtoul(optarg, NULL, 10);
            break;
        case 1001:
            cfg.line_width = (unsigned)strtoul(optarg, NULL, 10);
            break;
        case 1002:
            cfg.pace_output = hybbx_parse_bool(optarg, cfg.pace_output);
            break;
        case 1003:
            cfg.ansi = hybbx_parse_bool(optarg, cfg.ansi);
            break;
        case 1004:
            hybbx_strlcpy(cfg.wire_user, optarg, sizeof(cfg.wire_user));
            break;
        case 1005:
            hybbx_strlcpy(cfg.wire_password, optarg, sizeof(cfg.wire_password));
            break;
        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    argi = optind;
    if (argi < argc) {
        hybbx_strlcpy(cfg.host, argv[argi++], sizeof(cfg.host));
    }
    if (argi < argc) {
        cfg.port = (unsigned)strtoul(argv[argi++], NULL, 10);
    }

    if (ssh_connect_session(&cfg, &session, &channel) != 0) {
        return EXIT_FAILURE;
    }

    if (cfg.verbose) {
        fprintf(stderr,
                "hybbx-ssh: shell ready on %s:%u (baud=%u width=%u wire=%s)\n",
                cfg.host, cfg.port, cfg.baud, cfg.line_width, cfg.wire_user);
    }

    session_ctx_init(&ctx, session, channel, &cfg);
    run_session(&ctx, &cfg);
    session_ctx_fini(&ctx);

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    ssh_disconnect(session);
    ssh_free(session);

    return EXIT_SUCCESS;
}
