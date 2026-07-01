#ifndef HYBBX_SIXPACK_H
#define HYBBX_SIXPACK_H

#include "hybbx/kiss.h"
#include "hybbx/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_SIXPACK_LEAD 0x53u /* 'S' */

typedef void (*hybbx_sixpack_frame_cb)(const uint8_t *frame, size_t len,
                                       void *userdata);

typedef struct hybbx_sixpack_decoder {
    uint8_t buf[HYBBX_KISS_MAX_FRAME];
    size_t len;
    int in_frame;
    uint8_t count;
    uint8_t checksum;
} hybbx_sixpack_decoder_t;

void hybbx_sixpack_decoder_init(hybbx_sixpack_decoder_t *dec);

void hybbx_sixpack_decoder_feed(hybbx_sixpack_decoder_t *dec,
                              const uint8_t *data, size_t len,
                              hybbx_sixpack_frame_cb cb, void *userdata);

size_t hybbx_sixpack_encode(const uint8_t *frame, size_t frame_len,
                            uint8_t *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_SIXPACK_H */
