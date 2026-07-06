/*
 * Minimal SHA-1 for WebSocket handshake (RFC 6455). Public-domain style.
 */
#include "ws_sha1.h"

#include <stdint.h>
#include <string.h>
#include <string.h>

typedef struct ws_sha1_ctx {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
} ws_sha1_ctx_t;

static uint32_t rol(uint32_t v, unsigned bits)
{
    return (v << bits) | (v >> (32u - bits));
}

static void ws_sha1_transform(ws_sha1_ctx_t *ctx, const uint8_t block[64])
{
    uint32_t w[80];
    uint32_t a, b, c, d, e, f, k, t;
    unsigned i;

    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               (uint32_t)block[i * 4 + 3];
    }
    for (i = 16; i < 80; i++) {
        w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];

    for (i = 0; i < 80; i++) {
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6u;
        }
        t = rol(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = t;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
}

static void ws_sha1_init(ws_sha1_ctx_t *ctx)
{
    ctx->state[0] = 0x67452301u;
    ctx->state[1] = 0xEFCDAB89u;
    ctx->state[2] = 0x98BADCFEu;
    ctx->state[3] = 0x10325476u;
    ctx->state[4] = 0xC3D2E1F0u;
    ctx->count = 0;
}

static void ws_sha1_update(ws_sha1_ctx_t *ctx, const uint8_t *data, size_t len)
{
    size_t i = 0;
    size_t idx = (size_t)((ctx->count >> 3) & 63u);

    ctx->count += (uint64_t)len * 8u;

    if (idx + len > 63) {
        memcpy(ctx->buffer + idx, data, 64 - idx);
        ws_sha1_transform(ctx, ctx->buffer);
        for (i = 64 - idx; i + 63 < len; i += 64) {
            ws_sha1_transform(ctx, data + i);
        }
        idx = 0;
    } else {
        i = 0;
    }

    memcpy(ctx->buffer + idx, data + i, len - i);
}

static void ws_sha1_final(ws_sha1_ctx_t *ctx, uint8_t out[20])
{
    uint8_t final[64];
    size_t idx = (size_t)((ctx->count >> 3) & 63u);
    unsigned i;

    memcpy(final, ctx->buffer, idx);
    final[idx++] = 0x80;
    if (idx > 56) {
        memset(final + idx, 0, 64 - idx);
        ws_sha1_transform(ctx, final);
        idx = 0;
    }
    memset(final + idx, 0, 56 - idx);

    for (i = 0; i < 8; i++) {
        final[56 + i] = (uint8_t)(ctx->count >> (56 - i * 8));
    }
    ws_sha1_transform(ctx, final);

    for (i = 0; i < 5; i++) {
        out[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

void hybbx_ws_sha1(const uint8_t *data, size_t len, uint8_t out[20])
{
    ws_sha1_ctx_t ctx;

    ws_sha1_init(&ctx);
    ws_sha1_update(&ctx, data, len);
    ws_sha1_final(&ctx, out);
}
