#ifndef HYBBX_CIRCUIT_BALANCE_H
#define HYBBX_CIRCUIT_BALANCE_H

/**
 * HBX circuit auto load-balancing — pace and stabilize mixed link speeds,
 * duplex modes, and sync/async downlink traffic toward edge adapters.
 *
 * Escalation (low-bandwidth links): pause → break → cancel connection.
 * A few seconds of lag on slow links is normal; the balancer auto-stabilizes.
 */

#include "hybbx/circuit.h"
#include "hybbx/link.h"
#include "hybbx/types.h"

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_CIRCUIT_BALANCE_QUEUE_MAX   64u
#define HYBBX_CIRCUIT_BALANCE_DEFAULT_BAUD 115200u
#define HYBBX_CIRCUIT_BALANCE_LOW_BAUD_THRESHOLD 9600u

typedef enum hybbx_circuit_bandwidth {
    HYBBX_CIRCUIT_BW_HIGH = 0,
    HYBBX_CIRCUIT_BW_LOW = 1
} hybbx_circuit_bandwidth_t;

typedef enum hybbx_circuit_duplex {
    HYBBX_CIRCUIT_DUPLEX_UNSET = 0,
    HYBBX_CIRCUIT_DUPLEX_HALF = 1,
    HYBBX_CIRCUIT_DUPLEX_FULL = 2
} hybbx_circuit_duplex_t;

typedef enum hybbx_circuit_balance_action {
    HYBBX_CIRCUIT_BAL_NONE = 0,
    HYBBX_CIRCUIT_BAL_PAUSE = 1,
    HYBBX_CIRCUIT_BAL_BREAK = 2,
    HYBBX_CIRCUIT_BAL_CANCEL = 3,
    HYBBX_CIRCUIT_BAL_RESUME = 4
} hybbx_circuit_balance_action_t;

typedef enum hybbx_circuit_balance_tick_result {
    HYBBX_CIRCUIT_BAL_TICK_OK = 0,
    HYBBX_CIRCUIT_BAL_TICK_CANCEL_LINK = 1
} hybbx_circuit_balance_tick_result_t;

typedef struct hybbx_circuit_balance_config {
    int enabled;
    unsigned lag_sec;
    size_t queue_pause;
    size_t queue_break;
    size_t queue_cancel;
} hybbx_circuit_balance_config_t;

typedef struct hybbx_circuit_link_profile {
    hybbx_circuit_bandwidth_t bandwidth;
    hybbx_circuit_duplex_t duplex;
    unsigned baud;
    double frequency_mhz;
} hybbx_circuit_link_profile_t;

typedef struct hybbx_circuit_link_qos {
    unsigned baud;
    int duplex;
    const char *bandwidth;
    const char *frequency_mhz;
} hybbx_circuit_link_qos_t;

typedef hybbx_result_t (*hybbx_circuit_balance_send_fn)(void *ctx,
                                                      const uint8_t *frame,
                                                      size_t len);

typedef void (*hybbx_circuit_balance_flow_fn)(void *ctx,
                                              hybbx_circuit_balance_action_t action,
                                              const char *reason);

typedef struct hybbx_circuit_balance hybbx_circuit_balance_t;

void hybbx_circuit_balance_config_defaults(hybbx_circuit_balance_config_t *cfg);

void hybbx_circuit_link_profile_from_auth(const hybbx_link_auth_t *auth,
                                          hybbx_circuit_link_profile_t *out);

const char *hybbx_circuit_balance_action_name(hybbx_circuit_balance_action_t action);

hybbx_result_t hybbx_circuit_flow_ctrl_parse(const char *payload, size_t len,
                                             hybbx_circuit_balance_action_t *action_out,
                                             char *reason_out, size_t reason_cap);

size_t hybbx_circuit_flow_ctrl_format(hybbx_circuit_balance_action_t action,
                                      const char *reason,
                                      char *out, size_t out_cap);

hybbx_circuit_balance_t *hybbx_circuit_balance_create(
    const hybbx_circuit_balance_config_t *cfg);

void hybbx_circuit_balance_destroy(hybbx_circuit_balance_t *bal);

void hybbx_circuit_balance_set_profile(hybbx_circuit_balance_t *bal,
                                       const hybbx_circuit_link_profile_t *profile);

hybbx_circuit_balance_action_t hybbx_circuit_balance_action(
    const hybbx_circuit_balance_t *bal);

size_t hybbx_circuit_balance_queued_bytes(const hybbx_circuit_balance_t *bal);

/** De-escalate from cancel after user sacrifice — keep secondary link up. */
void hybbx_circuit_balance_spared_cancel(hybbx_circuit_balance_t *bal);

hybbx_result_t hybbx_circuit_balance_submit(hybbx_circuit_balance_t *bal,
                                            const uint8_t *frame, size_t len,
                                            hybbx_circuit_balance_send_fn send_fn,
                                            void *send_ctx,
                                            hybbx_circuit_balance_flow_fn flow_fn,
                                            void *flow_ctx);

hybbx_circuit_balance_tick_result_t hybbx_circuit_balance_tick(
    hybbx_circuit_balance_t *bal, unsigned poll_ms,
    hybbx_circuit_balance_send_fn send_fn, void *send_ctx,
    hybbx_circuit_balance_flow_fn flow_fn, void *flow_ctx);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_CIRCUIT_BALANCE_H */
