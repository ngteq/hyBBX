#ifndef HYBBX_CRDOP_H
#define HYBBX_CRDOP_H

/**
 * CRDOP — CB Radio Digital Open Protocol (Level 2, experimental).
 *
 * HyBBX `crdop` plugin: CB host-client bridge to external ARDOPC/ardopcf.
 * Modem DSP stays outside HyBBX. Profile helpers shared with `ardop`.
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

struct hybbx_ardop_config;

/** Parse `[transport.crdop]` config (CB defaults). */
hybbx_result_t hybbx_crdop_config_parse(const char *config,
                                        struct hybbx_ardop_config *out);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_CRDOP_H */
