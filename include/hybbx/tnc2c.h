#ifndef HYBBX_TNC2C_H
#define HYBBX_TNC2C_H

/**
 * Backward-compatible header for the Landolt TNC2C driver.
 * New code should include hybbx/tnc.h.
 */

#include "hybbx/tnc.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef hybbx_tnc_t hybbx_tnc2c_t;
typedef hybbx_tnc_params_t hybbx_tnc2c_params_t;

/* hybbx_tnc2c_open removed — use hybbx_tnc_open() with RX callback. */
#define hybbx_tnc2c_close hybbx_tnc_close
#define hybbx_tnc2c_poll hybbx_tnc_poll
#define hybbx_tnc2c_send_frame hybbx_tnc_send_frame

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_TNC2C_H */
