#ifndef HYBBX_BROADCAST_H
#define HYBBX_BROADCAST_H

/**
 * HyBBX broadcast — Main instance only.
 *
 * User command `/broadcast` (alias `/announce`): instant message to every
 * online session on this Main (local telnet/SSH/WebSocket users).
 *
 * INI `[broadcast]` ax25_auto: periodic AX.25 QST UI beacon over HBX to
 * Secondary extenders (low-bandwidth + half-duplex links). Separate from
 * `/broadcast`; not a user command.
 */

#include "hybbx/ax25.h"
#include "hybbx/config.h"
#include "hybbx/limits.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_AX25_FREQUENCY_MAX      40u
/** Local Main announce message cap. */
#define HYBBX_BROADCAST_MESSAGE_MAX   240u

#define HYBBX_BROADCAST_AUTO_MESSAGE_DEFAULT "Broadcast: @service@ online"

typedef struct hybbx_ax25_frequency_table {
    unsigned count;
    double mhz[HYBBX_AX25_FREQUENCY_MAX];
    char labels[HYBBX_AX25_FREQUENCY_MAX][32];
} hybbx_ax25_frequency_table_t;

typedef struct hybbx_broadcast_config {
    int enabled;
    int ax25_enabled;
    int ax25_auto;
    unsigned ax25_auto_interval_sec;
    char ax25_mycall[HYBBX_AX25_CALL_MAX + 1];
    char ax25_dest[HYBBX_AX25_CALL_MAX + 1];
    char ax25_auto_message[HYBBX_BROADCAST_AX25_MESSAGE_MAX + 1];
    hybbx_ax25_frequency_table_t frequencies;
} hybbx_broadcast_config_t;

struct hybbx_service;
struct hybbx_session;

void hybbx_ax25_frequency_table_clear(hybbx_ax25_frequency_table_t *table);

/** Load `[ax25]` frequency1 … frequencyN (MHz) and optional labels. */
void hybbx_ax25_frequency_apply(hybbx_ax25_frequency_table_t *table,
                                const hybbx_config_t *config);

/** Compare two MHz values (tolerance for INI rounding). */
int hybbx_ax25_frequency_match(double a_mhz, double b_mhz);

void hybbx_broadcast_config_defaults(hybbx_broadcast_config_t *cfg);

/** Load `[broadcast]` + `[ax25]` from INI. */
void hybbx_broadcast_config_apply(hybbx_broadcast_config_t *cfg,
                                  const hybbx_config_t *config);

const hybbx_broadcast_config_t *hybbx_service_get_broadcast(
    const struct hybbx_service *service);

/**
 * Send @p message to every online session on this Main (Sysop `/broadcast`).
 */
hybbx_result_t hybbx_broadcast_announce(struct hybbx_service *service,
                                        struct hybbx_session *from,
                                        const char *message);

/**
 * AX.25 auto-beacon over HBX (INI ax25_auto only).
 * Link must be low-bandwidth and half-duplex (QoS).
 */
hybbx_result_t hybbx_broadcast_ax25(struct hybbx_service *service,
                                    double frequency_mhz,
                                    const char *message);

/**
 * Periodic AX.25 auto-beacon (call once per second from Main service loop).
 */
void hybbx_broadcast_ax25_tick(struct hybbx_service *service);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_BROADCAST_H */
