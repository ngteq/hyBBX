#ifndef HYBBX_WS_PROTO_H
#define HYBBX_WS_PROTO_H

#include "hybbx/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_WS_HANDSHAKE_MAX 4096u
#define HYBBX_WS_FRAME_PAYLOAD_MAX 4096u

typedef void (*hybbx_ws_data_cb)(void *ctx, const uint8_t *data, size_t len);

typedef struct hybbx_ws_connection {
    int fd;
    void *tls;
    int established;
    uint8_t rx_buf[HYBBX_WS_FRAME_PAYLOAD_MAX + 16];
    size_t rx_len;
} hybbx_ws_connection_t;

void hybbx_ws_connection_init(hybbx_ws_connection_t *ws, int fd);

/** Attach TLS session (opaque SSL*) after accept; plain ws when NULL. */
void hybbx_ws_connection_set_tls(hybbx_ws_connection_t *ws, void *tls);

/** Release TLS resources; does not close @c fd. */
void hybbx_ws_connection_cleanup(hybbx_ws_connection_t *ws);

/**
 * Perform server-side HTTP Upgrade handshake.
 * @p path must match the request URI path (e.g. /hybbx) when non-empty.
 */
hybbx_result_t hybbx_ws_server_handshake(hybbx_ws_connection_t *ws,
                                         const char *path);

/**
 * Read WebSocket frames; invoke @p on_data for each complete text/binary
 * payload from the client. Returns HYBBX_ERR_IO on disconnect.
 */
hybbx_result_t hybbx_ws_read_frames(hybbx_ws_connection_t *ws,
                                    hybbx_ws_data_cb on_data,
                                    void *ctx);

/** Send one server text frame (unmasked). */
hybbx_result_t hybbx_ws_write_text(hybbx_ws_connection_t *ws,
                                   const char *data, size_t len);

/** Send one server ping frame (unmasked, empty payload). */
hybbx_result_t hybbx_ws_ping(hybbx_ws_connection_t *ws);

/** Send WebSocket close frame. */
hybbx_result_t hybbx_ws_close(hybbx_ws_connection_t *ws);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_WS_PROTO_H */
