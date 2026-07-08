#ifndef BAYCOM_KERNEL_H
#define BAYCOM_KERNEL_H

#include "baycom_modem.h"
#include "hybbx/baycom.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct baycom_kernel_modem baycom_kernel_modem_t;

hybbx_result_t baycom_kernel_open(baycom_kernel_modem_t **out,
                                  const hybbx_baycom_config_t *cfg,
                                  baycom_modem_frame_cb cb, void *userdata);
void baycom_kernel_close(baycom_kernel_modem_t *modem);
hybbx_result_t baycom_kernel_poll(baycom_kernel_modem_t *modem);
hybbx_result_t baycom_kernel_send_frame(baycom_kernel_modem_t *modem,
                                        const uint8_t *frame, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* BAYCOM_KERNEL_H */
