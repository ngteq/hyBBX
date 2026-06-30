#ifndef HYBBX_KISS_H
#define HYBBX_KISS_H

#include "hybbx/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_KISS_FEND  0xC0u
#define HYBBX_KISS_FESC  0xDBu
#define HYBBX_KISS_TFEND 0xDCu
#define HYBBX_KISS_TFESC 0xDDu

typedef enum hybbx_kiss_command {
    HYBBX_KISS_CMD_DATA = 0x00,
    HYBBX_KISS_CMD_TXDELAY = 0x01,
    HYBBX_KISS_CMD_PERSIST = 0x02,
    HYBBX_KISS_CMD_SLOTTIME = 0x03,
    HYBBX_KISS_CMD_TXTAIL = 0x04,
    HYBBX_KISS_CMD_FULLDUPLEX = 0x05,
    HYBBX_KISS_CMD_SETHARDWARE = 0x06
} hybbx_kiss_command_t;

#define HYBBX_KISS_MAX_FRAME 2048

typedef void (*hybbx_kiss_frame_cb)(uint8_t port, const uint8_t *frame,
                                    size_t len, void *userdata);

typedef struct hybbx_kiss_decoder {
    uint8_t buf[HYBBX_KISS_MAX_FRAME];
    size_t len;
    int in_frame;
    int escape;
} hybbx_kiss_decoder_t;

void hybbx_kiss_decoder_init(hybbx_kiss_decoder_t *dec);

/**
 * Feed raw serial bytes; complete KISS frames invoke @p cb.
 */
void hybbx_kiss_decoder_feed(hybbx_kiss_decoder_t *dec,
                             const uint8_t *data, size_t len,
                             hybbx_kiss_frame_cb cb, void *userdata);

/**
 * Encode a KISS frame into @p out (must hold at least @p out_cap bytes).
 * Returns encoded length or 0 on error.
 */
size_t hybbx_kiss_encode(uint8_t port, hybbx_kiss_command_t cmd,
                         const uint8_t *payload, size_t payload_len,
                         uint8_t *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_KISS_H */
