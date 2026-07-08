#ifndef BAYCOM_MODEM_H
#define BAYCOM_MODEM_H

#include "hybbx/baycom.h"
#include "hybbx/types.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*baycom_modem_frame_cb)(const uint8_t *frame, size_t len,
                                      void *userdata);

typedef struct baycom_modem baycom_modem_t;

hybbx_result_t baycom_modem_open(baycom_modem_t **out,
                                 const hybbx_baycom_config_t *cfg,
                                 baycom_modem_frame_cb cb, void *userdata);
void baycom_modem_close(baycom_modem_t *modem);
hybbx_result_t baycom_modem_poll(baycom_modem_t *modem);
hybbx_result_t baycom_modem_send_frame(baycom_modem_t *modem,
                                       const uint8_t *frame, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BAYCOM_MODEM_H */
