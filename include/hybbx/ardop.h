#ifndef HYBBX_ARDOP_H
#define HYBBX_ARDOP_H

/**
 * ARDOP link adapter — HyBBX Host-Client subset (external ARDOP modem).
 *
 * HyBBX is never a sound-modem service. Operator runs ARDOPC or ardopcf as a
 * separate process; this plugin speaks the TNC Host Interface over TCP.
 * For CRDOP, use the `crdop` plugin and external CRDOPC instead.
 */

#include "hybbx/types.h"
#include "hybbx/crdop.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default ARDOPC TCP control port (data = port + 1). */
#define HYBBX_ARDOP_DEFAULT_PORT 8515u

/** Max payload per ARDOP host data frame (spec timing guidance). */
#define HYBBX_ARDOP_DATA_MAX 2000u

typedef struct hybbx_ardop_config {
    char *ardop_host;
    unsigned ardop_port;
    char *mycall;
    char *arq_bandwidth;
    char *peer_call;
    int listen;
    char *circuit_host;
    unsigned circuit_port;
    char *link_id;
    char *link_password;
    char *link_role;
    double frequency_mhz;
    /** Experimental CRDOP preview: cb | amateur (see docs/CRDOP.md). */
    hybbx_crdop_radio_profile_t radio_profile;
} hybbx_ardop_config_t;

hybbx_result_t hybbx_ardop_config_parse(const char *config,
                                        hybbx_ardop_config_t *out);
void hybbx_ardop_config_free(hybbx_ardop_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_ARDOP_H */
