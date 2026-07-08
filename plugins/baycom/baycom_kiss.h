#ifndef BAYCOM_KISS_H
#define BAYCOM_KISS_H

#include "baycom_modem.h"
#include "hybbx/baycom.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct baycom_kiss_modem baycom_kiss_modem_t;

hybbx_result_t baycom_kiss_open(baycom_kiss_modem_t **out,
                                const hybbx_baycom_config_t *cfg,
                                baycom_modem_frame_cb cb, void *userdata);
void baycom_kiss_close(baycom_kiss_modem_t *modem);
hybbx_result_t baycom_kiss_poll(baycom_kiss_modem_t *modem);
hybbx_result_t baycom_kiss_send_frame(baycom_kiss_modem_t *modem,
                                      const uint8_t *frame, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BAYCOM_KISS_H */
