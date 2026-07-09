#ifndef HYBBX_CIRCUIT_TCP_H
#define HYBBX_CIRCUIT_TCP_H

/**
 * HBX circuit TCP hub on Main; remote Secondary processes connect via LINK_AUTH.
 * Secondaries are separate edge machines (extenders/repeaters), not telnet users
 * or local [transport.*] adapters on Main.
 * Hub: [circuit] bind/port. Client: hybbx_circuit_link_* + LINK_AUTH.
 * All inter-node paths (Secondary, mains_proxy mesh, future relays) use this
 * client pattern only — never a direct socket to a remote HyBBX Main.
 */

#include "hybbx/circuit.h"
#include "hybbx/circuit_balance.h"
#include "hybbx/circuit_bridge.h"
#include "hybbx/service.h"
#include "hybbx/types.h"
#include "hybbx/limits.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hybbx_circuit_hub hybbx_circuit_hub_t;

typedef struct hybbx_circuit_config {
    char bind4[64];
    char bind6[64];
    unsigned port;
    int ipv4;
    int ipv6;
    char link_password[128];
    unsigned link_stale_days;
    int link_auth;
    char data_path[HYBBX_PATH_MAX];
    char config_path[HYBBX_PATH_MAX];
    unsigned max_links;
    hybbx_circuit_bridge_registry_t bridge;
    hybbx_circuit_balance_config_t balance;
} hybbx_circuit_config_t;

/** Internal transport plugin used for sessions bridged over the circuit hub. */
extern const hybbx_transport_plugin_t hybbx_plugin_circuit;

void hybbx_circuit_config_defaults(hybbx_circuit_config_t *cfg);

hybbx_circuit_hub_t *hybbx_circuit_hub_create(hybbx_service_t *service);
void hybbx_circuit_hub_destroy(hybbx_circuit_hub_t *hub);

hybbx_result_t hybbx_circuit_hub_start(hybbx_circuit_hub_t *hub,
                                       const hybbx_circuit_config_t *cfg);

void hybbx_circuit_hub_stop(hybbx_circuit_hub_t *hub);

int hybbx_circuit_hub_running(const hybbx_circuit_hub_t *hub);

unsigned hybbx_circuit_hub_port(const hybbx_circuit_hub_t *hub);

/**
 * Send a pre-encoded HBX frame to the attached link adapter.
 */
hybbx_result_t hybbx_circuit_hub_send_raw(hybbx_circuit_hub_t *hub,
                                          const uint8_t *frame, size_t len);

hybbx_result_t hybbx_circuit_hub_send_hbx(hybbx_circuit_hub_t *hub,
                                          const uint8_t *frame, size_t len);

unsigned hybbx_circuit_hub_active_link_count(const hybbx_circuit_hub_t *hub);

/**
 * Send HBX to all active links. @p frequency_mhz 0 = any MHz.
 * @p require_broadcast_qos filters to low-bandwidth + half-duplex links.
 * Returns OK when at least one link accepted the frame.
 */
hybbx_result_t hybbx_circuit_hub_multicast_hbx(hybbx_circuit_hub_t *hub,
                                               const uint8_t *frame, size_t len,
                                               double frequency_mhz,
                                               int require_broadcast_qos);

double hybbx_circuit_hub_link_frequency_mhz(const hybbx_circuit_hub_t *hub);

/** Non-zero when link QoS is low-bandwidth and half-duplex (AX.25 broadcast allowed). */
int hybbx_circuit_hub_link_broadcast_qos(const hybbx_circuit_hub_t *hub);

void hybbx_circuit_hub_prune_links(hybbx_circuit_hub_t *hub);

/** Link adapter: connect to the internal circuit hub (TCP client). */
hybbx_result_t hybbx_circuit_link_connect(const char *host, unsigned port,
                                        int *out_fd);

hybbx_result_t hybbx_circuit_link_write(int fd, const uint8_t *frame,
                                        size_t len);

hybbx_result_t hybbx_circuit_link_read(int fd, uint8_t *buf, size_t buf_len,
                                       size_t *read_len);

/**
 * Send LINK_AUTH HBX frame after TCP connect (link/repeater edge daemon
 * toward the centralized daemon). Password only — no heartbeat/ping.
 */
hybbx_result_t hybbx_circuit_link_authenticate(int fd,
                                               const char *password,
                                               const char *role,
                                               const char *id);

/**
 * LINK_AUTH with optional QoS (baud, duplex, bandwidth) for auto load-balancing.
 */
hybbx_result_t hybbx_circuit_link_authenticate_ex(int fd,
                                                  const char *password,
                                                  const char *role,
                                                  const char *id,
                                                  const hybbx_circuit_link_qos_t *qos);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_CIRCUIT_TCP_H */
