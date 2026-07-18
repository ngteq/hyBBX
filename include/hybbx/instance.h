#ifndef HYBBX_INSTANCE_H
#define HYBBX_INSTANCE_H

#include "hybbx/networks.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Hardcoded instance role (one binary per role).
 *
 *   hybbxd  — Main: local user BBX; WebSocket→reverse-proxy only;
 *             mains_proxy to Secondary peers; HBX hub for Proxy edges
 *   hybbxsd — Secondary: peer other Mains via mains_proxy only;
 *             optional telnet/ssh (no WebSocket, no RF); no user BBX
 *   hybbxpd — Proxy: hybrid TCP bridge (telnet/ssh + circuit);
 *             no WebSocket, no mains_proxy mesh; RF via MAX25-Stack only
 */
typedef enum hybbx_instance_role {
    HYBBX_INSTANCE_MAIN = 1,
    HYBBX_INSTANCE_SECONDARY = 2,
    HYBBX_INSTANCE_PROXY = 3
} hybbx_instance_role_t;

/** Compile-time role for this binary (overridden by instance_role_*.c). */
hybbx_instance_role_t hybbx_instance_role(void);

const char *hybbx_instance_role_name(void);
const char *hybbx_instance_binary_name(void);

/** Non-zero when this binary hosts interactive user BBX sessions. */
int hybbx_instance_offers_user_bbx(void);

/** Non-zero when @p plugin_name may register/start on this instance. */
int hybbx_instance_plugin_allowed(const char *plugin_name);

/**
 * Force layout law onto @p networks after INI apply.
 * Logs corrections; returns HYBBX_OK or HYBBX_ERR_INVALID if unrecoverable.
 */
hybbx_result_t hybbx_networks_enforce_instance(hybbx_networks_config_t *networks);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_INSTANCE_H */
