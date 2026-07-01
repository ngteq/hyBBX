#ifndef HYBBX_AX25_H
#define HYBBX_AX25_H

#include "hybbx/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_AX25_CALL_MAX     6
#define HYBBX_AX25_SSID_MAX     15
#define HYBBX_AX25_ADDR_ENCODED 7
#define HYBBX_AX25_MAX_DIGI     8
#define HYBBX_AX25_FRAME_MAX    330
#define HYBBX_AX25_PID_NOL3     0xF0u
#define HYBBX_AX25_CTRL_UI      0x03u

typedef struct hybbx_ax25_address {
    char call[HYBBX_AX25_CALL_MAX + 1];
    unsigned ssid;
} hybbx_ax25_address_t;

typedef struct hybbx_ax25_path {
    hybbx_ax25_address_t dest;
    hybbx_ax25_address_t source;
    hybbx_ax25_address_t digi[HYBBX_AX25_MAX_DIGI];
    unsigned digi_count;
} hybbx_ax25_path_t;

/**
 * Parse "CALL" or "CALL-SSID" into @p out.
 */
hybbx_result_t hybbx_ax25_address_parse(const char *text,
                                        hybbx_ax25_address_t *out);

/**
 * Encode one AX.25 address field (7 bytes).
 * @p last non-zero on the final address field before control byte.
 */
void hybbx_ax25_encode_address(uint8_t *out, const hybbx_ax25_address_t *addr,
                               int last);

/** CRC-CCITT (0x1021), init 0xFFFF — AX.25 FCS. */
uint16_t hybbx_ax25_crc(const uint8_t *data, size_t len);

/**
 * Build an AX.25 UI frame (PID 0xF0) into @p out.
 * @return frame length or 0 on error.
 */
size_t hybbx_ax25_build_ui(const hybbx_ax25_path_t *path,
                           const uint8_t *payload, size_t payload_len,
                           uint8_t *out, size_t out_cap);

/**
 * Parse a UI frame. Returns payload length or 0 if not a UI frame / invalid FCS.
 */
size_t hybbx_ax25_parse_ui(const uint8_t *frame, size_t frame_len,
                           hybbx_ax25_path_t *path,
                           uint8_t *payload, size_t payload_cap);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_AX25_H */
