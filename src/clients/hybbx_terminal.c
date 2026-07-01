/**
 * hybbx-terminal — HyBBX AX.25 / HBX circuit terminal client (CLI only).
 *
 * Connects to the internal HBX circuit hub, authenticates with link password,
 * and exchanges TERMINAL and AX.25 UI frames for operator sessions.
 */

#if defined(__linux__) || defined(__GLIBC__)
#define _DEFAULT_SOURCE 1
#endif

#include "client_display.h"
#include "hybbx/ax25.h"
#include "hybbx/circuit.h"
#include "hybbx/circuit_tcp.h"
#include "hybbx/limits.h"
#include "hybbx/link.h"
#include "hybbx/traffic.h"
#include "hybbx/util.h"

#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HYBBX_TERMINAL_POLL_MS 50

typedef struct hybbx_terminal_config {
    char host[256];
    unsigned port;
    char link_id[HYBBX_LINK_ID_MAX];
    char link_password[128];
    char link_role[HYBBX_LINK_ROLE_MAX];
    char mycall[32];
    char dest[32];
    char via[128];
    int ax25_ui;
    int verbose;
    int ipv6;
} hybbx_terminal_config_t;

typedef struct terminal_session {
    int fd;
    const hybbx_terminal_config_t *cfg;
    hybbx_circuit_decoder_t dec;
    hybbx_client_display_t display;
    hybbx_ax25_path_t path;
    int path_ready;
} terminal_session_t;

static void config_defaults(hybbx_terminal_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    hybbx_strlcpy(cfg->host, "127.0.0.1", sizeof(cfg->host));
    cfg->port = HYBBX_CIRCUIT_DEFAULT_PORT;
    hybbx_strlcpy(cfg->link_id, "terminal-1", sizeof(cfg->link_id));
    hybbx_strlcpy(cfg->link_role, "gateway", sizeof(cfg->link_role));
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "hybbx-terminal — HyBBX AX.25 / HBX circuit terminal client (CLI only)\n"
            "\n"
            "Usage: %s [options] [host [port]]\n"
            "\n"
            "Options:\n"
            "  -H, --host HOST         Circuit hub host (default 127.0.0.1)\n"
            "  -p, --port PORT         Circuit hub port (default 7323)\n"
            "      --link-id ID        Link identity for password auth\n"
            "      --link-password P   Link password (HBX LINK_AUTH)\n"
            "      --link-role ROLE    gateway | repeater | link | digipeater\n"
            "      --mycall CALL       AX.25 source (CALL or CALL-SSID)\n"
            "      --dest CALL         AX.25 destination\n"
            "      --via LIST          Comma-separated digipeater path\n"
            "      --ax25-ui           Send lines as AX.25 UI frames (needs mycall/dest)\n"
            "      --baud N            Pace output (default 2400, 0=off)\n"
            "      --line-width N      Wrap display columns (default 40)\n"
            "      --pace yes|no       Output pacing\n"
            "      --ansi yes|no       Pass ANSI sequences\n"
            "  -6, --ipv6              Prefer IPv6 (reserved)\n"
            "  -v, --verbose           Log HBX frames on stderr\n"
            "  -h, --help              Show help\n"
            "\n"
            "Environment: HYBBX_CIRCUIT_HOST, HYBBX_CIRCUIT_PORT\n"
            "\n",
            prog);
}

static int build_ax25_path(const hybbx_terminal_config_t *cfg,
                           hybbx_ax25_path_t *path)
{
    const char *cursor;
    char token[32];

    memset(path, 0, sizeof(*path));

    if (cfg->dest[0] == '\0' || cfg->mycall[0] == '\0') {
        return 0;
    }

    if (hybbx_ax25_address_parse(cfg->dest, &path->dest) != HYBBX_OK) {
        return 0;
    }
    if (hybbx_ax25_address_parse(cfg->mycall, &path->source) != HYBBX_OK) {
        return 0;
    }

    cursor = cfg->via;
    while (cursor != NULL && *cursor != '\0' && path->digi_count < HYBBX_AX25_MAX_DIGI) {
        const char *comma = strchr(cursor, ',');
        size_t len;

        if (comma != NULL) {
            len = (size_t)(comma - cursor);
        } else {
            len = strlen(cursor);
        }
        if (len >= sizeof(token)) {
            len = sizeof(token) - 1;
        }
        memcpy(token, cursor, len);
        token[len] = '\0';

        if (hybbx_ax25_address_parse(token, &path->digi[path->digi_count]) == HYBBX_OK) {
            path->digi_count++;
        }

        cursor = comma != NULL ? comma + 1 : NULL;
    }

    return 1;
}

static void on_circuit_frame(hybbx_circuit_proto_t proto, uint16_t flags,
                             const uint8_t *payload, size_t len,
                             void *userdata)
{
    terminal_session_t *sess = (terminal_session_t *)userdata;
    uint8_t ui[HYBBX_AX25_PAYLOAD_MAX];
    hybbx_ax25_path_t path;
    size_t ui_len;

    (void)flags;

    if (sess == NULL || len == 0) {
        return;
    }

    if (sess->cfg->verbose) {
        fprintf(stderr, "hybbx-terminal: rx proto=%s (%u bytes)\n",
                hybbx_circuit_proto_name(proto), (unsigned)len);
    }

    switch (proto) {
    case HYBBX_CIRCUIT_PROTO_TERMINAL:
        hybbx_client_display_write(&sess->display, payload, len);
        break;
    case HYBBX_CIRCUIT_PROTO_AX25_UI:
        ui_len = hybbx_circuit_unpack_ax25_ui(payload, len, &path, ui, sizeof(ui));
        if (ui_len > 0) {
            hybbx_client_display_write(&sess->display, ui, ui_len);
        }
        break;
    case HYBBX_CIRCUIT_PROTO_AX25:
        ui_len = hybbx_ax25_parse_ui(payload, len, &path, ui, sizeof(ui));
        if (ui_len > 0) {
            hybbx_client_display_write(&sess->display, ui, ui_len);
        }
        break;
    default:
        break;
    }
}

static hybbx_result_t send_terminal_line(terminal_session_t *sess,
                                         const char *line)
{
    uint8_t frame[HYBBX_CIRCUIT_MAX_FRAME];
    size_t frame_len;
    size_t len = strlen(line);

    frame_len = hybbx_circuit_encode_terminal(line, len, frame, sizeof(frame));
    if (frame_len == 0) {
        return HYBBX_ERR_IO;
    }

    return hybbx_circuit_link_write(sess->fd, frame, frame_len);
}

static hybbx_result_t send_ax25_ui_line(terminal_session_t *sess,
                                        const char *line)
{
    uint8_t frame[HYBBX_CIRCUIT_MAX_FRAME];
    size_t frame_len;
    size_t len = strlen(line);

    if (!sess->path_ready) {
        return HYBBX_ERR_INVALID;
    }

    frame_len = hybbx_circuit_encode_ax25_ui(&sess->path,
                                             (const uint8_t *)line, len,
                                             HYBBX_CIRCUIT_FLAG_TX,
                                             frame, sizeof(frame));
    if (frame_len == 0) {
        return HYBBX_ERR_IO;
    }

    return hybbx_circuit_link_write(sess->fd, frame, frame_len);
}

static int run_session(terminal_session_t *sess)
{
    char inbuf[HYBBX_LINE_MAX + 2];
    int running = 1;

    while (running) {
        struct pollfd pfds[2];
        int pr;
        uint8_t buf[512];

        pfds[0].fd = STDIN_FILENO;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        pfds[1].fd = sess->fd;
        pfds[1].events = POLLIN;
        pfds[1].revents = 0;

        pr = poll(pfds, 2, HYBBX_TERMINAL_POLL_MS);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if ((pfds[0].revents & POLLIN) != 0) {
            if (fgets(inbuf, sizeof(inbuf), stdin) == NULL) {
                running = 0;
            } else {
                size_t len = strlen(inbuf);
                hybbx_result_t rc;

                if (len > 0 && inbuf[len - 1] == '\n') {
                    inbuf[len - 1] = '\0';
                }

                if (sess->cfg->ax25_ui && sess->path_ready) {
                    rc = send_ax25_ui_line(sess, inbuf);
                } else {
                    rc = send_terminal_line(sess, inbuf);
                }
                if (rc != HYBBX_OK) {
                    break;
                }
            }
        }

        if ((pfds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            break;
        }

        if ((pfds[1].revents & POLLIN) != 0) {
            size_t read_len;

            if (hybbx_circuit_link_read(sess->fd, buf, sizeof(buf),
                                        &read_len) != HYBBX_OK) {
                break;
            }
            if (read_len == 0) {
                continue;
            }

            hybbx_circuit_decoder_feed(&sess->dec, buf, read_len,
                                       on_circuit_frame, sess);
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    hybbx_terminal_config_t cfg;
    terminal_session_t sess;
    const char *env;
    int opt;
    int argi;
    hybbx_traffic_config_t traffic;

    static const struct option long_opts[] = {
        {"host", required_argument, NULL, 'H'},
        {"port", required_argument, NULL, 'p'},
        {"link-id", required_argument, NULL, 1000},
        {"link-password", required_argument, NULL, 1001},
        {"link-role", required_argument, NULL, 1002},
        {"mycall", required_argument, NULL, 1003},
        {"dest", required_argument, NULL, 1004},
        {"via", required_argument, NULL, 1005},
        {"ax25-ui", no_argument, NULL, 1006},
        {"baud", required_argument, NULL, 1007},
        {"line-width", required_argument, NULL, 1008},
        {"pace", required_argument, NULL, 1009},
        {"ansi", required_argument, NULL, 1010},
        {"ipv6", no_argument, NULL, '6'},
        {"verbose", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    config_defaults(&cfg);
    hybbx_traffic_config_defaults(&traffic);

    env = getenv("HYBBX_CIRCUIT_HOST");
    if (env != NULL && env[0] != '\0') {
        hybbx_strlcpy(cfg.host, env, sizeof(cfg.host));
    }
    env = getenv("HYBBX_CIRCUIT_PORT");
    if (env != NULL && env[0] != '\0') {
        cfg.port = (unsigned)strtoul(env, NULL, 10);
    }

    while ((opt = getopt_long(argc, argv, "H:p:6vh", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'H':
            hybbx_strlcpy(cfg.host, optarg, sizeof(cfg.host));
            break;
        case 'p':
            cfg.port = (unsigned)strtoul(optarg, NULL, 10);
            break;
        case '6':
            cfg.ipv6 = 1;
            break;
        case 'v':
            cfg.verbose = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        case 1000:
            hybbx_strlcpy(cfg.link_id, optarg, sizeof(cfg.link_id));
            break;
        case 1001:
            hybbx_strlcpy(cfg.link_password, optarg, sizeof(cfg.link_password));
            break;
        case 1002:
            hybbx_strlcpy(cfg.link_role, optarg, sizeof(cfg.link_role));
            break;
        case 1003:
            hybbx_strlcpy(cfg.mycall, optarg, sizeof(cfg.mycall));
            break;
        case 1004:
            hybbx_strlcpy(cfg.dest, optarg, sizeof(cfg.dest));
            break;
        case 1005:
            hybbx_strlcpy(cfg.via, optarg, sizeof(cfg.via));
            break;
        case 1006:
            cfg.ax25_ui = 1;
            break;
        case 1007:
            traffic.baud = (unsigned)strtoul(optarg, NULL, 10);
            break;
        case 1008:
            traffic.line_width = (unsigned)strtoul(optarg, NULL, 10);
            break;
        case 1009:
            traffic.pace_output = hybbx_parse_bool(optarg, traffic.pace_output);
            break;
        case 1010:
            traffic.ansi = hybbx_parse_bool(optarg, traffic.ansi);
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

    memset(&sess, 0, sizeof(sess));
    sess.cfg = &cfg;
    hybbx_circuit_decoder_init(&sess.dec);
    hybbx_client_display_init(&sess.display, &traffic);
    sess.path_ready = build_ax25_path(&cfg, &sess.path);

    if (hybbx_circuit_link_connect(cfg.host, cfg.port, &sess.fd) != HYBBX_OK) {
        fprintf(stderr, "hybbx-terminal: connect %s:%u failed\n",
                cfg.host, cfg.port);
        return EXIT_FAILURE;
    }

    if (cfg.link_password[0] != '\0') {
        if (hybbx_circuit_link_authenticate(sess.fd, cfg.link_password,
                                            cfg.link_role, cfg.link_id) != HYBBX_OK) {
            fprintf(stderr, "hybbx-terminal: link authentication failed\n");
            close(sess.fd);
            return EXIT_FAILURE;
        }
    }

    if (cfg.verbose) {
        fprintf(stderr, "hybbx-terminal: connected to %s:%u (ax25-ui=%s)\n",
                cfg.host, cfg.port, cfg.ax25_ui ? "yes" : "no");
    }

    run_session(&sess);
    close(sess.fd);
    return EXIT_SUCCESS;
}
