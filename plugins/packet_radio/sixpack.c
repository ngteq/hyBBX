#include "sixpack.h"

#include <string.h>

void hybbx_sixpack_decoder_init(hybbx_sixpack_decoder_t *dec)
{
    if (dec == NULL) {
        return;
    }

    memset(dec, 0, sizeof(*dec));
}

static void sixpack_reset(hybbx_sixpack_decoder_t *dec)
{
    dec->len = 0;
    dec->count = 0;
    dec->checksum = 0;
    dec->in_frame = 0;
}

static void sixpack_deliver(hybbx_sixpack_decoder_t *dec,
                            hybbx_sixpack_frame_cb cb, void *userdata)
{
    if (cb != NULL && dec->len > 0) {
        cb(dec->buf, dec->len, userdata);
    }

    sixpack_reset(dec);
}

void hybbx_sixpack_decoder_feed(hybbx_sixpack_decoder_t *dec,
                              const uint8_t *data, size_t len,
                              hybbx_sixpack_frame_cb cb, void *userdata)
{
    size_t i;

    if (dec == NULL || data == NULL) {
        return;
    }

    for (i = 0; i < len; i++) {
        uint8_t raw = data[i];
        uint8_t byte = raw;

        if (!dec->in_frame) {
            if (raw == HYBBX_SIXPACK_LEAD) {
                dec->in_frame = 1;
                dec->len = 0;
                dec->count = 0;
                dec->checksum = 0;
            }
            continue;
        }

        if (dec->count == 0) {
            dec->count = raw;
            dec->checksum = raw;
            continue;
        }

        dec->count--;
        dec->checksum = (uint8_t)(dec->checksum + raw);

        if (raw < 0x30u) {
            byte = (uint8_t)(raw + 0x40u);
        }

        if (dec->count > 0) {
            if (dec->len >= HYBBX_KISS_MAX_FRAME) {
                sixpack_reset(dec);
                continue;
            }
            dec->buf[dec->len++] = byte;
            continue;
        }

        if (dec->checksum == 0) {
            sixpack_deliver(dec, cb, userdata);
        } else {
            sixpack_reset(dec);
        }
    }
}

static size_t sixpack_write_encoded(uint8_t byte, uint8_t *out, size_t out_cap,
                                    size_t pos)
{
    if (byte < 0x30u) {
        if (pos + 1 > out_cap) {
            return 0;
        }
        out[pos++] = (uint8_t)(byte + 0x40u);
        return pos;
    }

    if (pos + 1 > out_cap) {
        return 0;
    }

    out[pos++] = byte;
    return pos;
}

size_t hybbx_sixpack_encode(const uint8_t *frame, size_t frame_len,
                            uint8_t *out, size_t out_cap)
{
    uint8_t scratch[HYBBX_KISS_MAX_FRAME + 4];
    size_t enc_len = 0;
    size_t i;
    uint8_t count;
    uint8_t sum;
    size_t pos = 0;

    if (frame == NULL || out == NULL || frame_len == 0 ||
        frame_len > HYBBX_KISS_MAX_FRAME) {
        return 0;
    }

    for (i = 0; i < frame_len; i++) {
        enc_len = sixpack_write_encoded(frame[i], scratch, sizeof(scratch),
                                        enc_len);
        if (enc_len == 0) {
            return 0;
        }
    }

    if (enc_len + 4 > out_cap || enc_len + 1 > 255) {
        return 0;
    }

    count = (uint8_t)(enc_len + 1);
    sum = count;
    for (i = 0; i < enc_len; i++) {
        sum = (uint8_t)(sum + scratch[i]);
    }

    out[pos++] = HYBBX_SIXPACK_LEAD;
    out[pos++] = count;
    for (i = 0; i < enc_len; i++) {
        out[pos++] = scratch[i];
    }
    out[pos++] = (uint8_t)(0x100u - (unsigned)sum);

    return pos;
}
