#include "ws_proto.h"
#include "ws_sha1.h"
#include "ws_tls.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#define WS_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

static int header_has_token(const char *value, const char *token)
{
    const char *p;

    if (value == NULL || token == NULL) {
        return 0;
    }

    for (p = value; *p != '\0'; p++) {
        if (strncasecmp(p, token, strlen(token)) == 0) {
            return 1;
        }
    }

    return 0;
}

static int header_value_equals(const char *value, const char *expected)
{
    size_t expected_len;

    if (value == NULL || expected == NULL) {
        return 0;
    }

    expected_len = strlen(expected);
    if (strncasecmp(value, expected, expected_len) != 0) {
        return 0;
    }

    {
        char c = value[expected_len];

        return c == '\0' || c == '\r' || c == ' ' || c == '\t' || c == ',';
    }
}

static ssize_t recv_some(int fd, void *buf, size_t len)
{
    ssize_t n;

    n = recv(fd, buf, len, 0);
    if (n < 0 && errno == EINTR) {
        return 0;
    }
    return n;
}

static ssize_t ws_recv_some(hybbx_ws_connection_t *ws, void *buf, size_t len)
{
    if (ws == NULL) {
        return -1;
    }

    if (ws->tls != NULL) {
        return hybbx_ws_tls_recv(ws->tls, buf, len);
    }

    return recv_some(ws->fd, buf, len);
}

static ssize_t send_all(int fd, const void *buf, size_t len)
{
    const uint8_t *p = (const uint8_t *)buf;
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, p + sent, len - sent, MSG_NOSIGNAL);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        sent += (size_t)n;
    }

    return (ssize_t)sent;
}

static ssize_t ws_send_all(hybbx_ws_connection_t *ws, const void *buf,
                           size_t len)
{
    if (ws == NULL) {
        return -1;
    }

    if (ws->tls != NULL) {
        return hybbx_ws_tls_send(ws->tls, buf, len);
    }

    return send_all(ws->fd, buf, len);
}

static int base64_encode(const uint8_t *in, size_t in_len, char *out,
                         size_t out_len)
{
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i;
    size_t o = 0;

    if (out_len < ((in_len + 2) / 3) * 4 + 1) {
        return -1;
    }

    for (i = 0; i < in_len; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16;

        if (i + 1 < in_len) {
            v |= (uint32_t)in[i + 1] << 8;
        }
        if (i + 2 < in_len) {
            v |= (uint32_t)in[i + 2];
        }

        out[o++] = tbl[(v >> 18) & 63u];
        out[o++] = tbl[(v >> 12) & 63u];
        out[o++] = (i + 1 < in_len) ? tbl[(v >> 6) & 63u] : '=';
        out[o++] = (i + 2 < in_len) ? tbl[v & 63u] : '=';
    }

    out[o] = '\0';
    return 0;
}

static const char *header_value(const char *headers, const char *name)
{
    size_t name_len = strlen(name);
    const char *p = headers;

    while (p != NULL && *p != '\0') {
        const char *line_end = strstr(p, "\r\n");

        if (line_end == NULL) {
            break;
        }

        if ((size_t)(line_end - p) > name_len + 2 &&
            strncasecmp(p, name, name_len) == 0 && p[name_len] == ':') {
            const char *value = p + name_len + 1;

            while (*value == ' ' || *value == '\t') {
                value++;
            }
            return value;
        }

        p = line_end + 2;
    }

    return NULL;
}

static int path_matches(const char *request_line, const char *path)
{
    const char *sp1;
    const char *sp2;
    size_t path_len;

    if (path == NULL || path[0] == '\0') {
        return 1;
    }

    sp1 = strchr(request_line, ' ');
    if (sp1 == NULL) {
        return 0;
    }
    sp1++;
    sp2 = strchr(sp1, ' ');
    if (sp2 == NULL) {
        return 0;
    }

    path_len = strlen(path);
    if ((size_t)(sp2 - sp1) < path_len) {
        return 0;
    }
    if (strncmp(sp1, path, path_len) != 0) {
        return 0;
    }
    if (sp1[path_len] != ' ' && sp1[path_len] != '?' &&
        sp1[path_len] != '\0') {
        return 0;
    }

    return 1;
}

void hybbx_ws_connection_init(hybbx_ws_connection_t *ws, int fd)
{
    if (ws == NULL) {
        return;
    }

    memset(ws, 0, sizeof(*ws));
    ws->fd = fd;
}

void hybbx_ws_connection_set_tls(hybbx_ws_connection_t *ws, void *tls)
{
    if (ws == NULL) {
        return;
    }

    ws->tls = tls;
}

void hybbx_ws_connection_cleanup(hybbx_ws_connection_t *ws)
{
    if (ws == NULL) {
        return;
    }

    if (ws->tls != NULL) {
        hybbx_ws_tls_shutdown(ws->tls);
        hybbx_ws_tls_free(ws->tls);
        ws->tls = NULL;
    }
}

hybbx_result_t hybbx_ws_server_handshake(hybbx_ws_connection_t *ws,
                                         const char *path)
{
    char buf[HYBBX_WS_HANDSHAKE_MAX];
    size_t total = 0;
    const char *key_hdr;
    char key[128];
    char accept_src[256];
    uint8_t digest[20];
    char accept_b64[64];
    char response[512];
    const char *upgrade;
    const char *connection;

    if (ws == NULL || ws->fd < 0) {
        return HYBBX_ERR_INVALID;
    }

    while (total + 1 < sizeof(buf)) {
        ssize_t n = ws_recv_some(ws, buf + total, sizeof(buf) - 1 - total);

        if (n < 0) {
            return HYBBX_ERR_IO;
        }
        if (n == 0) {
            return HYBBX_ERR_IO;
        }

        total += (size_t)n;
        buf[total] = '\0';

        if (strstr(buf, "\r\n\r\n") != NULL) {
            break;
        }
    }

    if (strncmp(buf, "GET ", 4) != 0) {
        return HYBBX_ERR_DENIED;
    }

    {
        const char *eol = strstr(buf, "\r\n");
        char first_line[256];
        size_t line_len;

        if (eol == NULL) {
            return HYBBX_ERR_DENIED;
        }
        line_len = (size_t)(eol - buf);
        if (line_len >= sizeof(first_line)) {
            return HYBBX_ERR_DENIED;
        }
        memcpy(first_line, buf, line_len);
        first_line[line_len] = '\0';
        if (!path_matches(first_line, path)) {
            return HYBBX_ERR_DENIED;
        }
    }

    upgrade = header_value(buf, "Upgrade");
    connection = header_value(buf, "Connection");
    if (upgrade == NULL || connection == NULL ||
        !header_value_equals(upgrade, "websocket") ||
        !header_has_token(connection, "upgrade")) {
        return HYBBX_ERR_DENIED;
    }

    key_hdr = header_value(buf, "Sec-WebSocket-Key");
    if (key_hdr == NULL) {
        return HYBBX_ERR_DENIED;
    }

    {
        const char *key_end = strstr(key_hdr, "\r\n");
        size_t key_len;

        if (key_end == NULL) {
            return HYBBX_ERR_DENIED;
        }
        key_len = (size_t)(key_end - key_hdr);
        while (key_len > 0 &&
               (key_hdr[key_len - 1] == ' ' || key_hdr[key_len - 1] == '\t')) {
            key_len--;
        }
        if (key_len >= sizeof(key)) {
            return HYBBX_ERR_DENIED;
        }
        memcpy(key, key_hdr, key_len);
        key[key_len] = '\0';
    }

    snprintf(accept_src, sizeof(accept_src), "%s%s", key, WS_GUID);
    hybbx_ws_sha1((const uint8_t *)accept_src, strlen(accept_src), digest);
    if (base64_encode(digest, sizeof(digest), accept_b64,
                      sizeof(accept_b64)) != 0) {
        return HYBBX_ERR_IO;
    }

    snprintf(response, sizeof(response),
             "HTTP/1.1 101 Switching Protocols\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Accept: %s\r\n"
             "\r\n",
             accept_b64);

    if (ws_send_all(ws, response, strlen(response)) < 0) {
        return HYBBX_ERR_IO;
    }

    ws->established = 1;
    ws->rx_len = 0;
    return HYBBX_OK;
}

static hybbx_result_t ws_handle_frame(hybbx_ws_connection_t *ws,
                                      const uint8_t *frame, size_t frame_len,
                                      hybbx_ws_data_cb on_data, void *ctx)
{
    uint8_t opcode;
    uint64_t payload_len;
    size_t header_len;
    uint8_t mask[4];
    uint8_t payload[HYBBX_WS_FRAME_PAYLOAD_MAX];
    size_t i;

    if (frame_len < 2) {
        return HYBBX_ERR_IO;
    }

    opcode = frame[0] & 0x0Fu;
    if ((frame[0] & 0x80) == 0) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    if ((frame[1] & 0x80) == 0) {
        return HYBBX_ERR_IO;
    }

    payload_len = frame[1] & 0x7Fu;
    header_len = 2;

    if (payload_len == 126) {
        if (frame_len < 4) {
            return HYBBX_ERR_IO;
        }
        payload_len = ((uint64_t)frame[2] << 8) | frame[3];
        header_len = 4;
    } else if (payload_len == 127) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    if (payload_len > HYBBX_WS_FRAME_PAYLOAD_MAX) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    if (frame_len < header_len + 4 + payload_len) {
        return HYBBX_ERR_IO;
    }

    memcpy(mask, frame + header_len, 4);
    header_len += 4;

    for (i = 0; i < payload_len; i++) {
        payload[i] = frame[header_len + i] ^ mask[i % 4];
    }

    switch (opcode) {
    case 0x1:
    case 0x2:
        if (on_data != NULL && payload_len > 0) {
            on_data(ctx, payload, (size_t)payload_len);
        }
        return HYBBX_OK;
    case 0x8:
        return HYBBX_ERR_IO;
    case 0x9:
        {
            uint8_t pong[2];

            pong[0] = 0x8A;
            pong[1] = 0x00;
            (void)ws_send_all(ws, pong, 2);
        }
        return HYBBX_OK;
    case 0xA:
        return HYBBX_OK;
    default:
        return HYBBX_OK;
    }
}

hybbx_result_t hybbx_ws_read_frames(hybbx_ws_connection_t *ws,
                                    hybbx_ws_data_cb on_data,
                                    void *ctx)
{
    uint8_t tmp[256];
    ssize_t n;

    if (ws == NULL || !ws->established) {
        return HYBBX_ERR_INVALID;
    }

    n = ws_recv_some(ws, tmp, sizeof(tmp));
    if (n < 0) {
        return HYBBX_ERR_IO;
    }
    if (n == 0) {
        return HYBBX_ERR_IO;
    }

    if (ws->rx_len + (size_t)n > sizeof(ws->rx_buf)) {
        ws->rx_len = 0;
        return HYBBX_ERR_IO;
    }

    memcpy(ws->rx_buf + ws->rx_len, tmp, (size_t)n);
    ws->rx_len += (size_t)n;

    while (ws->rx_len >= 2) {
        uint64_t payload_len = ws->rx_buf[1] & 0x7Fu;
        size_t header_len = 2;
        size_t frame_len;
        hybbx_result_t rc;

        if (payload_len == 126) {
            if (ws->rx_len < 4) {
                return HYBBX_OK;
            }
            payload_len = ((uint64_t)ws->rx_buf[2] << 8) | ws->rx_buf[3];
            header_len = 4;
        } else if (payload_len == 127) {
            return HYBBX_ERR_UNSUPPORTED;
        }

        if (payload_len > HYBBX_WS_FRAME_PAYLOAD_MAX) {
            return HYBBX_ERR_UNSUPPORTED;
        }

        frame_len = header_len + 4 + (size_t)payload_len;
        if (ws->rx_len < frame_len) {
            return HYBBX_OK;
        }

        rc = ws_handle_frame(ws, ws->rx_buf, frame_len, on_data, ctx);
        if (rc != HYBBX_OK) {
            return rc;
        }

        if (ws->rx_len > frame_len) {
            memmove(ws->rx_buf, ws->rx_buf + frame_len,
                    ws->rx_len - frame_len);
        }
        ws->rx_len -= frame_len;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_ws_write_text(hybbx_ws_connection_t *ws,
                                   const char *data, size_t len)
{
    uint8_t hdr[10];
    size_t hdr_len = 2;

    if (ws == NULL || data == NULL || !ws->established) {
        return HYBBX_ERR_INVALID;
    }

    if (len > HYBBX_WS_FRAME_PAYLOAD_MAX) {
        return HYBBX_ERR_INVALID;
    }

    hdr[0] = 0x81;
    if (len < 126) {
        hdr[1] = (uint8_t)len;
        hdr_len = 2;
    } else {
        hdr[1] = 126;
        hdr[2] = (uint8_t)((len >> 8) & 0xFFu);
        hdr[3] = (uint8_t)(len & 0xFFu);
        hdr_len = 4;
    }

    if (ws_send_all(ws, hdr, hdr_len) < 0) {
        return HYBBX_ERR_IO;
    }
    if (len > 0 && ws_send_all(ws, data, len) < 0) {
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_ws_ping(hybbx_ws_connection_t *ws)
{
    uint8_t frame[2];

    if (ws == NULL || !ws->established) {
        return HYBBX_ERR_INVALID;
    }

    frame[0] = 0x89;
    frame[1] = 0x00;

    if (ws_send_all(ws, frame, sizeof(frame)) < 0) {
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_ws_close(hybbx_ws_connection_t *ws)
{
    uint8_t frame[2];

    if (ws == NULL || ws->fd < 0) {
        return HYBBX_ERR_INVALID;
    }

    frame[0] = 0x88;
    frame[1] = 0x00;
    (void)ws_send_all(ws, frame, 2);
    ws->established = 0;
    return HYBBX_OK;
}
