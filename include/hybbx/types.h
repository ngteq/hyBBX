#ifndef HYBBX_TYPES_H
#define HYBBX_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum hybbx_result {
    HYBBX_OK = 0,
    /** Input is a local/mailbox command (not a HyBBX '/' command). */
    HYBBX_LOCAL_CMD = 1,
    /** Session should be closed (e.g. /exit). */
    HYBBX_SESSION_END = 2,
    HYBBX_ERR_INVALID = -1,
    HYBBX_ERR_NOMEM = -2,
    HYBBX_ERR_NOT_FOUND = -3,
    HYBBX_ERR_IO = -4,
    HYBBX_ERR_UNSUPPORTED = -5,
    HYBBX_ERR_BUSY = -6,
    HYBBX_ERR_DENIED = -7
} hybbx_result_t;

typedef enum hybbx_transport_kind {
    HYBBX_TRANSPORT_TELNET = 1,
    HYBBX_TRANSPORT_PACKET_RADIO = 2,
    /* HYBBX_TRANSPORT_SSH = 3,  -- planned */
} hybbx_transport_kind_t;

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_TYPES_H */
