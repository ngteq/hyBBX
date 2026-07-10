#ifndef HYBBX_MAINS_PROXY_H
#define HYBBX_MAINS_PROXY_H

/**
 * Main-to-Main mesh proxy — link HyBBX instances for user services only.
 *
 * SECURITY: inter-node mesh links MUST attach via HBX/Circuit only
 * (hybbx_circuit_link_connect + LINK_AUTH). Never open a raw TCP socket
 * to a remote Main or peer process — same rule as Secondary edge links.
 *
 * Only proxymail and proxychat service payloads cross the mesh. No user
 * accounts, rights, or other Main data are transferred.
 */

#include "hybbx/circuit.h"
#include "hybbx/config.h"
#include "hybbx/link.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_MAINS_PROXY_PEER_ID_MAX   64
#define HYBBX_MAINS_PROXY_HOST_MAX      256
#define HYBBX_MAINS_PROXY_MAX_PEERS     8u

typedef enum hybbx_mains_proxy_wire {
    /** IP path carried on HBX circuit frames (not a direct Main socket). */
    HYBBX_MAINS_PROXY_WIRE_CIRCUIT = 0,
    HYBBX_MAINS_PROXY_WIRE_AX25,
} hybbx_mains_proxy_wire_t;

typedef enum hybbx_mains_proxy_duplex {
    HYBBX_MAINS_PROXY_DUPLEX_FULL = 0,
    HYBBX_MAINS_PROXY_DUPLEX_HALF,
} hybbx_mains_proxy_duplex_t;

typedef struct hybbx_mains_proxy_peer_config {
    char peer_id[HYBBX_MAINS_PROXY_PEER_ID_MAX];
    char circuit_host[HYBBX_MAINS_PROXY_HOST_MAX];
    unsigned circuit_port;
    char link_id[HYBBX_LINK_ID_MAX];
    char link_password[128];
    /** @deprecated Ignored for mesh connect — use circuit_host. */
    char host[HYBBX_MAINS_PROXY_HOST_MAX];
    /** @deprecated Ignored for mesh connect — use circuit_port. */
    unsigned port;
    hybbx_mains_proxy_wire_t wire;
    hybbx_mains_proxy_duplex_t duplex;
    int use_secondary;
    int enabled;
} hybbx_mains_proxy_peer_config_t;

typedef struct hybbx_mains_proxy_mesh {
    unsigned peer_count;
    hybbx_mains_proxy_peer_config_t peers[HYBBX_MAINS_PROXY_MAX_PEERS];
    int running;
} hybbx_mains_proxy_mesh_t;

struct hybbx_service;

hybbx_mains_proxy_wire_t hybbx_mains_proxy_wire_parse(const char *value);
const char *hybbx_mains_proxy_wire_name(hybbx_mains_proxy_wire_t wire);

hybbx_mains_proxy_duplex_t hybbx_mains_proxy_duplex_parse(const char *value);
const char *hybbx_mains_proxy_duplex_name(hybbx_mains_proxy_duplex_t duplex);

void hybbx_mains_proxy_peer_defaults(hybbx_mains_proxy_peer_config_t *peer);

/** Parse one `transport.mains_proxy` / `transport.mains_proxyN` section. */
hybbx_result_t hybbx_mains_proxy_peer_parse(const char *config,
                                            hybbx_mains_proxy_peer_config_t *out);

void hybbx_mains_proxy_mesh_init(hybbx_mains_proxy_mesh_t *mesh);

/**
 * Register peers and start mesh. Existing mesh state is replaced when called
 * again while running.
 */
hybbx_result_t hybbx_mains_proxy_mesh_start(struct hybbx_service *service,
                                            hybbx_mains_proxy_mesh_t *mesh);

void hybbx_mains_proxy_mesh_stop(hybbx_mains_proxy_mesh_t *mesh);

/** Periodic mesh I/O. Call from service main loop when running. */
void hybbx_mains_proxy_mesh_tick(struct hybbx_service *service,
                                 hybbx_mains_proxy_mesh_t *mesh);

/** Non-zero when the mesh relay is active with at least one live peer link. */
int hybbx_mains_proxy_mesh_active(void);

/**
 * Deliver proxymail to a configured peer (HBX PROXY_MAIL frame).
 * @p from_address and @p to_address use @c user@service form.
 */
hybbx_result_t hybbx_mains_proxy_send_mail(struct hybbx_service *service,
                                         const char *from_address,
                                         const char *to_address,
                                         const char *subject,
                                         const char *body);

/** Relay a proxychat line to all connected peers (HBX PROXY_CHAT frame). */
hybbx_result_t hybbx_mains_proxy_send_chat(struct hybbx_service *service,
                                           const char *from_address,
                                           const char *line);

/** Handle PROXY_MAIL / PROXY_CHAT received on the circuit hub (inbound link). */
void hybbx_mains_proxy_inbound_frame(struct hybbx_service *service,
                                     hybbx_circuit_proto_t proto,
                                     const uint8_t *payload, size_t len);

/** Service-loop tick (implemented by plugin when built). */
void hybbx_mains_proxy_plugin_tick(void);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_MAINS_PROXY_H */
