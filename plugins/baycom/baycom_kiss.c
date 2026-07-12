#include "baycom_kiss.h"

#include "hybbx/kiss.h"
#include "serial_port.h"
#include "hybbx/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct baycom_kiss_modem {
    hybbx_baycom_config_t cfg;
    hybbx_serial_port_t *serial;
    hybbx_kiss_decoder_t kiss_dec;
    baycom_modem_frame_cb frame_cb;
    void *frame_userdata;
};

static char *baycom_strdup(const char *s)
{
    size_t len;
    char *copy;

    if (s == NULL) {
        return NULL;
    }

    len = strlen(s) + 1;
    copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len);
    }
    return copy;
}

static void on_kiss_frame(uint8_t port, const uint8_t *frame, size_t len,
                          void *userdata)
{
    baycom_kiss_modem_t *km = (baycom_kiss_modem_t *)userdata;

    (void)port;

    if (km == NULL || km->frame_cb == NULL || len == 0) {
        return;
    }

    km->frame_cb(frame, len, km->frame_userdata);
}

static hybbx_result_t kiss_send_param(baycom_kiss_modem_t *km,
                                      hybbx_kiss_command_t cmd, uint8_t value)
{
    uint8_t out[16];
    size_t out_len;

    out_len = hybbx_kiss_encode(0, cmd, &value, 1, out, sizeof(out));
    if (out_len == 0) {
        return HYBBX_ERR_IO;
    }

    return hybbx_serial_write(km->serial, out, out_len);
}

static hybbx_result_t kiss_apply_channel_params(baycom_kiss_modem_t *km)
{
    hybbx_result_t rc;

    rc = kiss_send_param(km, HYBBX_KISS_CMD_TXDELAY, (uint8_t)km->cfg.txdelay);
    if (rc != HYBBX_OK) {
        return rc;
    }
    rc = kiss_send_param(km, HYBBX_KISS_CMD_PERSIST, (uint8_t)km->cfg.persist);
    if (rc != HYBBX_OK) {
        return rc;
    }
    rc = kiss_send_param(km, HYBBX_KISS_CMD_SLOTTIME, (uint8_t)km->cfg.slot);
    if (rc != HYBBX_OK) {
        return rc;
    }
    rc = kiss_send_param(km, HYBBX_KISS_CMD_TXTAIL, (uint8_t)km->cfg.txtail);
    if (rc != HYBBX_OK) {
        return rc;
    }
    return kiss_send_param(km, HYBBX_KISS_CMD_FULLDUPLEX,
                           km->cfg.full_duplex ? 1u : 0u);
}

hybbx_result_t baycom_kiss_open(baycom_kiss_modem_t **out,
                                const hybbx_baycom_config_t *cfg,
                                baycom_modem_frame_cb cb, void *userdata)
{
    baycom_kiss_modem_t *km;
    hybbx_serial_params_t sparams;
    hybbx_result_t rc;

    if (out == NULL || cfg == NULL || cfg->device == NULL) {
        return HYBBX_ERR_INVALID;
    }

    *out = NULL;
    km = calloc(1, sizeof(*km));
    if (km == NULL) {
        return HYBBX_ERR_NOMEM;
    }

    km->cfg = *cfg;
    km->cfg.device = baycom_strdup(cfg->device);
    km->cfg.mycall = baycom_strdup(cfg->mycall);
    km->cfg.circuit_host = baycom_strdup(cfg->circuit_host);
    km->cfg.link_id = baycom_strdup(cfg->link_id);
    km->cfg.link_password = baycom_strdup(cfg->link_password);
    km->cfg.link_role = baycom_strdup(cfg->link_role);
    km->cfg.frequency_mhz = baycom_strdup(cfg->frequency_mhz);
    km->frame_cb = cb;
    km->frame_userdata = userdata;
    hybbx_kiss_decoder_init(&km->kiss_dec);

    hybbx_serial_params_default(&sparams, cfg->serial_baud);
    sparams.data_bits = 8;
    sparams.parity = HYBBX_SERIAL_PARITY_NONE;
    sparams.stop_bits = 1;
    sparams.assert_modem_lines = 0;

    rc = hybbx_serial_open(&km->serial, km->cfg.device, &sparams);
    if (rc != HYBBX_OK) {
        baycom_kiss_close(km);
        return rc;
    }

    rc = kiss_apply_channel_params(km);
    if (rc != HYBBX_OK) {
        hybbx_log_warn("[baycom] kiss channel params failed");
    }

    *out = km;
    return HYBBX_OK;
}

void baycom_kiss_close(baycom_kiss_modem_t *km)
{
    if (km == NULL) {
        return;
    }

    if (km->serial != NULL) {
        hybbx_serial_close(km->serial);
    }

    hybbx_baycom_config_free(&km->cfg);
    free(km);
}

hybbx_result_t baycom_kiss_poll(baycom_kiss_modem_t *km)
{
    uint8_t buf[256];
    size_t read_len = 0;
    hybbx_result_t rc;

    if (km == NULL || km->serial == NULL) {
        return HYBBX_ERR_IO;
    }

    rc = hybbx_serial_read(km->serial, buf, sizeof(buf), &read_len);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (read_len > 0) {
        hybbx_kiss_decoder_feed(&km->kiss_dec, buf, read_len, on_kiss_frame, km);
    }

    return HYBBX_OK;
}

hybbx_result_t baycom_kiss_send_frame(baycom_kiss_modem_t *km,
                                      const uint8_t *frame, size_t len)
{
    uint8_t out[HYBBX_KISS_MAX_FRAME + 8];
    size_t out_len;

    if (km == NULL || km->serial == NULL || frame == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    out_len = hybbx_kiss_encode(0, HYBBX_KISS_CMD_DATA, frame, len,
                                out, sizeof(out));
    if (out_len == 0) {
        return HYBBX_ERR_IO;
    }

    return hybbx_serial_write(km->serial, out, out_len);
}
