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
 * Telnet is the static-enabled core IP session transport (always on when built).
 * SSH, AX.25, WebSocket, and other optional adapters are toggled in `[networks]`.
 */
typedef struct hybbx_networks_config {
    /** AX.25 / packet radio link adapter (`transport.packet_radio`). */
    int ax25;
    /** BayCom PR-Stack (`transport.baycom`) — kernel SER12/PAR96 or KISS serial. */
    int baycom;
    /** ARDOP host-client (`transport.ardop`) — external ARDOPC. */
    int ardop;
    /** CRDOP CB host-client (`transport.crdop`) — external CRDOPC (github.com/ngteq/CRDOP). */
    int crdop;
    /** SSH transport (`transport.ssh`) — libssh server on port 3232. */
    int ssh;
    /** WebSocket forward-proxy (`transport.websocket`). */
    int websocket;
    /** Internal HBX circuit hub for edge link/repeater daemons. */
    int circuit;
} hybbx_networks_config_t;

void hybbx_networks_config_defaults(hybbx_networks_config_t *networks);

/** Load `[networks]` from an INI file. */
void hybbx_networks_config_apply(hybbx_networks_config_t *networks,
                                 const hybbx_config_t *config);

/**
 * Non-zero when @p plugin_name is the static core transport (telnet).
 * Ignores `[transport.*] enabled` and is started when built.
 */
int hybbx_networks_is_static_transport(const char *plugin_name);

/**
 * Non-zero when transport @p plugin_name should be started for this config.
 * Static transports are always wanted; optional adapters follow `[networks]`.
 */
int hybbx_networks_transport_wanted(const char *plugin_name,
                                    const hybbx_networks_config_t *networks);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_NETWORKS_H */
