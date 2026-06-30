#include "hybbx/kiss.h"

#include <string.h>

void hybbx_kiss_decoder_init(hybbx_kiss_decoder_t *dec)
{
    if (dec == NULL) {
        return;
    }

    memset(dec, 0, sizeof(*dec));
}

static void deliver_frame(hybbx_kiss_decoder_t *dec,
                          hybbx_kiss_frame_cb cb, void *userdata)
{
    uint8_t port;
    const uint8_t *payload;
    size_t payload_len;

    if (dec->len < 1) {
        return;
    }

    port = (uint8_t)((dec->buf[0] >> 4) & 0x0Fu);
    payload = dec->buf + 1;
    payload_len = dec->len - 1;

    if (cb != NULL) {
        cb(port, payload, payload_len, userdata);
    }
}

void hybbx_kiss_decoder_feed(hybbx_kiss_decoder_t *dec,
                             const uint8_t *data, size_t len,
                             hybbx_kiss_frame_cb cb, void *userdata)
{
    size_t i;

    if (dec == NULL || data == NULL) {
        return;
    }

    for (i = 0; i < len; i++) {
        uint8_t byte = data[i];

        if (byte == HYBBX_KISS_FEND) {
            if (dec->in_frame && dec->len > 0) {
                deliver_frame(dec, cb, userdata);
            }
            dec->in_frame = 1;
            dec->escape = 0;
            dec->len = 0;
            continue;
        }

        if (!dec->in_frame) {
            continue;
        }

        if (dec->escape) {
            if (byte == HYBBX_KISS_TFEND) {
                byte = HYBBX_KISS_FEND;
            } else if (byte == HYBBX_KISS_TFESC) {
                byte = HYBBX_KISS_FESC;
            }
            dec->escape = 0;
        } else if (byte == HYBBX_KISS_FESC) {
            dec->escape = 1;
            continue;
        }

        if (dec->len >= HYBBX_KISS_MAX_FRAME) {
            dec->len = 0;
            dec->in_frame = 0;
            continue;
        }

        dec->buf[dec->len++] = byte;
    }
}

static size_t kiss_write_byte(uint8_t byte, uint8_t *out, size_t out_cap, size_t pos)
{
    if (pos + 1 > out_cap) {
        return 0;
    }

    if (byte == HYBBX_KISS_FEND) {
        if (pos + 2 > out_cap) {
            return 0;
        }
        out[pos++] = HYBBX_KISS_FESC;
        out[pos++] = HYBBX_KISS_TFEND;
        return pos;
    }

    if (byte == HYBBX_KISS_FESC) {
        if (pos + 2 > out_cap) {
            return 0;
        }
        out[pos++] = HYBBX_KISS_FESC;
        out[pos++] = HYBBX_KISS_TFESC;
        return pos;
    }

    out[pos++] = byte;
    return pos;
}

size_t hybbx_kiss_encode(uint8_t port, hybbx_kiss_command_t cmd,
                         const uint8_t *payload, size_t payload_len,
                         uint8_t *out, size_t out_cap)
{
    size_t pos = 0;
    uint8_t cmd_byte;
    size_t i;

    if (out == NULL || out_cap < 3) {
        return 0;
    }

    if (payload_len > 0 && payload == NULL) {
        return 0;
    }

    if (payload_len + 4 > out_cap) {
        return 0;
    }

    out[pos++] = HYBBX_KISS_FEND;

    cmd_byte = (uint8_t)(((port & 0x0Fu) << 4) | ((uint8_t)cmd & 0x0Fu));
    pos = kiss_write_byte(cmd_byte, out, out_cap, pos);
    if (pos == 0) {
        return 0;
    }

    for (i = 0; i < payload_len; i++) {
        pos = kiss_write_byte(payload[i], out, out_cap, pos);
        if (pos == 0) {
            return 0;
        }
    }

    if (pos + 1 > out_cap) {
        return 0;
    }

    out[pos++] = HYBBX_KISS_FEND;
    return pos;
}
