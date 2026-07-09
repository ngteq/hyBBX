#ifndef HYBBX_PROXYCHAT_H
#define HYBBX_PROXYCHAT_H

/**
 * Inter-Main chat via mains_proxy — stub only; no distribution yet.
 *
 * Local `/chat` stays on this Main. `/proxychat` is the mains-proxy sub-area
 * under `/chat` (or `/chat proxychat`).
 */

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_session;

/** Show proxychat area banner (stub). */
void hybbx_proxychat_show_banner(struct hybbx_session *session);

/**
 * Post a line in proxychat (stub — not relayed).
 * @return HYBBX_OK (message is not sent over the mesh).
 */
hybbx_result_t hybbx_proxychat_post_stub(struct hybbx_session *session,
                                         const char *line);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_PROXYCHAT_H */
