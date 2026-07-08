#include "baycom_modem.h"
#include "baycom_kernel.h"
#include "baycom_kiss.h"

#include <stdlib.h>

struct baycom_modem {
    hybbx_baycom_backend_t backend;
    baycom_kernel_modem_t *kernel;
    baycom_kiss_modem_t *kiss;
};

hybbx_result_t baycom_modem_open(baycom_modem_t **out,
                                 const hybbx_baycom_config_t *cfg,
                                 baycom_modem_frame_cb cb, void *userdata)
{
    baycom_modem_t *modem;
    hybbx_result_t rc;

    if (out == NULL || cfg == NULL) {
        return HYBBX_ERR_INVALID;
    }

    *out = NULL;
    modem = calloc(1, sizeof(*modem));
    if (modem == NULL) {
        return HYBBX_ERR_NOMEM;
    }

    modem->backend = cfg->backend;

    if (cfg->backend == HYBBX_BAYCOM_BACKEND_KISS) {
        rc = baycom_kiss_open(&modem->kiss, cfg, cb, userdata);
    } else {
        rc = baycom_kernel_open(&modem->kernel, cfg, cb, userdata);
    }

    if (rc != HYBBX_OK) {
        baycom_modem_close(modem);
        return rc;
    }

    *out = modem;
    return HYBBX_OK;
}

void baycom_modem_close(baycom_modem_t *modem)
{
    if (modem == NULL) {
        return;
    }

    if (modem->kernel != NULL) {
        baycom_kernel_close(modem->kernel);
    }
    if (modem->kiss != NULL) {
        baycom_kiss_close(modem->kiss);
    }

    free(modem);
}

hybbx_result_t baycom_modem_poll(baycom_modem_t *modem)
{
    if (modem == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (modem->kiss != NULL) {
        return baycom_kiss_poll(modem->kiss);
    }
    if (modem->kernel != NULL) {
        return baycom_kernel_poll(modem->kernel);
    }

    return HYBBX_ERR_IO;
}

hybbx_result_t baycom_modem_send_frame(baycom_modem_t *modem,
                                       const uint8_t *frame, size_t len)
{
    if (modem == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (modem->kiss != NULL) {
        return baycom_kiss_send_frame(modem->kiss, frame, len);
    }
    if (modem->kernel != NULL) {
        return baycom_kernel_send_frame(modem->kernel, frame, len);
    }

    return HYBBX_ERR_IO;
}
