#include "hybbx/circuit.h"

#include <string.h>

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static void write_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)((v >> 24) & 0xFFu);
    p[1] = (uint8_t)((v >> 16) & 0xFFu);
    p[2] = (uint8_t)((v >> 8) & 0xFFu);
    p[3] = (uint8_t)(v & 0xFFu);
}

static void write_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)((v >> 8) & 0xFFu);
    p[1] = (uint8_t)(v & 0xFFu);
}

const char *hybbx_circuit_proto_name(hybbx_circuit_proto_t proto)
{
    switch (proto) {
    case HYBBX_CIRCUIT_PROTO_AX25:
        return "ax25";
    case HYBBX_CIRCUIT_PROTO_AX25_UI:
        return "ax25_ui";
    case HYBBX_CIRCUIT_PROTO_LINK_AUTH:
        return "link_auth";
    case HYBBX_CIRCUIT_PROTO_LINK_AUTH_ACK:
        return "link_auth_ack";
    case HYBBX_CIRCUIT_PROTO_FLOW_CTRL:
        return "flow_ctrl";
    case HYBBX_CIRCUIT_PROTO_TERMINAL:
        return "terminal";
    case HYBBX_CIRCUIT_PROTO_RESERVED_APRS:
        return "aprs";
    case HYBBX_CIRCUIT_PROTO_RESERVED_NETROM:
        return "netrom";
    default:
        return "unknown";
    }
}

void hybbx_circuit_decoder_init(hybbx_circuit_decoder_t *dec)
{
    if (dec == NULL) {
        return;
    }

    memset(dec, 0, sizeof(*dec));
}

static void circuit_deliver(hybbx_circuit_decoder_t *dec,
                            hybbx_circuit_frame_cb cb, void *userdata)
{
    const uint8_t *payload;
    size_t payload_len;

    if (dec->len < HYBBX_CIRCUIT_HEADER_SIZE) {
        dec->len = 0;
        dec->need = 0;
        dec->have_header = 0;
        return;
    }

    payload = dec->buf + HYBBX_CIRCUIT_HEADER_SIZE;
    payload_len = dec->len - HYBBX_CIRCUIT_HEADER_SIZE;

    if (cb != NULL) {
        cb(dec->proto, dec->flags, payload, payload_len, userdata);
    }

    dec->len = 0;
    dec->need = 0;
    dec->have_header = 0;
}

static int circuit_parse_header(hybbx_circuit_decoder_t *dec)
{
    const uint8_t *h = dec->buf;

    if (h[0] != HYBBX_CIRCUIT_MAGIC_0 || h[1] != HYBBX_CIRCUIT_MAGIC_1 ||
        h[2] != HYBBX_CIRCUIT_MAGIC_2 || h[3] != HYBBX_CIRCUIT_VERSION) {
        return 0;
    }

    dec->proto = (hybbx_circuit_proto_t)h[4];
    dec->flags = read_be16(h + 5);
    dec->need = (size_t)read_be32(h + 7);
    if (dec->need > HYBBX_CIRCUIT_MAX_PAYLOAD) {
        return 0;
    }

    return 1;
}

void hybbx_circuit_decoder_feed(hybbx_circuit_decoder_t *dec,
                                const uint8_t *data, size_t len,
                                hybbx_circuit_frame_cb cb, void *userdata)
{
    size_t i;

    if (dec == NULL || data == NULL) {
        return;
    }

    for (i = 0; i < len; i++) {
        if (dec->len >= sizeof(dec->buf)) {
            hybbx_circuit_decoder_init(dec);
        }

        dec->buf[dec->len++] = data[i];

        if (!dec->have_header) {
            if (dec->len < HYBBX_CIRCUIT_HEADER_SIZE) {
                continue;
            }
            if (!circuit_parse_header(dec)) {
                memmove(dec->buf, dec->buf + 1, dec->len - 1);
                dec->len--;
                continue;
            }
            dec->have_header = 1;
        }

        if (dec->len < HYBBX_CIRCUIT_HEADER_SIZE + dec->need) {
            continue;
        }

        circuit_deliver(dec, cb, userdata);
    }
}

size_t hybbx_circuit_encode(hybbx_circuit_proto_t proto, uint16_t flags,
                            const uint8_t *payload, size_t payload_len,
                            uint8_t *out, size_t out_cap)
{
    if (out == NULL || out_cap < HYBBX_CIRCUIT_HEADER_SIZE) {
        return 0;
    }

    if (payload_len > HYBBX_CIRCUIT_MAX_PAYLOAD) {
        return 0;
    }

    if (payload_len > 0 && payload == NULL) {
        return 0;
    }

    if (HYBBX_CIRCUIT_HEADER_SIZE + payload_len > out_cap) {
        return 0;
    }

    out[0] = HYBBX_CIRCUIT_MAGIC_0;
    out[1] = HYBBX_CIRCUIT_MAGIC_1;
    out[2] = HYBBX_CIRCUIT_MAGIC_2;
    out[3] = HYBBX_CIRCUIT_VERSION;
    out[4] = (uint8_t)proto;
    write_be16(out + 5, flags);

    write_be32(out + 7, (uint32_t)payload_len);
    if (payload_len > 0) {
        memcpy(out + HYBBX_CIRCUIT_HEADER_SIZE, payload, payload_len);
    }

    return HYBBX_CIRCUIT_HEADER_SIZE + payload_len;
}

size_t hybbx_circuit_encode_ax25(const uint8_t *frame, size_t frame_len,
                                 uint16_t flags,
                                 uint8_t *out, size_t out_cap)
{
    return hybbx_circuit_encode(HYBBX_CIRCUIT_PROTO_AX25,
                                (uint16_t)(flags | HYBBX_CIRCUIT_FLAG_RX),
                                frame, frame_len, out, out_cap);
}

static size_t pack_path_payload(const hybbx_ax25_path_t *path,
                                const uint8_t *payload, size_t payload_len,
                                uint8_t *out, size_t out_cap)
{
    size_t pos = 0;
    unsigned i;

    if (path == NULL || out == NULL) {
        return 0;
    }

    if (1 + payload_len > out_cap) {
        return 0;
    }

    out[pos++] = (uint8_t)path->digi_count;
    if (pos + HYBBX_AX25_ADDR_ENCODED * (2 + path->digi_count) > out_cap) {
        return 0;
    }

    hybbx_ax25_encode_address(out + pos, &path->dest, 0);
    pos += HYBBX_AX25_ADDR_ENCODED;
    for (i = 0; i < path->digi_count; i++) {
        hybbx_ax25_encode_address(out + pos, &path->digi[i], 0);
        pos += HYBBX_AX25_ADDR_ENCODED;
    }
    hybbx_ax25_encode_address(out + pos, &path->source, 1);
    pos += HYBBX_AX25_ADDR_ENCODED;

    if (payload_len > 0) {
        if (pos + payload_len > out_cap) {
            return 0;
        }
        memcpy(out + pos, payload, payload_len);
        pos += payload_len;
    }

    return pos;
}

size_t hybbx_circuit_encode_ax25_ui(const hybbx_ax25_path_t *path,
                                    const uint8_t *payload, size_t payload_len,
                                    uint16_t flags,
                                    uint8_t *out, size_t out_cap)
{
    uint8_t body[HYBBX_CIRCUIT_MAX_PAYLOAD];
    size_t body_len;

    body_len = pack_path_payload(path, payload, payload_len, body, sizeof(body));
    if (body_len == 0 && payload_len > 0) {
        return 0;
    }

    return hybbx_circuit_encode(HYBBX_CIRCUIT_PROTO_AX25_UI,
                                (uint16_t)(flags | HYBBX_CIRCUIT_FLAG_PATH),
                                body, body_len, out, out_cap);
}

size_t hybbx_circuit_encode_terminal(const char *data, size_t len,
                                     uint8_t *out, size_t out_cap)
{
    return hybbx_circuit_encode(HYBBX_CIRCUIT_PROTO_TERMINAL,
                                HYBBX_CIRCUIT_FLAG_TX,
                                (const uint8_t *)data, len, out, out_cap);
}

size_t hybbx_circuit_encode_link_msg(hybbx_circuit_proto_t proto,
                                     const char *payload, size_t payload_len,
                                     uint8_t *out, size_t out_cap)
{
    if (proto != HYBBX_CIRCUIT_PROTO_LINK_AUTH &&
        proto != HYBBX_CIRCUIT_PROTO_LINK_AUTH_ACK) {
        return 0;
    }

    return hybbx_circuit_encode(proto, HYBBX_CIRCUIT_FLAG_NONE,
                                (const uint8_t *)payload, payload_len,
                                out, out_cap);
}

static void decode_address_field(const uint8_t *in, hybbx_ax25_address_t *out)
{
    size_t i;

    for (i = 0; i < HYBBX_AX25_CALL_MAX; i++) {
        char ch = (char)((in[i] >> 1) & 0x7Fu);
        out->call[i] = ch == ' ' ? '\0' : ch;
    }
    out->call[HYBBX_AX25_CALL_MAX] = '\0';
    out->ssid = (unsigned)((in[6] >> 1) & 0x0Fu);
}

size_t hybbx_circuit_unpack_ax25_ui(const uint8_t *payload, size_t payload_len,
                                    hybbx_ax25_path_t *path,
                                    uint8_t *ui, size_t ui_cap)
{
    size_t pos = 0;
    unsigned digi_count;
    unsigned i;

    if (payload == NULL || payload_len < 1) {
        return 0;
    }

    if (path != NULL) {
        memset(path, 0, sizeof(*path));
    }

    digi_count = payload[0];
    if (digi_count > HYBBX_AX25_MAX_DIGI) {
        return 0;
    }

    pos = 1;
    if (pos + HYBBX_AX25_ADDR_ENCODED * (size_t)(2 + digi_count) > payload_len) {
        return 0;
    }

    if (path != NULL) {
        decode_address_field(payload + pos, &path->dest);
        pos += HYBBX_AX25_ADDR_ENCODED;
        for (i = 0; i < digi_count; i++) {
            decode_address_field(payload + pos, &path->digi[i]);
            pos += HYBBX_AX25_ADDR_ENCODED;
        }
        path->digi_count = digi_count;
        decode_address_field(payload + pos, &path->source);
    } else {
        pos += HYBBX_AX25_ADDR_ENCODED * (size_t)(2 + digi_count);
    }
    pos += HYBBX_AX25_ADDR_ENCODED;

    {
        size_t ui_len = payload_len - pos;

        if (ui_len > ui_cap) {
            return 0;
        }

        if (ui_len > 0 && ui != NULL) {
            memcpy(ui, payload + pos, ui_len);
        }

        return ui_len;
    }
}
