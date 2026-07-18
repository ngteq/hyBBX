#ifndef HYBBX_NETWORKS_H
#define HYBBX_NETWORKS_H

#include "hybbx/config.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Connection / link adapter switches from INI `[networks]`.
 *
 * Static transport depends on instance role (see hybbx/instance.h):
 *   Main      — websocket (always on when built)
 *   Secondary — mains_proxy
 *   Proxy     — none (optional TCP transports)
 */
typedef struct hybbx_networks_config {
    /** Telnet (`transport.telnet`) — not used on Main (WebSocket only). */
    int telnet;
    /** AX.25 / packet radio — always forced off (MAX25-Stack owns RF). */
    int ax25;
    /** BayCom — always forced off (MAX25-Stack owns RF). */
    int baycom;
    /** ARDOP — always forced off (MAX25-Stack owns RF / soft-modem). */
    int ardop;
    /** CRDOP — always forced off (MAX25-Stack owns RF / soft-modem). */
    int crdop;
    /** SSH transport (`transport.ssh`). */
    int ssh;
    /** WebSocket forward-proxy (`transport.websocket`) — Main static path. */
    int websocket;
    /** Internal HBX circuit hub for edge link/repeater daemons. */
    int circuit;
    /** Main-to-Main mesh proxy (`transport.mains_proxy`). */
    int mains_proxy;
} hybbx_networks_config_t;

void hybbx_networks_config_defaults(hybbx_networks_config_t *networks);

/** Load `[networks]` from an INI file. */
void hybbx_networks_config_apply(hybbx_networks_config_t *networks,
                                 const hybbx_config_t *config);

/**
 * Non-zero when @p plugin_name is the role's static core transport.
 * Ignores `[transport.*] enabled` and is started when built.
 */
int hybbx_networks_is_static_transport(const char *plugin_name);

/**
 * Non-zero when transport @p plugin_name should be started for this config.
 * Respects instance plugin allow-list and `[networks]` toggles.
 */
int hybbx_networks_transport_wanted(const char *plugin_name,
                                    const hybbx_networks_config_t *networks);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_NETWORKS_H */
