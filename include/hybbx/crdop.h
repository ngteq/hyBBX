#ifndef HYBBX_CRDOP_H
#define HYBBX_CRDOP_H

/**
 * CRDOP — CB Radio Digital Open Protocol (Level 2, experimental).
 *
 * Planned after HyBBX v1.0.0. CRDOP modem = external CRDOPC (not inside HyBBX).
 * Today: radio_profile hints on the ardop Host-Client plugin only.
 */

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum hybbx_crdop_radio_profile {
    HYBBX_CRDOP_PROFILE_AMATEUR = 0,
    HYBBX_CRDOP_PROFILE_CB = 1
} hybbx_crdop_radio_profile_t;

/** Parse radio_profile INI value (cb, amateur). */
hybbx_crdop_radio_profile_t hybbx_crdop_profile_parse(const char *value);

const char *hybbx_crdop_profile_name(hybbx_crdop_radio_profile_t profile);

/** Default ARQ bandwidth string for profile when INI omits arq_bandwidth. */
const char *hybbx_crdop_default_arq_bandwidth(hybbx_crdop_radio_profile_t profile);

/** Non-zero when bandwidth string is above CB-safe ceiling (1000MAX). */
int hybbx_crdop_bandwidth_exceeds_cb(const char *arq_bandwidth);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_CRDOP_H */
