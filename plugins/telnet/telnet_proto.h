#ifndef HYBBX_TELNET_PROTO_H
#define HYBBX_TELNET_PROTO_H

#include "hybbx/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hybbx_telnet_parser {
    int state;
    int command;
} hybbx_telnet_parser_t;

typedef void (*hybbx_telnet_data_cb)(void *ctx, const uint8_t *data, size_t len);

void hybbx_telnet_parser_init(hybbx_telnet_parser_t *parser);

/** Send initial option negotiation (echo, suppress go ahead). */
hybbx_result_t hybbx_telnet_send_greeting(int fd);

/**
 * Strip telnet protocol bytes and invoke @p on_data for user payload.
 */
hybbx_result_t hybbx_telnet_parser_feed(hybbx_telnet_parser_t *parser, int fd,
                                        const uint8_t *data, size_t len,
                                        hybbx_telnet_data_cb on_data,
                                        void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_TELNET_PROTO_H */
