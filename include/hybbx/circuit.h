#ifndef HYBBX_CIRCUIT_H
#define HYBBX_CIRCUIT_H

/**
 * HBX — Hybrid Bridge eXchange (v1).
 *
 * HyBBX internal framed protocol on the circuit TCP hub. Multiplexes
 * link-layer payloads (AX.25 and future stacks) and application streams
 * (terminal, proxymail, proxychat) between HyBBX processes. Header magic:
 * `H` `B` `X`. Transport is internal TCP/IPv4+IPv6 only — the application
 * core never sees KISS, AX.25 on-air framing, or serial.
 *
 * Short form in docs: **HBX** or **HBX/Circuit** — Circuit = TCP hub
 * (:7323), HBX = framing on top.
 */

#include "hybbx/ax25.h"
#include "hybbx/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_CIRCUIT_MAGIC_0 'H'
#define HYBBX_CIRCUIT_MAGIC_1 'B'
#define HYBBX_CIRCUIT_MAGIC_2 'X'
#define HYBBX_CIRCUIT_VERSION  0x01u

#define HYBBX_CIRCUIT_HEADER_SIZE 12u
#define HYBBX_CIRCUIT_MAX_PAYLOAD 4096u
#define HYBBX_CIRCUIT_MAX_FRAME (HYBBX_CIRCUIT_HEADER_SIZE + HYBBX_CIRCUIT_MAX_PAYLOAD)

/** Default internal circuit TCP port (loopback). */
#define HYBBX_CIRCUIT_DEFAULT_PORT 7323u

/**
 * Protocol identifiers — extensible for future link stacks.
 * Values 0x01..0x7F: link-layer payloads; 0x80..0xFF: application streams.
 */
typedef enum hybbx_circuit_proto {
    HYBBX_CIRCUIT_PROTO_AX25 = 0x01,       /**< Raw AX.25 frame incl. FCS */
    HYBBX_CIRCUIT_PROTO_AX25_UI = 0x02,      /**< Masked UI: path + payload */
    HYBBX_CIRCUIT_PROTO_LINK_AUTH = 0x03,    /**< Edge link password auth (no ping) */
    HYBBX_CIRCUIT_PROTO_LINK_AUTH_ACK = 0x04,/**< Central auth response */
    HYBBX_CIRCUIT_PROTO_FLOW_CTRL = 0x05,    /**< Load-balance pause/break/cancel */
    HYBBX_CIRCUIT_PROTO_TERMINAL = 0x10,     /**< BBS terminal byte stream */
    HYBBX_CIRCUIT_PROTO_PROXY_MAIL = 0x80,   /**< Inter-Main proxymail (service only) */
    HYBBX_CIRCUIT_PROTO_PROXY_CHAT = 0x81,   /**< Inter-Main proxychat (service only) */
    HYBBX_CIRCUIT_PROTO_RESERVED_APRS = 0x20,
    HYBBX_CIRCUIT_PROTO_RESERVED_NETROM = 0x21
} hybbx_circuit_proto_t;

typedef enum hybbx_circuit_flags {
    HYBBX_CIRCUIT_FLAG_NONE = 0x0000,
    HYBBX_CIRCUIT_FLAG_RX = 0x0001,        /**< Frame originated on RF RX */
    HYBBX_CIRCUIT_FLAG_TX = 0x0002,        /**< Frame destined for RF TX */
    HYBBX_CIRCUIT_FLAG_PATH = 0x0004,       /**< AX.25_UI payload carries path */
    HYBBX_CIRCUIT_FLAG_G3RUH_FSK = 0x0008  /**< G3RUH 9600 FSK on-air (vs AFSK) */
} hybbx_circuit_flags_t;

typedef void (*hybbx_circuit_frame_cb)(hybbx_circuit_proto_t proto,
                                       uint16_t flags,
                                       const uint8_t *payload, size_t len,
                                       void *userdata);

typedef struct hybbx_circuit_decoder {
    uint8_t buf[HYBBX_CIRCUIT_MAX_FRAME];
    size_t len;
    size_t need;
    int have_header;
    hybbx_circuit_proto_t proto;
    uint16_t flags;
} hybbx_circuit_decoder_t;

void hybbx_circuit_decoder_init(hybbx_circuit_decoder_t *dec);

void hybbx_circuit_decoder_feed(hybbx_circuit_decoder_t *dec,
                                const uint8_t *data, size_t len,
                                hybbx_circuit_frame_cb cb, void *userdata);

size_t hybbx_circuit_encode(hybbx_circuit_proto_t proto, uint16_t flags,
                            const uint8_t *payload, size_t payload_len,
                            uint8_t *out, size_t out_cap);

size_t hybbx_circuit_encode_ax25(const uint8_t *frame, size_t frame_len,
                                 uint16_t flags,
                                 uint8_t *out, size_t out_cap);

size_t hybbx_circuit_encode_ax25_ui(const hybbx_ax25_path_t *path,
                                    const uint8_t *payload, size_t payload_len,
                                    uint16_t flags,
                                    uint8_t *out, size_t out_cap);

size_t hybbx_circuit_encode_terminal(const char *data, size_t len,
                                     uint8_t *out, size_t out_cap);

/** Encode LINK_AUTH or LINK_AUTH_ACK line-oriented payload. */
size_t hybbx_circuit_encode_link_msg(hybbx_circuit_proto_t proto,
                                   const char *payload, size_t payload_len,
                                   uint8_t *out, size_t out_cap);

/**
 * Unpack AX.25_UI masked payload into path + UI bytes.
 * @return UI payload length or 0 on error.
 */
size_t hybbx_circuit_unpack_ax25_ui(const uint8_t *payload, size_t payload_len,
                                    hybbx_ax25_path_t *path,
                                    uint8_t *ui, size_t ui_cap);

const char *hybbx_circuit_proto_name(hybbx_circuit_proto_t proto);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_CIRCUIT_H */
