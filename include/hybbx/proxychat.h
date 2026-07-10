#ifndef HYBBX_PROXYCHAT_H
#define HYBBX_PROXYCHAT_H

/**
 * Inter-Main chat via mains_proxy.
 *
 * Local `/chat` stays on this Main. `/proxychat` is the mains-proxy sub-area
 * under `/chat` (or `/chat proxychat`). Only chat lines cross the mesh — no
 * user accounts or rights.
 */

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_service;
struct hybbx_session;

/** Show proxychat area banner. */
void hybbx_proxychat_show_banner(struct hybbx_session *session);

/**
 * Post a line in proxychat (relay when mesh is active).
 */
hybbx_result_t hybbx_proxychat_post(struct hybbx_service *service,
                                    struct hybbx_session *session,
                                    const char *line);

/** Deliver a line received from a remote Main. */
void hybbx_proxychat_receive(struct hybbx_service *service,
                             const char *from_address,
                             const char *line);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_PROXYCHAT_H */
