#include "hybbx/tnc2c.h"
#include "hybbx/kiss.h"
#include "serial_port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct hybbx_tnc2c {
    hybbx_serial_port_t *serial;
    hybbx_packet_radio_config_t config;
    hybbx_kiss_decoder_t decoder;
    int kiss_active;
};

static const char *modem_name(hybbx_tnc2c_modem_t modem)
{
    switch (modem) {
    case HYBBX_TNC2C_MODEM_9600:
        return "9600";
    case HYBBX_TNC2C_MODEM_19200:
        return "19200";
    case HYBBX_TNC2C_MODEM_TCM3105:
    default:
        return "tcm3105";
    }
}

static hybbx_result_t send_raw(hybbx_tnc2c_t *tnc, const uint8_t *data, size_t len)
{
    return hybbx_serial_write(tnc->serial, data, len);
}

static hybbx_result_t send_kiss_param(hybbx_tnc2c_t *tnc,
                                      hybbx_kiss_command_t cmd,
                                      uint8_t value)
{
    uint8_t frame[16];
    size_t len;

    len = hybbx_kiss_encode((uint8_t)tnc->config.tnc2c.kiss_port, cmd,
                            &value, 1, frame, sizeof(frame));
    if (len == 0) {
        return HYBBX_ERR_IO;
    }

    return send_raw(tnc, frame, len);
}

static hybbx_result_t tnc2c_apply_kiss_params(hybbx_tnc2c_t *tnc)
{
    hybbx_result_t rc;

    rc = send_kiss_param(tnc, HYBBX_KISS_CMD_TXDELAY,
                         (uint8_t)tnc->config.tnc2c.txdelay);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = send_kiss_param(tnc, HYBBX_KISS_CMD_PERSIST,
                         (uint8_t)tnc->config.tnc2c.persist);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = send_kiss_param(tnc, HYBBX_KISS_CMD_SLOTTIME,
                         (uint8_t)tnc->config.tnc2c.slot);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = send_kiss_param(tnc, HYBBX_KISS_CMD_TXTAIL,
                         (uint8_t)tnc->config.tnc2c.txtail);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = send_kiss_param(tnc, HYBBX_KISS_CMD_FULLDUPLEX,
                         (uint8_t)(tnc->config.tnc2c.fullduplex ? 1 : 0));
    return rc;
}

static hybbx_result_t tnc2c_enter_kiss_mode(hybbx_tnc2c_t *tnc)
{
    static const char kiss_on[] = "kiss on\r";
    uint8_t drain[128];
    size_t read_len;
    size_t i;

    if (tnc->config.protocol != HYBBX_PACKET_RADIO_PROTO_KISS) {
        return HYBBX_OK;
    }

    if (!tnc->config.tnc2c.kiss_on_startup) {
        tnc->kiss_active = 1;
        return HYBBX_OK;
    }

    if (send_raw(tnc, (const uint8_t *)kiss_on, sizeof(kiss_on) - 1) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    for (i = 0; i < 8; i++) {
        if (hybbx_serial_read(tnc->serial, drain, sizeof(drain),
                              &read_len) != HYBBX_OK) {
            return HYBBX_ERR_IO;
        }
        if (read_len == 0) {
            break;
        }
    }

    tnc->kiss_active = 1;
    return HYBBX_OK;
}

static void on_kiss_frame(uint8_t port, const uint8_t *frame, size_t len,
                          void *userdata)
{
    hybbx_tnc2c_t *tnc = (hybbx_tnc2c_t *)userdata;

    (void)port;
    printf("[tnc2c] AX.25 frame received (%zu bytes", len);
    if (len >= 2) {
        printf(", dest=%02x:%02x", frame[0], frame[1]);
    }
    printf(")\n");

    (void)tnc;
    /* TODO: forward to HyBBX session/mailbox layer */
}

static void config_copy(hybbx_packet_radio_config_t *dst,
                        const hybbx_packet_radio_config_t *src)
{
    memset(dst, 0, sizeof(*dst));
    dst->device_type = src->device_type;
    dst->device = src->device;
    dst->baud = src->baud;
    dst->tnc = src->tnc;
    dst->protocol = src->protocol;
    dst->tnc2c = src->tnc2c;
}

hybbx_result_t hybbx_tnc2c_open(hybbx_tnc2c_t **out,
                                  const hybbx_packet_radio_config_t *config)
{
    hybbx_tnc2c_t *tnc;
    hybbx_result_t rc;

    if (out == NULL || config == NULL || config->device == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (config->tnc != HYBBX_PACKET_RADIO_TNC_TNC2C) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    tnc = calloc(1, sizeof(*tnc));
    if (tnc == NULL) {
        return HYBBX_ERR_NOMEM;
    }

    config_copy(&tnc->config, config);
    hybbx_kiss_decoder_init(&tnc->decoder);

    rc = hybbx_serial_open(&tnc->serial, config->device, config->baud);
    if (rc != HYBBX_OK) {
        fprintf(stderr, "[tnc2c] failed to open %s at %u baud\n",
                config->device, config->baud);
        free(tnc);
        return rc;
    }

    printf("[tnc2c] opened %s host=%u baud radio=%u baud modem=%s clock=%.1f MHz\n",
           config->device, config->baud, config->tnc2c.radio_baud,
           modem_name(config->tnc2c.modem), (double)config->tnc2c.clock_mhz);

    if (config->protocol == HYBBX_PACKET_RADIO_PROTO_KISS) {
        rc = tnc2c_enter_kiss_mode(tnc);
        if (rc != HYBBX_OK) {
            hybbx_tnc2c_close(tnc);
            return rc;
        }

        rc = tnc2c_apply_kiss_params(tnc);
        if (rc != HYBBX_OK) {
            hybbx_tnc2c_close(tnc);
            return rc;
        }

        printf("[tnc2c] KISS mode active (port %u, txdelay=%u persist=%u slot=%u)\n",
               config->tnc2c.kiss_port, config->tnc2c.txdelay,
               config->tnc2c.persist, config->tnc2c.slot);
    } else {
        printf("[tnc2c] host mode active (TNC2 command interface)\n");
    }

    *out = tnc;
    return HYBBX_OK;
}

void hybbx_tnc2c_close(hybbx_tnc2c_t *tnc)
{
    if (tnc == NULL) {
        return;
    }

    hybbx_serial_close(tnc->serial);
    tnc->serial = NULL;
    free(tnc);
}

hybbx_result_t hybbx_tnc2c_send_frame(hybbx_tnc2c_t *tnc,
                                      const uint8_t *frame, size_t len)
{
    uint8_t buf[HYBBX_KISS_MAX_FRAME + 8];
    size_t encoded;

    if (tnc == NULL || frame == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    if (!tnc->kiss_active &&
        tnc->config.protocol != HYBBX_PACKET_RADIO_PROTO_KISS) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    encoded = hybbx_kiss_encode((uint8_t)tnc->config.tnc2c.kiss_port,
                                HYBBX_KISS_CMD_DATA, frame, len,
                                buf, sizeof(buf));
    if (encoded == 0) {
        return HYBBX_ERR_IO;
    }

    return send_raw(tnc, buf, encoded);
}

hybbx_result_t hybbx_tnc2c_poll(hybbx_tnc2c_t *tnc)
{
    uint8_t buf[256];
    size_t read_len;
    hybbx_result_t rc;

    if (tnc == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_serial_read(tnc->serial, buf, sizeof(buf), &read_len);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (read_len == 0) {
        return HYBBX_OK;
    }

    if (tnc->kiss_active ||
        tnc->config.protocol == HYBBX_PACKET_RADIO_PROTO_KISS) {
        hybbx_kiss_decoder_feed(&tnc->decoder, buf, read_len,
                                on_kiss_frame, tnc);
    }

    return HYBBX_OK;
}
