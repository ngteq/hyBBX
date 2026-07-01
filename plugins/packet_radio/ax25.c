#include "hybbx/ax25.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static void address_upper(char *call)
{
    size_t i;

    for (i = 0; call[i] != '\0'; i++) {
        call[i] = (char)toupper((unsigned char)call[i]);
    }
}

hybbx_result_t hybbx_ax25_address_parse(const char *text,
                                        hybbx_ax25_address_t *out)
{
    const char *dash;
    size_t call_len;
    char *end = NULL;
    unsigned long ssid;

    if (text == NULL || out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    memset(out, 0, sizeof(*out));
    dash = strchr(text, '-');
    if (dash != NULL) {
        call_len = (size_t)(dash - text);
        ssid = strtoul(dash + 1, &end, 10);
        if (end == dash + 1 || ssid > HYBBX_AX25_SSID_MAX) {
            return HYBBX_ERR_INVALID;
        }
        out->ssid = (unsigned)ssid;
    } else {
        call_len = strlen(text);
    }

    if (call_len == 0 || call_len > HYBBX_AX25_CALL_MAX) {
        return HYBBX_ERR_INVALID;
    }

    memcpy(out->call, text, call_len);
    out->call[call_len] = '\0';
    address_upper(out->call);
    return HYBBX_OK;
}

void hybbx_ax25_encode_address(uint8_t *out, const hybbx_ax25_address_t *addr,
                               int last)
{
    size_t i;
    char padded[HYBBX_AX25_CALL_MAX];

    memset(padded, ' ', sizeof(padded));
    for (i = 0; i < HYBBX_AX25_CALL_MAX && addr->call[i] != '\0'; i++) {
        padded[i] = addr->call[i];
    }

    for (i = 0; i < HYBBX_AX25_CALL_MAX; i++) {
        out[i] = (uint8_t)((unsigned char)padded[i] << 1);
    }

    out[6] = (uint8_t)(((addr->ssid & 0x0Fu) << 1) | 0x60u);
    if (last) {
        out[6] |= 0x01u;
    }
}

uint16_t hybbx_ax25_crc(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFu;
    size_t i;
    int bit;

    for (i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0; bit < 8; bit++) {
            if (crc & 0x8000u) {
                crc = (uint16_t)((crc << 1) ^ 0x1021u);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

static size_t append_addresses(const hybbx_ax25_path_t *path,
                               uint8_t *out, size_t out_cap)
{
    size_t pos = 0;
    unsigned i;

    if (out_cap < (size_t)(2 + path->digi_count) * HYBBX_AX25_ADDR_ENCODED) {
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
    return pos;
}

size_t hybbx_ax25_build_ui(const hybbx_ax25_path_t *path,
                           const uint8_t *payload, size_t payload_len,
                           uint8_t *out, size_t out_cap)
{
    size_t pos;
    uint16_t crc;

    if (path == NULL || out == NULL ||
        (payload_len > 0 && payload == NULL)) {
        return 0;
    }

    if (payload_len + 32 > out_cap || payload_len > HYBBX_AX25_FRAME_MAX) {
        return 0;
    }

    pos = append_addresses(path, out, out_cap);
    if (pos + 4 + payload_len > out_cap) {
        return 0;
    }

    out[pos++] = HYBBX_AX25_CTRL_UI;
    out[pos++] = HYBBX_AX25_PID_NOL3;

    if (payload_len > 0) {
        memcpy(out + pos, payload, payload_len);
        pos += payload_len;
    }

    crc = hybbx_ax25_crc(out, pos);
    out[pos++] = (uint8_t)(crc & 0xFFu);
    out[pos++] = (uint8_t)((crc >> 8) & 0xFFu);
    return pos;
}

static void decode_address(const uint8_t *in, hybbx_ax25_address_t *out)
{
    size_t i;

    for (i = 0; i < HYBBX_AX25_CALL_MAX; i++) {
        char ch = (char)((in[i] >> 1) & 0x7Fu);
        out->call[i] = ch == ' ' ? '\0' : ch;
    }
    out->call[HYBBX_AX25_CALL_MAX] = '\0';

    for (i = HYBBX_AX25_CALL_MAX; i > 0; i--) {
        if (out->call[i - 1] != '\0') {
            break;
        }
    }
    out->call[i] = '\0';
    out->ssid = (unsigned)((in[6] >> 1) & 0x0Fu);
}

size_t hybbx_ax25_parse_ui(const uint8_t *frame, size_t frame_len,
                           hybbx_ax25_path_t *path,
                           uint8_t *payload, size_t payload_cap)
{
    size_t pos = 0;
    unsigned digi = 0;
    uint16_t crc_rx;
    uint16_t crc_calc;

    if (frame == NULL || frame_len < 16) {
        return 0;
    }

    if (path != NULL) {
        memset(path, 0, sizeof(*path));
    }

    while (pos + HYBBX_AX25_ADDR_ENCODED <= frame_len) {
        int last = frame[pos + 6] & 0x01;
        hybbx_ax25_address_t tmp;

        decode_address(frame + pos, &tmp);
        if (path != NULL) {
            if (digi == 0) {
                path->dest = tmp;
            } else if (!last) {
                if (digi - 1 < HYBBX_AX25_MAX_DIGI) {
                    path->digi[digi - 1] = tmp;
                    path->digi_count = digi;
                }
            } else {
                path->source = tmp;
            }
        }

        pos += HYBBX_AX25_ADDR_ENCODED;
        digi++;

        if (last) {
            break;
        }

        if (digi > HYBBX_AX25_MAX_DIGI + 1) {
            return 0;
        }
    }

    if (pos + 4 > frame_len) {
        return 0;
    }

    if (frame[pos] != HYBBX_AX25_CTRL_UI) {
        return 0;
    }

    pos += 2; /* control + PID */

    if (frame_len < pos + 2) {
        return 0;
    }

    crc_rx = (uint16_t)frame[frame_len - 2] |
             ((uint16_t)frame[frame_len - 1] << 8);
    crc_calc = hybbx_ax25_crc(frame, frame_len - 2);
    if (crc_rx != crc_calc) {
        return 0;
    }

    {
        size_t payload_len = frame_len - pos - 2;

        if (payload_len > payload_cap) {
            return 0;
        }

        if (payload_len > 0 && payload != NULL) {
            memcpy(payload, frame + pos, payload_len);
        }

        return payload_len;
    }
}
