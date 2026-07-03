#ifndef HYBBX_BROADCAST_H
#define HYBBX_BROADCAST_H

/**
 * HyBBX broadcasting — Main instance only.
 *
 * AX.25: QST UI over HBX to Secondary extenders on links that are
 * low-bandwidth AND half-duplex (HyBBX QoS). Frequencies are MHz only —
 * no CB channel numbers (those differ by region).
 * TCP/IP: stub (logged; wire path reserved for after v1.0.0).
 */

#include "hybbx/ax25.h"
#include "hybbx/config.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_AX25_FREQUENCY_MAX      40u
#define HYBBX_BROADCAST_MESSAGE_MAX   240u

typedef struct hybbx_ax25_frequency_table {
    unsigned count;
    double mhz[HYBBX_AX25_FREQUENCY_MAX];
    char labels[HYBBX_AX25_FREQUENCY_MAX][32];
} hybbx_ax25_frequency_table_t;

typedef struct hybbx_broadcast_config {
    int enabled;
    int tcp_enabled;
    int ax25_enabled;
    char ax25_mycall[HYBBX_AX25_CALL_MAX + 1];
    char ax25_dest[HYBBX_AX25_CALL_MAX + 1];
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
 * AX.25 broadcast from Main over HBX (dest=QST UI).
 * Link must be low-bandwidth and half-duplex (QoS).
 * @p frequency_mhz 0.0 = any frequency on the connected qualifying link;
 *                    else must match the link's reported frequency_mhz.
 */
hybbx_result_t hybbx_broadcast_ax25(struct hybbx_service *service,
                                    double frequency_mhz,
                                    const char *message);

/** TCP/IP broadcast stub — logs only until telnet fan-out is implemented. */
hybbx_result_t hybbx_broadcast_tcp_stub(struct hybbx_service *service,
                                          const char *message);

void hybbx_broadcast_list_ax25_frequencies(struct hybbx_session *session,
                                           const hybbx_broadcast_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_BROADCAST_H */
