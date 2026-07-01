#include "hybbx/plugin.h"
#include "hybbx/service.h"
#include "hybbx/packet_radio.h"
#include "hybbx/tnc.h"
#include "hybbx/circuit.h"
#include "hybbx/circuit_tcp.h"
#include "hybbx/traffic.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/select.h>
#endif

static hybbx_service_t *g_service;
static hybbx_packet_radio_config_t g_active_config;
static hybbx_tnc_t *g_tnc;
static int g_circuit_fd = -1;
static hybbx_circuit_decoder_t g_circuit_dec;
static pthread_t g_poll_thread;
static volatile int g_radio_running;
static int g_poll_thread_started;

extern const hybbx_transport_plugin_t hybbx_plugin_packet_radio;

static const char *device_type_name(hybbx_packet_radio_device_type_t type)
{
    return type == HYBBX_PACKET_RADIO_DEVICE_SERIAL ? "serial" : "usb";
}

static const char *protocol_name(hybbx_packet_radio_protocol_t proto)
{
    switch (proto) {
    case HYBBX_PACKET_RADIO_PROTO_HOSTMODE:
        return "hostmode";
    case HYBBX_PACKET_RADIO_PROTO_SIXPACK:
        return "6pack";
    case HYBBX_PACKET_RADIO_PROTO_KISS:
    default:
        return "kiss";
    }
}

static const char *tnc_name(hybbx_packet_radio_tnc_t tnc)
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

static hybbx_result_t circuit_uplink(const uint8_t *frame, size_t frame_len)
{
    uint8_t hbx[HYBBX_CIRCUIT_MAX_FRAME];
    size_t hbx_len;
    uint16_t flags = HYBBX_CIRCUIT_FLAG_RX;

    if (g_circuit_fd < 0) {
        return HYBBX_ERR_IO;
    }

    if (hybbx_packet_radio_modulation_is_g3ruh(
            g_active_config.params.modulation)) {
        flags |= HYBBX_CIRCUIT_FLAG_G3RUH_FSK;
    }

    hbx_len = hybbx_circuit_encode_ax25(frame, frame_len, flags,
                                        hbx, sizeof(hbx));
    if (hbx_len == 0) {
        return HYBBX_ERR_IO;
    }

    return hybbx_circuit_link_write(g_circuit_fd, hbx, hbx_len);
}

static void on_tnc_ax25_frame(const uint8_t *frame, size_t len, void *userdata)
{
    (void)userdata;

    if (len == 0) {
        return;
    }

    (void)circuit_uplink(frame, len);
}

static void on_tnc_host_ui(const uint8_t *payload, size_t len,
                         const hybbx_ax25_path_t *path, void *userdata)
{
    uint8_t hbx[HYBBX_CIRCUIT_MAX_FRAME];
    size_t hbx_len;

    (void)userdata;
    (void)path;

    if (g_circuit_fd < 0 || len == 0) {
        return;
    }

    hbx_len = hybbx_circuit_encode(HYBBX_CIRCUIT_PROTO_TERMINAL,
                                   HYBBX_CIRCUIT_FLAG_RX,
                                   payload, len, hbx, sizeof(hbx));
    if (hbx_len > 0) {
        (void)hybbx_circuit_link_write(g_circuit_fd, hbx, hbx_len);
    }
}

static hybbx_result_t packet_radio_send_bytes(const uint8_t *data, size_t len)
{
    if (g_tnc == NULL || data == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    if (g_active_config.protocol == HYBBX_PACKET_RADIO_PROTO_HOSTMODE) {
        if (hybbx_tnc_host_connected(g_tnc)) {
            return hybbx_tnc_send_converse(g_tnc, (const char *)data, len);
        }
        return HYBBX_ERR_BUSY;
    }

    return hybbx_tnc_send_ui(g_tnc, data, len);
}

static void on_circuit_downlink(hybbx_circuit_proto_t proto, uint16_t flags,
                                const uint8_t *payload, size_t len,
                                void *userdata)
{
    (void)flags;
    (void)userdata;

    if (g_tnc == NULL || len == 0) {
        return;
    }

    switch (proto) {
    case HYBBX_CIRCUIT_PROTO_TERMINAL:
        (void)packet_radio_send_bytes(payload, len);
        break;
    case HYBBX_CIRCUIT_PROTO_AX25:
        (void)hybbx_tnc_send_frame(g_tnc, payload, len);
        break;
    case HYBBX_CIRCUIT_PROTO_AX25_UI: {
        uint8_t ui[HYBBX_AX25_PAYLOAD_MAX];
        size_t ui_len = hybbx_circuit_unpack_ax25_ui(payload, len, NULL,
                                                      ui, sizeof(ui));
        if (ui_len > 0) {
            (void)packet_radio_send_bytes(ui, ui_len);
        }
        break;
    }
    default:
        break;
    }
}

static void packet_radio_poll_sleep_ms(unsigned ms)
{
#if defined(_WIN32)
    Sleep(ms);
#else
    struct timeval tv;

    tv.tv_sec = (time_t)(ms / 1000u);
    tv.tv_usec = (suseconds_t)((ms % 1000u) * 1000u);
    (void)select(0, NULL, NULL, NULL, &tv);
#endif
}

static void *packet_radio_poll_thread(void *arg)
{
    uint8_t buf[512];

    (void)arg;

    while (g_radio_running) {
        if (g_tnc != NULL) {
            (void)hybbx_tnc_poll(g_tnc);
        }

        if (g_circuit_fd >= 0) {
            size_t read_len = 0;
            hybbx_result_t rc = hybbx_circuit_link_read(g_circuit_fd, buf,
                                                        sizeof(buf), &read_len);
            if (rc == HYBBX_OK && read_len > 0) {
                hybbx_circuit_decoder_feed(&g_circuit_dec, buf, read_len,
                                           on_circuit_downlink, NULL);
            }
        }

        packet_radio_poll_sleep_ms(20);
    }

    return NULL;
}

static hybbx_result_t packet_radio_connect_circuit(void)
{
    const char *host = g_active_config.circuit_host;
    unsigned port = g_active_config.circuit_port;
    unsigned attempt;

    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }
    if (port == 0) {
        port = HYBBX_CIRCUIT_DEFAULT_PORT;
    }

    for (attempt = 0; attempt < 50; attempt++) {
        hybbx_result_t rc = hybbx_circuit_link_connect(host, port,
                                                     &g_circuit_fd);
        if (rc == HYBBX_OK) {
            hybbx_circuit_decoder_init(&g_circuit_dec);
            printf("[packet_radio] linked to internal circuit %s:%u (HBX)\n",
                   host, port);
            return HYBBX_OK;
        }
        packet_radio_poll_sleep_ms(100);
    }

    fprintf(stderr,
            "[packet_radio] could not connect to internal circuit %s:%u\n",
            host, port);
    return HYBBX_ERR_IO;
}

static hybbx_result_t packet_radio_init(hybbx_service_t *service)
{
    g_service = service;
    g_tnc = NULL;
    g_circuit_fd = -1;
    g_radio_running = 0;
    return HYBBX_OK;
}

static void packet_radio_shutdown(void)
{
    g_radio_running = 0;
    if (g_poll_thread_started) {
        pthread_join(g_poll_thread, NULL);
        g_poll_thread_started = 0;
    }

    if (g_circuit_fd >= 0) {
        close(g_circuit_fd);
        g_circuit_fd = -1;
    }

    if (g_tnc != NULL) {
        hybbx_tnc_close(g_tnc);
        g_tnc = NULL;
    }

    hybbx_packet_radio_config_free(&g_active_config);
}

static hybbx_result_t packet_radio_start(const char *config)
{
    hybbx_result_t rc;
    hybbx_tnc_frame_cb frame_cb = on_tnc_ax25_frame;
    hybbx_tnc_ui_cb ui_cb = NULL;

    (void)g_service;

    packet_radio_shutdown();
    memset(&g_active_config, 0, sizeof(g_active_config));
    g_poll_thread = (pthread_t)0;

    rc = hybbx_packet_radio_config_parse(config, &g_active_config);
    if (rc != HYBBX_OK) {
        fprintf(stderr, "[packet_radio] invalid configuration\n");
        return rc;
    }

    printf("[packet_radio] tnc=%s protocol=%s device_type=%s device=%s "
           "band=%s duplex=%s modulation=%s circuit=%s:%u (HBX over internal TCP)\n",
           tnc_name(g_active_config.tnc),
           protocol_name(g_active_config.protocol),
           device_type_name(g_active_config.device_type),
           g_active_config.device,
           hybbx_packet_radio_band_name(g_active_config.params.band),
           hybbx_packet_radio_duplex_name(g_active_config.params.duplex),
           hybbx_packet_radio_modulation_name(g_active_config.params.modulation),
           g_active_config.circuit_host,
           g_active_config.circuit_port);

    rc = packet_radio_connect_circuit();
    if (rc != HYBBX_OK) {
        packet_radio_shutdown();
        return rc;
    }

    if (g_active_config.protocol == HYBBX_PACKET_RADIO_PROTO_HOSTMODE) {
        frame_cb = NULL;
        ui_cb = on_tnc_host_ui;
    }

    rc = hybbx_tnc_open(&g_tnc, &g_active_config, frame_cb, ui_cb, NULL);
    if (rc != HYBBX_OK) {
        packet_radio_shutdown();
        return rc;
    }

    g_radio_running = 1;
    if (pthread_create(&g_poll_thread, NULL, packet_radio_poll_thread,
                       NULL) != 0) {
        fprintf(stderr, "[packet_radio] poll thread failed\n");
        packet_radio_shutdown();
        return HYBBX_ERR_IO;
    }
    g_poll_thread_started = 1;

    return HYBBX_OK;
}

static hybbx_result_t packet_radio_stop(void)
{
    printf("[packet_radio] stop\n");
    packet_radio_shutdown();
    return HYBBX_OK;
}

const hybbx_transport_plugin_t hybbx_plugin_packet_radio = {
    .name = "packet_radio",
    .kind = HYBBX_TRANSPORT_PACKET_RADIO,
    .version = 1,
    .init = packet_radio_init,
    .shutdown = packet_radio_shutdown,
    .start = packet_radio_start,
    .stop = packet_radio_stop,
    .write = NULL,
};
