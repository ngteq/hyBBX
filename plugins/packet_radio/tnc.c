#if defined(__linux__) || defined(__GLIBC__)
#define _DEFAULT_SOURCE 1
#endif

#include "hybbx/tnc.h"
#include "hybbx/kiss.h"
#include "hybbx/traffic.h"
#include "serial_port.h"
#include "sixpack.h"
#include "tnc_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if !defined(_WIN32)
#include <sys/select.h>
#endif

struct hybbx_tnc {
    hybbx_packet_radio_config_t config;
    hybbx_serial_port_t *serial;
    hybbx_kiss_decoder_t kiss_dec;
    hybbx_sixpack_decoder_t sixpack_dec;
    hybbx_tnc_host_t host;
    hybbx_ax25_path_t tx_path;
    int kiss_active;
    int host_ready;
    hybbx_tnc_frame_cb frame_cb;
    hybbx_tnc_ui_cb ui_cb;
    void *rx_userdata;
    unsigned long long last_rx_ms;
};

static void config_copy(hybbx_packet_radio_config_t *dst,
                        const hybbx_packet_radio_config_t *src)
{
    unsigned i;

    memset(dst, 0, sizeof(*dst));
    dst->device_type = src->device_type;
    dst->device = src->device;
    dst->baud = src->baud;
    dst->tnc = src->tnc;
    dst->protocol = src->protocol;
    dst->params = src->params;
    dst->mycall = src->mycall;
    dst->dest_call = src->dest_call;
    dst->via_count = src->via_count;
    for (i = 0; i < src->via_count; i++) {
        dst->via[i] = src->via[i];
    }
}

static unsigned long long tnc_monotonic_ms(void)
{
#if defined(CLOCK_MONOTONIC)
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return (unsigned long long)ts.tv_sec * 1000ull +
               (unsigned long long)ts.tv_nsec / 1000000ull;
    }
#endif
    return (unsigned long long)time(NULL) * 1000ull;
}

static void tnc_note_rx(hybbx_tnc_t *tnc)
{
    if (tnc != NULL) {
        tnc->last_rx_ms = tnc_monotonic_ms();
    }
}

static hybbx_result_t tnc_wait_half_duplex_clear(hybbx_tnc_t *tnc)
{
    unsigned guard_ms;
    unsigned long long now;
    unsigned long long elapsed;

    if (tnc == NULL || hybbx_tnc_duplex_is_full(tnc)) {
        return HYBBX_OK;
    }

    guard_ms = tnc->config.params.slot * 10u;
    if (guard_ms < 50u) {
        guard_ms = 50u;
    }

    now = tnc_monotonic_ms();
    if (tnc->last_rx_ms == 0 || now <= tnc->last_rx_ms) {
        return HYBBX_OK;
    }

    elapsed = now - tnc->last_rx_ms;
    if (elapsed >= guard_ms) {
        return HYBBX_OK;
    }

#if defined(_WIN32)
    Sleep((DWORD)(guard_ms - elapsed));
#else
    {
        struct timeval tv;

        tv.tv_sec = (time_t)((guard_ms - elapsed) / 1000u);
        tv.tv_usec = (suseconds_t)(((guard_ms - elapsed) % 1000u) * 1000u);
        (void)select(0, NULL, NULL, NULL, &tv);
    }
#endif

    return HYBBX_OK;
}

static hybbx_result_t send_raw(hybbx_tnc_t *tnc, const uint8_t *data, size_t len)
{
    return hybbx_serial_write(tnc->serial, data, len);
}

static hybbx_result_t send_cmd(hybbx_tnc_t *tnc, const char *cmd)
{
    char line[128];
    size_t len;

    len = hybbx_tnc_host_format_cmd(cmd, line, sizeof(line));
    if (len == 0) {
        return HYBBX_ERR_INVALID;
    }

    return send_raw(tnc, (const uint8_t *)line, len);
}

static hybbx_result_t drain_serial(hybbx_tnc_t *tnc, unsigned rounds)
{
    uint8_t buf[128];
    size_t read_len;
    unsigned i;

    for (i = 0; i < rounds; i++) {
        if (hybbx_serial_read(tnc->serial, buf, sizeof(buf),
                              &read_len) != HYBBX_OK) {
            return HYBBX_ERR_IO;
        }
        if (read_len == 0) {
            break;
        }
    }

    return HYBBX_OK;
}

static hybbx_result_t send_kiss_param(hybbx_tnc_t *tnc,
                                      hybbx_kiss_command_t cmd,
                                      uint8_t value)
{
    uint8_t frame[16];
    size_t len;

    len = hybbx_kiss_encode((uint8_t)tnc->config.params.kiss_port, cmd,
                            &value, 1, frame, sizeof(frame));
    if (len == 0) {
        return HYBBX_ERR_IO;
    }

    return send_raw(tnc, frame, len);
}

static hybbx_result_t tnc_apply_kiss_params(hybbx_tnc_t *tnc)
{
    hybbx_result_t rc;

    rc = send_kiss_param(tnc, HYBBX_KISS_CMD_TXDELAY,
                         (uint8_t)tnc->config.params.txdelay);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = send_kiss_param(tnc, HYBBX_KISS_CMD_PERSIST,
                         (uint8_t)tnc->config.params.persist);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = send_kiss_param(tnc, HYBBX_KISS_CMD_SLOTTIME,
                         (uint8_t)tnc->config.params.slot);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = send_kiss_param(tnc, HYBBX_KISS_CMD_TXTAIL,
                         (uint8_t)tnc->config.params.txtail);
    if (rc != HYBBX_OK) {
        return rc;
    }

    return send_kiss_param(tnc, HYBBX_KISS_CMD_FULLDUPLEX,
                           (uint8_t)(tnc->config.params.fullduplex ? 1 : 0));
}

static hybbx_result_t tnc_apply_radio_modulation(hybbx_tnc_t *tnc)
{
    char cmd[64];

    if (tnc == NULL ||
        !hybbx_packet_radio_modulation_is_g3ruh(tnc->config.params.modulation)) {
        return HYBBX_OK;
    }

    drain_serial(tnc, 2);
    (void)send_cmd(tnc, "MODEM 9600");
    (void)send_cmd(tnc, "HB 9600");
    if (tnc->config.tnc == HYBBX_PACKET_RADIO_TNC_TNC2C) {
        snprintf(cmd, sizeof(cmd), "CLOCK %.1f",
                 (double)tnc->config.params.clock_mhz);
        (void)send_cmd(tnc, cmd);
    }
    (void)send_cmd(tnc, "PACLEN 256");
    drain_serial(tnc, 2);
    return HYBBX_OK;
}

static hybbx_result_t tnc_enter_kiss_mode(hybbx_tnc_t *tnc)
{
    if (!tnc->config.params.kiss_on_startup) {
        tnc->kiss_active = 1;
        return HYBBX_OK;
    }

    if (send_cmd(tnc, "kiss on") != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    drain_serial(tnc, 8);
    tnc->kiss_active = 1;
    return HYBBX_OK;
}

static void tnc_deliver_ui(hybbx_tnc_t *tnc, const uint8_t *frame, size_t len)
{
    uint8_t payload[HYBBX_AX25_PAYLOAD_MAX];
    hybbx_ax25_path_t path;
    size_t payload_len;

    payload_len = hybbx_ax25_parse_ui(frame, len, &path, payload,
                                      sizeof(payload));
    if (payload_len == 0) {
        return;
    }

    if (tnc->ui_cb != NULL) {
        tnc->ui_cb(payload, payload_len, &path, tnc->rx_userdata);
    }

    tnc_note_rx(tnc);
}

static void on_kiss_frame(uint8_t port, const uint8_t *frame, size_t len,
                          void *userdata)
{
    hybbx_tnc_t *tnc = (hybbx_tnc_t *)userdata;
    uint8_t payload[HYBBX_AX25_PAYLOAD_MAX];
    hybbx_ax25_path_t path;
    size_t payload_len;

    (void)port;

    if (tnc->frame_cb != NULL) {
        tnc->frame_cb(frame, len, tnc->rx_userdata);
    }

    if (tnc->ui_cb == NULL) {
        return;
    }

    payload_len = hybbx_ax25_parse_ui(frame, len, &path, payload,
                                      sizeof(payload));
    if (payload_len > 0) {
        tnc->ui_cb(payload, payload_len, &path, tnc->rx_userdata);
    }
}

static void on_sixpack_frame(const uint8_t *frame, size_t len, void *userdata)
{
    hybbx_tnc_t *tnc = (hybbx_tnc_t *)userdata;

    if (tnc->frame_cb != NULL) {
        tnc->frame_cb(frame, len, tnc->rx_userdata);
    }

    if (tnc->ui_cb != NULL) {
        tnc_deliver_ui(tnc, frame, len);
    }
}

static void on_host_text(const uint8_t *data, size_t len, void *userdata)
{
    hybbx_tnc_t *tnc = (hybbx_tnc_t *)userdata;

    if (tnc->ui_cb != NULL) {
        tnc->ui_cb(data, len, NULL, tnc->rx_userdata);
    }

    tnc_note_rx(tnc);
}

static void on_host_event(hybbx_tnc_host_state_t state, void *userdata)
{
    hybbx_tnc_t *tnc = (hybbx_tnc_t *)userdata;

    if (state == HYBBX_TNC_HOST_CMD) {
        tnc->host_ready = 1;
    }

    printf("[tnc] host state -> %d\n", (int)state);
}

static hybbx_result_t tnc_build_tx_path(hybbx_tnc_t *tnc)
{
    unsigned i;
    hybbx_result_t rc;

    memset(&tnc->tx_path, 0, sizeof(tnc->tx_path));

    if (tnc->config.dest_call != NULL && tnc->config.dest_call[0] != '\0') {
        rc = hybbx_ax25_address_parse(tnc->config.dest_call,
                                      &tnc->tx_path.dest);
        if (rc != HYBBX_OK) {
            return rc;
        }
    } else {
        (void)hybbx_ax25_address_parse("QST", &tnc->tx_path.dest);
    }

    if (tnc->config.mycall != NULL && tnc->config.mycall[0] != '\0') {
        rc = hybbx_ax25_address_parse(tnc->config.mycall,
                                      &tnc->tx_path.source);
        if (rc != HYBBX_OK) {
            return rc;
        }
    } else {
        (void)hybbx_ax25_address_parse("HYBBX", &tnc->tx_path.source);
    }

    for (i = 0; i < tnc->config.via_count; i++) {
        rc = hybbx_ax25_address_parse(tnc->config.via[i],
                                      &tnc->tx_path.digi[i]);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }
    tnc->tx_path.digi_count = tnc->config.via_count;

    return HYBBX_OK;
}

static const char *tnc_profile_name(hybbx_packet_radio_tnc_t tnc)
{
    switch (tnc) {
    case HYBBX_PACKET_RADIO_TNC_BAYCOM:
        return "baycom";
    case HYBBX_PACKET_RADIO_TNC_PCCOM:
        return "pccom";
    case HYBBX_PACKET_RADIO_TNC_GENERIC:
        return "generic";
    case HYBBX_PACKET_RADIO_TNC_TNC2C:
    default:
        return "tnc2c";
    }
}

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


static hybbx_result_t tnc_profile_init(hybbx_tnc_t *tnc)
{
    hybbx_result_t rc;
    char cmd[96];

    rc = tnc_build_tx_path(tnc);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = tnc_apply_radio_modulation(tnc);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (tnc->config.protocol == HYBBX_PACKET_RADIO_PROTO_KISS) {
        rc = tnc_enter_kiss_mode(tnc);
        if (rc != HYBBX_OK) {
            return rc;
        }
        return tnc_apply_kiss_params(tnc);
    }

    if (tnc->config.protocol == HYBBX_PACKET_RADIO_PROTO_SIXPACK) {
        tnc->kiss_active = 0;
        return HYBBX_OK;
    }

    /* Host mode setup (TNC2 / PC-COM / BayCom with firmware TNC). */
    drain_serial(tnc, 4);
    (void)send_cmd(tnc, "");

    if (tnc->config.mycall != NULL && tnc->config.mycall[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "MYCALL %s", tnc->config.mycall);
        rc = send_cmd(tnc, cmd);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    snprintf(cmd, sizeof(cmd), "TXDELAY %u", tnc->config.params.txdelay);
    (void)send_cmd(tnc, cmd);
    snprintf(cmd, sizeof(cmd), "Persist %u", tnc->config.params.persist);
    (void)send_cmd(tnc, cmd);
    snprintf(cmd, sizeof(cmd), "SlotTime %u", tnc->config.params.slot);
    (void)send_cmd(tnc, cmd);
    snprintf(cmd, sizeof(cmd), "TXTAIL %u", tnc->config.params.txtail);
    (void)send_cmd(tnc, cmd);

    if (tnc->config.params.fullduplex) {
        (void)send_cmd(tnc, "FULLDUP ON");
    } else {
        (void)send_cmd(tnc, "FULLDUP OFF");
    }

    if (tnc->config.tnc == HYBBX_PACKET_RADIO_TNC_PCCOM) {
        (void)send_cmd(tnc, "PACLEN 256");
        (void)send_cmd(tnc, "MAXFRAME 4");
    }

    if (tnc->config.tnc == HYBBX_PACKET_RADIO_TNC_BAYCOM) {
        (void)send_cmd(tnc, "PACLEN 256");
    }

    if (tnc->config.params.host_connect_on_start &&
        tnc->config.dest_call != NULL && tnc->config.dest_call[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "C %s", tnc->config.dest_call);
        rc = send_cmd(tnc, cmd);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    drain_serial(tnc, 6);
    return HYBBX_OK;
}

hybbx_result_t hybbx_tnc_open(hybbx_tnc_t **out,
                              const hybbx_packet_radio_config_t *config,
                              hybbx_tnc_frame_cb frame_cb,
                              hybbx_tnc_ui_cb ui_cb,
                              void *rx_userdata)
{
    hybbx_tnc_t *tnc;
    hybbx_result_t rc;

    if (out == NULL || config == NULL || config->device == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (frame_cb == NULL && ui_cb == NULL) {
        return HYBBX_ERR_INVALID;
    }

    tnc = calloc(1, sizeof(*tnc));
    if (tnc == NULL) {
        return HYBBX_ERR_NOMEM;
    }

    config_copy(&tnc->config, config);
    tnc->frame_cb = frame_cb;
    tnc->ui_cb = ui_cb;
    tnc->rx_userdata = rx_userdata;

    hybbx_kiss_decoder_init(&tnc->kiss_dec);
    hybbx_sixpack_decoder_init(&tnc->sixpack_dec);
    hybbx_tnc_host_init(&tnc->host, on_host_text, on_host_event, tnc);

    rc = hybbx_serial_open(&tnc->serial, config->device, config->baud);
    if (rc != HYBBX_OK) {
        fprintf(stderr, "[tnc] failed to open %s at %u baud\n",
                config->device, config->baud);
        free(tnc);
        return rc;
    }

    printf("[tnc] profile=%s protocol=%u device=%s host=%u radio=%u "
           "modem=%s modulation=%s band=%s duplex=%s\n",
           tnc_profile_name(config->tnc), (unsigned)config->protocol,
           config->device, config->baud, config->params.radio_baud,
           modem_name(config->params.modem),
           hybbx_packet_radio_modulation_name(config->params.modulation),
           hybbx_packet_radio_band_name(config->params.band),
           hybbx_packet_radio_duplex_name(config->params.duplex));

    rc = tnc_profile_init(tnc);
    if (rc != HYBBX_OK) {
        hybbx_tnc_close(tnc);
        return rc;
    }

    if (config->protocol == HYBBX_PACKET_RADIO_PROTO_KISS) {
        printf("[tnc] KISS active (port %u)\n", config->params.kiss_port);
    } else if (config->protocol == HYBBX_PACKET_RADIO_PROTO_SIXPACK) {
        printf("[tnc] 6PACK active\n");
    } else {
        printf("[tnc] TNC2 host mode active\n");
    }

    *out = tnc;
    return HYBBX_OK;
}

void hybbx_tnc_close(hybbx_tnc_t *tnc)
{
    if (tnc == NULL) {
        return;
    }

    if (tnc->serial != NULL) {
        if (tnc->kiss_active &&
            tnc->config.protocol == HYBBX_PACKET_RADIO_PROTO_KISS) {
            (void)send_cmd(tnc, "kiss off");
        }
        hybbx_serial_close(tnc->serial);
    }

    free(tnc);
}

hybbx_result_t hybbx_tnc_send_frame(hybbx_tnc_t *tnc,
                                    const uint8_t *frame, size_t len)
{
    uint8_t buf[HYBBX_KISS_MAX_FRAME + 8];
    size_t encoded;
    hybbx_result_t rc;

    if (tnc == NULL || frame == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    rc = tnc_wait_half_duplex_clear(tnc);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (tnc->config.protocol == HYBBX_PACKET_RADIO_PROTO_SIXPACK) {
        encoded = hybbx_sixpack_encode(frame, len, buf, sizeof(buf));
        if (encoded == 0) {
            return HYBBX_ERR_IO;
        }
        return send_raw(tnc, buf, encoded);
    }

    if (!tnc->kiss_active &&
        tnc->config.protocol != HYBBX_PACKET_RADIO_PROTO_KISS) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    encoded = hybbx_kiss_encode((uint8_t)tnc->config.params.kiss_port,
                                HYBBX_KISS_CMD_DATA, frame, len,
                                buf, sizeof(buf));
    if (encoded == 0) {
        return HYBBX_ERR_IO;
    }

    return send_raw(tnc, buf, encoded);
}

hybbx_result_t hybbx_tnc_send_ui(hybbx_tnc_t *tnc,
                                 const uint8_t *payload, size_t len)
{
    uint8_t frame[HYBBX_AX25_FRAME_MAX];
    size_t frame_len;

    if (tnc == NULL) {
        return HYBBX_ERR_INVALID;
    }

    frame_len = hybbx_ax25_build_ui(&tnc->tx_path, payload, len,
                                    frame, sizeof(frame));
    if (frame_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    return hybbx_tnc_send_frame(tnc, frame, frame_len);
}

hybbx_result_t hybbx_tnc_send_converse(hybbx_tnc_t *tnc,
                                       const char *data, size_t len)
{
    hybbx_result_t rc;

    if (tnc == NULL || data == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (tnc->config.protocol != HYBBX_PACKET_RADIO_PROTO_HOSTMODE) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    rc = tnc_wait_half_duplex_clear(tnc);
    if (rc != HYBBX_OK) {
        return rc;
    }

    return send_raw(tnc, (const uint8_t *)data, len);
}

hybbx_packet_radio_duplex_t hybbx_tnc_duplex(const hybbx_tnc_t *tnc)
{
    if (tnc == NULL) {
        return HYBBX_PACKET_RADIO_DUPLEX_HALF;
    }

    return tnc->config.params.duplex;
}

hybbx_packet_radio_band_t hybbx_tnc_band(const hybbx_tnc_t *tnc)
{
    if (tnc == NULL) {
        return HYBBX_PACKET_RADIO_BAND_UNSET;
    }

    return tnc->config.params.band;
}

int hybbx_tnc_duplex_is_full(const hybbx_tnc_t *tnc)
{
    return tnc != NULL &&
           tnc->config.params.duplex == HYBBX_PACKET_RADIO_DUPLEX_FULL;
}

int hybbx_tnc_host_connected(const hybbx_tnc_t *tnc)
{
    if (tnc == NULL) {
        return 0;
    }

    return tnc->host.state == HYBBX_TNC_HOST_CONNECTED;
}

hybbx_result_t hybbx_tnc_poll(hybbx_tnc_t *tnc)
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

    switch (tnc->config.protocol) {
    case HYBBX_PACKET_RADIO_PROTO_KISS:
        hybbx_kiss_decoder_feed(&tnc->kiss_dec, buf, read_len,
                                on_kiss_frame, tnc);
        break;
    case HYBBX_PACKET_RADIO_PROTO_SIXPACK:
        hybbx_sixpack_decoder_feed(&tnc->sixpack_dec, buf, read_len,
                                   on_sixpack_frame, tnc);
        break;
    case HYBBX_PACKET_RADIO_PROTO_HOSTMODE:
        hybbx_tnc_host_feed(&tnc->host, buf, read_len);
        break;
    default:
        break;
    }

    return HYBBX_OK;
}
