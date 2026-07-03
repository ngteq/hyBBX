#ifndef ARDOP_HOST_H
#define ARDOP_HOST_H

/**
 * Minimal ARDOP TNC Host Interface client (TCP control + data ports).
 * Implements the subset HyBBX needs — see docs/MANUAL.md § ARDOP.
 */

#include "hybbx/types.h"
#include "hybbx/ardop.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ardop_host ardop_host_t;

typedef void (*ardop_host_data_fn)(const uint8_t *data, size_t len, void *ctx);
typedef void (*ardop_host_event_fn)(const char *event, const char *detail,
                                    void *ctx);

ardop_host_t *ardop_host_create(const hybbx_ardop_config_t *cfg,
                                ardop_host_data_fn on_data,
                                ardop_host_event_fn on_event,
                                void *userdata,
                                const char *log_tag);
void ardop_host_destroy(ardop_host_t *host);

hybbx_result_t ardop_host_connect(ardop_host_t *host);
void ardop_host_disconnect(ardop_host_t *host);

/** Drive I/O and state machine; call periodically from plugin poll thread. */
void ardop_host_poll(ardop_host_t *host);

int ardop_host_link_up(const ardop_host_t *host);

/** Control TCP session to external ARDOPC is open. */
int ardop_host_modem_connected(const ardop_host_t *host);

hybbx_result_t ardop_host_send_data(ardop_host_t *host,
                                    const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* ARDOP_HOST_H */
