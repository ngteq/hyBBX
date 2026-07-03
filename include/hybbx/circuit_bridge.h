#ifndef HYBBX_CIRCUIT_BRIDGE_H
#define HYBBX_CIRCUIT_BRIDGE_H

/**
 * Main-side bridge registry for remote Secondaries — one entry per
 * [transport.packet_radioN] or [transport.ardopN] section (link metadata; RF runs on the Secondary host).
 * Secondaries (edge extenders/repeaters, not telnet users or local Main transports)
 * authenticate with matching link_id + link_password over the HBX/TCP circuit hub.
 */

#include "hybbx/config.h"
#include "hybbx/link.h"
#include "hybbx/limits.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hybbx_circuit_bridge_entry {
    char link_id[HYBBX_LINK_ID_MAX];
    char link_password[128];
    char link_role[HYBBX_LINK_ROLE_MAX];
    double frequency_mhz;
} hybbx_circuit_bridge_entry_t;

typedef struct hybbx_circuit_bridge_registry {
    hybbx_circuit_bridge_entry_t entries[HYBBX_CIRCUIT_MAX_LINKS];
    unsigned count;
} hybbx_circuit_bridge_registry_t;

void hybbx_circuit_bridge_clear(hybbx_circuit_bridge_registry_t *reg);

/** Load all transport.packet_radioN and transport.ardopN sections from Main INI. */
hybbx_result_t hybbx_circuit_bridge_load(hybbx_circuit_bridge_registry_t *reg,
                                         const hybbx_config_t *config);

const hybbx_circuit_bridge_entry_t *hybbx_circuit_bridge_find(
    const hybbx_circuit_bridge_registry_t *reg, const char *link_id);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_CIRCUIT_BRIDGE_H */
