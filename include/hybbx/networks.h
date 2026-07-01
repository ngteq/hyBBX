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
 * Telnet and SSH are static-enabled core IP session transports (always on
 * when the plugin is built). AX.25 and other optional adapters are toggled
 * here; see @ref hybbx_networks_transport_wanted.
 */
typedef struct hybbx_networks_config {
    /** AX.25 / packet radio link adapter (`transport.packet_radio`). */
    int ax25;
    /** WebSocket transport (planned). */
    int websocket;
    /** Internal HBX circuit hub for edge link/repeater daemons. */
    int circuit;
} hybbx_networks_config_t;

void hybbx_networks_config_defaults(hybbx_networks_config_t *networks);

/** Load `[networks]` from an INI file. */
void hybbx_networks_config_apply(hybbx_networks_config_t *networks,
                                 const hybbx_config_t *config);

/**
 * Non-zero when @p plugin_name is a static core transport (telnet, ssh).
 * These ignore `[transport.*] enabled` and are started when built.
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
