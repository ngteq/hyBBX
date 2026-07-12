#include "ardop_host.h"
#include "ardop_crc.h"

#include "hybbx/socket.h"
#include "hybbx/util.h"
#include "hybbx/log.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ARDOP_HOST_RX_MAX 8192u
#define ARDOP_HOST_CMD_MAX 256u
#define ARDOP_HOST_FRAME_MAX (HYBBX_ARDOP_DATA_MAX + 64u)

struct ardop_host {
    hybbx_ardop_config_t cfg;
    ardop_host_data_fn on_data;
    ardop_host_event_fn on_event;
    void *userdata;
    int ctrl_fd;
    int data_fd;
    int tnc_ready;
    int link_up;
    char peer_call[16];
    unsigned char ctrl_rx[ARDOP_HOST_RX_MAX];
    size_t ctrl_len;
    unsigned char data_rx[ARDOP_HOST_RX_MAX];
    size_t data_len;
    int init_sent;
    const char *log_tag;
};

static const char *host_log_tag(const ardop_host_t *host)
{
    if (host != NULL && host->log_tag != NULL && host->log_tag[0] != '\0') {
        return host->log_tag;
    }
    return "ardop";
}

static const char *host_modem_name(const ardop_host_t *host)
{
    if (host != NULL && host->log_tag != NULL &&
        strcmp(host->log_tag, "crdop") == 0) {
        return "CRDOPC";
    }
    return "ARDOPC";
}

static int str_ieq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }

    while (*a != '\0' && *b != '\0') {
        char ca = (char)(*a >= 'A' && *a <= 'Z' ? *a + 32 : *a);
        char cb = (char)(*b >= 'A' && *b <= 'Z' ? *b + 32 : *b);

        if (ca != cb) {
            return 0;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static void ardop_host_fire_event(ardop_host_t *host, const char *event,
                                  const char *detail)
{
    if (host != NULL && host->on_event != NULL) {
        host->on_event(event, detail, host->userdata);
    }
}

static int ardop_host_set_socket_opts(int fd)
{
    int on = 1;

    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on)) != 0) {
        return -1;
    }

    return 0;
}

static int ardop_host_tcp_connect(const char *host, unsigned port)
{
    struct sockaddr_in addr;
    int fd;
    int rc;

    if (host == NULL || host[0] == '\0' || port == 0) {
        return -1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    if (ardop_host_set_socket_opts(fd) != 0) {
        close(fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        close(fd);
        return -1;
    }

    rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static hybbx_result_t ardop_host_write_all(int fd, const unsigned char *buf,
                                           size_t len)
{
    size_t off = 0;

    if (fd < 0 || buf == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    while (off < len) {
        ssize_t n = send(fd, buf + off, len - off, MSG_NOSIGNAL);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return HYBBX_ERR_IO;
        }
        if (n == 0) {
            return HYBBX_ERR_IO;
        }
        off += (size_t)n;
    }

    return HYBBX_OK;
}

static hybbx_result_t ardop_host_send_cmd_frame(ardop_host_t *host,
                                                const char *cmd)
{
    unsigned char frame[ARDOP_HOST_CMD_MAX + 4];
    size_t len;

    if (host == NULL || cmd == NULL || host->ctrl_fd < 0) {
        return HYBBX_ERR_INVALID;
    }

    len = strlen(cmd);
    if (len + 3 > sizeof(frame)) {
        return HYBBX_ERR_INVALID;
    }

    memcpy(frame, cmd, len);
    frame[len++] = '\r';
    len = ardop_crc16_append(frame, len, sizeof(frame));
    if (len == 0) {
        return HYBBX_ERR_IO;
    }

    return ardop_host_write_all(host->ctrl_fd, frame, len);
}

static hybbx_result_t ardop_host_send_rdy(ardop_host_t *host)
{
    return ardop_host_send_cmd_frame(host, "RDY");
}

static void ardop_host_strip_prefix(char *line)
{
    if (line == NULL) {
        return;
    }

    if (strncmp(line, "c:", 2) == 0 || strncmp(line, "C:", 2) == 0) {
        memmove(line, line + 2, strlen(line + 2) + 1);
    }
}

static void ardop_host_handle_ctrl_line(ardop_host_t *host, char *line)
{
    char *sp;

    if (host == NULL || line == NULL) {
        return;
    }

    ardop_host_strip_prefix(line);

    if (str_ieq(line, "RDY")) {
        host->tnc_ready = 1;
        return;
    }

    if (strncmp(line, "VERSION", 7) == 0) {
        hybbx_log_debug("[%s] modem %s", host_log_tag(host), line);
        (void)ardop_host_send_rdy(host);
        return;
    }

    if (strncmp(line, "CONNECTED", 9) == 0) {
        char *tok;

        sp = line + 9;
        while (*sp == ' ') {
            sp++;
        }
        hybbx_strlcpy(host->peer_call, sp, sizeof(host->peer_call));
        tok = strchr(host->peer_call, ' ');
        if (tok != NULL) {
            *tok = '\0';
        }
        host->link_up = 1;
        ardop_host_fire_event(host, "connected", host->peer_call);
        (void)ardop_host_send_rdy(host);
        return;
    }

    if (str_ieq(line, "DISCONNECTED")) {
        host->link_up = 0;
        host->peer_call[0] = '\0';
        ardop_host_fire_event(host, "disconnected", NULL);
        (void)ardop_host_send_rdy(host);
        return;
    }

    if (strncmp(line, "FAULT", 5) == 0 ||
        strncmp(line, "CRCFAULT", 8) == 0) {
        hybbx_log_warn("[%s] TNC fault: %s", host_log_tag(host), line);
        (void)ardop_host_send_rdy(host);
        return;
    }

    if (strncmp(line, "BUSY", 4) == 0 ||
        strncmp(line, "NEWSTATE", 8) == 0 ||
        strncmp(line, "BUFFER", 6) == 0 ||
        strncmp(line, "STATUS", 6) == 0 ||
        strncmp(line, "PENDING", 7) == 0 ||
        strncmp(line, "TARGET", 6) == 0) {
        (void)ardop_host_send_rdy(host);
        return;
    }

    (void)ardop_host_send_rdy(host);
}

static void ardop_host_process_ctrl_rx(ardop_host_t *host)
{
    unsigned char *cr;
    unsigned char frame[ARDOP_HOST_CMD_MAX + 8];
    size_t msg_len;
    char line[ARDOP_HOST_CMD_MAX];

    if (host == NULL) {
        return;
    }

    for (;;) {
        cr = memchr(host->ctrl_rx, '\r', host->ctrl_len);
        if (cr == NULL) {
            return;
        }

        msg_len = (size_t)(cr - host->ctrl_rx) + 1u + 2u;
        if (host->ctrl_len < msg_len) {
            return;
        }

        if (msg_len > sizeof(frame)) {
            host->ctrl_len = 0;
            return;
        }

        memcpy(frame, host->ctrl_rx, msg_len);
        if (!ardop_crc16_valid(frame, (unsigned short)msg_len)) {
            hybbx_log_warn("[%s] control CRC fault", host_log_tag(host));
            host->ctrl_len = 0;
            return;
        }

        frame[msg_len - 3] = '\0';
        hybbx_strlcpy(line, (const char *)frame, sizeof(line));
        ardop_host_handle_ctrl_line(host, line);

        memmove(host->ctrl_rx, host->ctrl_rx + msg_len, host->ctrl_len - msg_len);
        host->ctrl_len -= msg_len;
    }
}

static void ardop_host_process_data_rx(ardop_host_t *host)
{
    size_t off = 0;

    if (host == NULL) {
        return;
    }

    while (host->data_len >= off + 6u) {
        const unsigned char *buf = host->data_rx + off;
        size_t remain = host->data_len - off;
        size_t payload_len;
        size_t frame_len;
        const uint8_t *payload;
        const char *tag;

        if (buf[0] != 'd' && buf[0] != 'D') {
            off++;
            continue;
        }
        if (buf[1] != ':') {
            off++;
            continue;
        }

        payload_len = ((size_t)buf[2] << 8) | (size_t)buf[3];
        if (payload_len < 3 || payload_len > HYBBX_ARDOP_DATA_MAX + 8u) {
            off++;
            continue;
        }

        frame_len = 2u + 2u + payload_len + 2u;
        if (remain < frame_len) {
            break;
        }

        if (!ardop_crc16_valid(buf, (unsigned short)frame_len)) {
            off++;
            continue;
        }

        tag = (const char *)(buf + 4);
        payload = buf + 7;
        payload_len -= 3u;

        if (strncmp(tag, "ARQ", 3) == 0 && host->on_data != NULL &&
            payload_len > 0) {
            host->on_data(payload, payload_len, host->userdata);
        }

        (void)ardop_host_send_cmd_frame(host, "RDY");
        off += frame_len;
    }

    if (off > 0) {
        memmove(host->data_rx, host->data_rx + off, host->data_len - off);
        host->data_len -= off;
    }
}

static hybbx_result_t ardop_host_send_init(ardop_host_t *host)
{
    char cmd[96];
    hybbx_result_t rc;

    if (host == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = ardop_host_send_cmd_frame(host, "INITIALIZE");
    if (rc != HYBBX_OK) {
        return rc;
    }

    snprintf(cmd, sizeof(cmd), "MYCALL %s",
             host->cfg.mycall != NULL ? host->cfg.mycall : "NOCALL");
    rc = ardop_host_send_cmd_frame(host, cmd);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = ardop_host_send_cmd_frame(host, "PROTOCOLMODE ARQ");
    if (rc != HYBBX_OK) {
        return rc;
    }

    snprintf(cmd, sizeof(cmd), "ARQBW %s",
             host->cfg.arq_bandwidth != NULL ? host->cfg.arq_bandwidth :
             "500MAX");
    rc = ardop_host_send_cmd_frame(host, cmd);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = ardop_host_send_cmd_frame(host,
                                   host->cfg.listen ? "LISTEN TRUE" :
                                   "LISTEN FALSE");
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (host->cfg.peer_call != NULL && host->cfg.peer_call[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "ARQCALL %s 5", host->cfg.peer_call);
        rc = ardop_host_send_cmd_frame(host, cmd);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    host->init_sent = 1;
    hybbx_log_info("[%s] host init sent (external %s at %s:%u)",
                   host_log_tag(host), host_modem_name(host),
                   host->cfg.ardop_host != NULL ? host->cfg.ardop_host : "?",
                   host->cfg.ardop_port);
    return HYBBX_OK;
}

static void ardop_host_read_fd(int fd, unsigned char *buf, size_t *len,
                               size_t cap)
{
    ssize_t n;

    if (fd < 0 || buf == NULL || len == NULL || *len >= cap) {
        return;
    }

    n = recv(fd, buf + *len, cap - *len, 0);
    if (n > 0) {
        *len += (size_t)n;
    } else if (n == 0) {
        close(fd);
    }
}

ardop_host_t *ardop_host_create(const hybbx_ardop_config_t *cfg,
                                ardop_host_data_fn on_data,
                                ardop_host_event_fn on_event,
                                void *userdata,
                                const char *log_tag)
{
    ardop_host_t *host;

    if (cfg == NULL) {
        return NULL;
    }

    host = calloc(1, sizeof(*host));
    if (host == NULL) {
        return NULL;
    }

    host->cfg = *cfg;
    host->on_data = on_data;
    host->on_event = on_event;
    host->userdata = userdata;
    host->ctrl_fd = -1;
    host->data_fd = -1;
    host->log_tag = log_tag;
    return host;
}

void ardop_host_destroy(ardop_host_t *host)
{
    if (host == NULL) {
        return;
    }

    ardop_host_disconnect(host);
    free(host);
}

hybbx_result_t ardop_host_connect(ardop_host_t *host)
{
    const char *h;
    unsigned port;

    if (host == NULL) {
        return HYBBX_ERR_INVALID;
    }

    ardop_host_disconnect(host);

    h = host->cfg.ardop_host;
    if (h == NULL || h[0] == '\0') {
        h = "127.0.0.1";
    }
    port = host->cfg.ardop_port;
    if (port == 0) {
        port = HYBBX_ARDOP_DEFAULT_PORT;
    }

    host->ctrl_fd = ardop_host_tcp_connect(h, port);
    if (host->ctrl_fd < 0) {
        hybbx_log_warn("[%s] cannot connect to %s control %s:%u",
                       host_log_tag(host), host_modem_name(host), h, port);
        return HYBBX_ERR_IO;
    }

    host->data_fd = ardop_host_tcp_connect(h, port + 1u);
    if (host->data_fd < 0) {
        hybbx_log_warn("[%s] cannot connect to %s data %s:%u",
                       host_log_tag(host), host_modem_name(host), h, port + 1u);
        ardop_host_disconnect(host);
        return HYBBX_ERR_IO;
    }

    host->tnc_ready = 0;
    host->init_sent = 0;
    hybbx_log_info("[%s] connected to external %s %s:%u (data %u)",
                   host_log_tag(host), host_modem_name(host), h, port, port + 1u);

    (void)ardop_host_send_init(host);
    {
        unsigned spin;

        for (spin = 0; spin < 8u; spin++) {
            ardop_host_poll(host);
            if (host->tnc_ready) {
                break;
            }
        }
    }
    return HYBBX_OK;
}

void ardop_host_disconnect(ardop_host_t *host)
{
    if (host == NULL) {
        return;
    }

    if (host->ctrl_fd >= 0) {
        close(host->ctrl_fd);
        host->ctrl_fd = -1;
    }
    if (host->data_fd >= 0) {
        close(host->data_fd);
        host->data_fd = -1;
    }

    host->ctrl_len = 0;
    host->data_len = 0;
    host->tnc_ready = 0;
    host->link_up = 0;
    host->init_sent = 0;
    host->peer_call[0] = '\0';
}

void ardop_host_poll(ardop_host_t *host)
{
    struct pollfd pfds[2];
    int count = 0;
    int pr;

    if (host == NULL || host->ctrl_fd < 0) {
        return;
    }

    pfds[0].fd = host->ctrl_fd;
    pfds[0].events = POLLIN;
    count = 1;

    if (host->data_fd >= 0) {
        pfds[1].fd = host->data_fd;
        pfds[1].events = POLLIN;
        count = 2;
    }

    pr = poll(pfds, (nfds_t)count, 0);
    if (pr <= 0) {
        if (host->tnc_ready && !host->init_sent) {
            (void)ardop_host_send_init(host);
        }
        return;
    }

    if ((pfds[0].revents & POLLIN) != 0) {
        ardop_host_read_fd(host->ctrl_fd, host->ctrl_rx, &host->ctrl_len,
                           ARDOP_HOST_RX_MAX);
        ardop_host_process_ctrl_rx(host);
    }

    if (count > 1 && (pfds[1].revents & POLLIN) != 0) {
        ardop_host_read_fd(host->data_fd, host->data_rx, &host->data_len,
                           ARDOP_HOST_RX_MAX);
        ardop_host_process_data_rx(host);
    }

    if (host->tnc_ready && !host->init_sent) {
        (void)ardop_host_send_init(host);
    }
}

int ardop_host_link_up(const ardop_host_t *host)
{
    return host != NULL && host->link_up;
}

int ardop_host_modem_connected(const ardop_host_t *host)
{
    return host != NULL && host->ctrl_fd >= 0;
}

hybbx_result_t ardop_host_send_data(ardop_host_t *host,
                                    const uint8_t *data, size_t len)
{
    unsigned char frame[ARDOP_HOST_FRAME_MAX];
    size_t frame_len;

    if (host == NULL || data == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    if (!host->link_up || host->data_fd < 0) {
        return HYBBX_ERR_BUSY;
    }

    if (len > HYBBX_ARDOP_DATA_MAX) {
        return HYBBX_ERR_INVALID;
    }

    if (len + 8 > sizeof(frame)) {
        return HYBBX_ERR_INVALID;
    }

    frame[0] = 'D';
    frame[1] = ':';
    frame[2] = (unsigned char)((len >> 8) & 0xff);
    frame[3] = (unsigned char)(len & 0xff);
    memcpy(frame + 4, data, len);

    frame_len = 4u + len;
    frame_len = ardop_crc16_append(frame, frame_len, sizeof(frame));
    if (frame_len == 0) {
        return HYBBX_ERR_IO;
    }

    return ardop_host_write_all(host->data_fd, frame, frame_len);
}
