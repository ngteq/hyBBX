/*
 * packet_radio — link/repeater edge adapter: serial TNC, KISS/AX.25 → HBX client.
 * INI: [transport.packet_radio] or [transport.packet_radioN]. See share/hybbx.ini.example.
 */
#include "hybbx/plugin.h"
#include "hybbx/service.h"
#include "hybbx/packet_radio.h"
#include "hybbx/tnc.h"
#include "hybbx/circuit.h"
#include "hybbx/circuit_tcp.h"
#include "hybbx/circuit_balance.h"
#include "hybbx/traffic.h"
#include "hybbx/limits.h"
#include "hybbx/security_ban.h"
#include "hybbx/ax25.h"
#include "hybbx/util.h"
#include "hybbx/log.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <poll.h>
#include <sys/select.h>
#endif

#define PACKET_RADIO_POLL_MS 20u

static hybbx_service_t *g_service;

typedef struct packet_radio_instance {
    hybbx_packet_radio_config_t config;
    hybbx_tnc_t *tnc;
    int circuit_fd;
    hybbx_circuit_decoder_t circuit_dec;
    volatile int flow_paused;
    volatile int flow_cancel;
    unsigned index;
    unsigned circuit_reconnect_polls;
} packet_radio_instance_t;

static packet_radio_instance_t g_instances[HYBBX_PACKET_RADIO_MAX_INSTANCES];
static unsigned g_instance_count;
static pthread_t g_poll_thread;
static volatile int g_radio_running;
static int g_poll_thread_started;

extern const hybbx_transport_plugin_t hybbx_plugin_packet_radio;

static hybbx_result_t instance_connect_circuit(packet_radio_instance_t *inst);

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

static int instance_circuit_uplink_allowed(const packet_radio_instance_t *inst)
{
    return inst != NULL && !inst->flow_paused && !inst->flow_cancel;
}

static int packet_radio_source_banned(const hybbx_ax25_path_t *path)
{
    char raw[HYBBX_CALLID_MAX];
    char norm[HYBBX_CALLID_MAX];

    if (path == NULL || path->source.call[0] == '\0') {
        return 0;
    }

    if (path->source.ssid > 0u) {
        snprintf(raw, sizeof(raw), "%s-%u", path->source.call,
                 path->source.ssid);
    } else {
        hybbx_strlcpy(raw, path->source.call, sizeof(raw));
    }

    if (!hybbx_security_callid_normalize(raw, norm, sizeof(norm))) {
        return 0;
    }

    return hybbx_security_ban_callid_is_banned(norm);
}

static int packet_radio_frame_source_banned(const uint8_t *frame, size_t len)
{
    hybbx_ax25_path_t path;
    uint8_t scratch[HYBBX_AX25_FRAME_MAX];

    if (frame == NULL || len == 0) {
        return 0;
    }

    if (hybbx_ax25_parse_ui(frame, len, &path, scratch, sizeof(scratch)) == 0) {
        return 0;
    }

    return packet_radio_source_banned(&path);
}

static hybbx_result_t instance_circuit_uplink(packet_radio_instance_t *inst,
                                              const uint8_t *frame,
                                              size_t frame_len)
{
    uint8_t hbx[HYBBX_CIRCUIT_MAX_FRAME];
    size_t hbx_len;
    uint16_t flags = HYBBX_CIRCUIT_FLAG_RX;

    if (inst == NULL || inst->circuit_fd < 0) {
        return HYBBX_ERR_IO;
    }

    if (!instance_circuit_uplink_allowed(inst)) {
        return HYBBX_OK;
    }

    if (hybbx_packet_radio_modulation_is_g3ruh(inst->config.params.modulation)) {
        flags |= HYBBX_CIRCUIT_FLAG_G3RUH_FSK;
    }

    hbx_len = hybbx_circuit_encode_ax25(frame, frame_len, flags,
                                        hbx, sizeof(hbx));
    if (hbx_len == 0) {
        return HYBBX_ERR_IO;
    }

    return hybbx_circuit_link_write(inst->circuit_fd, hbx, hbx_len);
}

static void on_tnc_ax25_frame(const uint8_t *frame, size_t len, void *userdata)
{
    packet_radio_instance_t *inst = (packet_radio_instance_t *)userdata;

    if (len == 0 || inst == NULL) {
        return;
    }

    if (packet_radio_frame_source_banned(frame, len)) {
        return;
    }

    (void)instance_circuit_uplink(inst, frame, len);
}

static void on_tnc_host_ui(const uint8_t *payload, size_t len,
                           const hybbx_ax25_path_t *path, void *userdata)
{
    packet_radio_instance_t *inst = (packet_radio_instance_t *)userdata;
    uint8_t hbx[HYBBX_CIRCUIT_MAX_FRAME];
    size_t hbx_len;

    (void)path;

    if (inst == NULL || inst->circuit_fd < 0 || len == 0) {
        return;
    }

    if (path != NULL && packet_radio_source_banned(path)) {
        return;
    }

    if (!instance_circuit_uplink_allowed(inst)) {
        return;
    }

    hbx_len = hybbx_circuit_encode(HYBBX_CIRCUIT_PROTO_TERMINAL,
                                   HYBBX_CIRCUIT_FLAG_RX,
                                   payload, len, hbx, sizeof(hbx));
    if (hbx_len > 0) {
        (void)hybbx_circuit_link_write(inst->circuit_fd, hbx, hbx_len);
    }
}

static void instance_format_call(const hybbx_ax25_address_t *addr,
                                 char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return;
    }

    out[0] = '\0';
    if (addr == NULL || addr->call[0] == '\0') {
        return;
    }

    if (addr->ssid > 0u) {
        snprintf(out, out_len, "%s-%u", addr->call, addr->ssid);
    } else {
        hybbx_strlcpy(out, addr->call, out_len);
    }
}

static void instance_log_rf_tx(packet_radio_instance_t *inst,
                             hybbx_result_t rc,
                             const hybbx_ax25_path_t *path,
                             size_t payload_len)
{
    char src[HYBBX_AX25_CALL_MAX + 8];
    char dst[HYBBX_AX25_CALL_MAX + 8];

    if (inst == NULL) {
        return;
    }

    instance_format_call(path != NULL ? &path->source : NULL, src, sizeof(src));
    instance_format_call(path != NULL ? &path->dest : NULL, dst, sizeof(dst));

    if (rc == HYBBX_OK) {
        hybbx_log_stats("[packet_radio%u] RF TX %s>%s (%zu bytes)",
                        inst->index + 1, src, dst, payload_len);
    } else {
        hybbx_log_warn("[packet_radio%u] RF TX failed %s>%s (%zu bytes, rc=%d)",
                      inst->index + 1, src, dst, payload_len, (int)rc);
    }
}

static hybbx_result_t instance_tx_ax25_ui(packet_radio_instance_t *inst,
                                          const hybbx_ax25_path_t *path,
                                          const uint8_t *payload, size_t len)
{
    uint8_t frame[HYBBX_AX25_FRAME_MAX];
    size_t frame_len;
    hybbx_result_t rc;

    if (inst == NULL || inst->tnc == NULL || path == NULL ||
        payload == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    frame_len = hybbx_ax25_build_ui(path, payload, len, frame, sizeof(frame));
    if (frame_len == 0) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_tnc_send_frame(inst->tnc, frame, frame_len);
    instance_log_rf_tx(inst, rc, path, len);
    return rc;
}

static hybbx_result_t instance_send_bytes(packet_radio_instance_t *inst,
                                          const uint8_t *data, size_t len)
{
    if (inst == NULL || inst->tnc == NULL || data == NULL || len == 0) {
        return HYBBX_ERR_INVALID;
    }

    if (inst->config.protocol == HYBBX_PACKET_RADIO_PROTO_HOSTMODE) {
        if (hybbx_tnc_host_connected(inst->tnc)) {
            return hybbx_tnc_send_converse(inst->tnc, (const char *)data, len);
        }
        return HYBBX_ERR_BUSY;
    }

    return hybbx_tnc_send_ui(inst->tnc, data, len);
}

static void instance_circuit_disconnect(packet_radio_instance_t *inst)
{
    if (inst == NULL) {
        return;
    }

    if (inst->circuit_fd >= 0) {
        close(inst->circuit_fd);
        inst->circuit_fd = -1;
    }
    hybbx_circuit_decoder_init(&inst->circuit_dec);
    inst->circuit_reconnect_polls = 0;
}

static void instance_circuit_reconnect(packet_radio_instance_t *inst)
{
    if (inst == NULL) {
        return;
    }

    instance_circuit_disconnect(inst);
    if (instance_connect_circuit(inst) == HYBBX_OK) {
        hybbx_log_info("[packet_radio%u] circuit reconnected", inst->index + 1);
    }
}

static void instance_on_circuit_flow_ctrl(packet_radio_instance_t *inst,
                                          hybbx_circuit_proto_t proto,
                                          uint16_t flags,
                                          const uint8_t *payload, size_t len)
{
    hybbx_circuit_balance_action_t action;
    char reason[64];

    (void)proto;
    (void)flags;

    if (inst == NULL) {
        return;
    }

    if (hybbx_circuit_flow_ctrl_parse((const char *)payload, len, &action,
                                      reason, sizeof(reason)) != HYBBX_OK) {
        return;
    }

    switch (action) {
    case HYBBX_CIRCUIT_BAL_PAUSE:
        inst->flow_paused = 1;
        hybbx_log_stats("[packet_radio%u] circuit flow pause (%s)",
                        inst->index + 1, reason);
        break;
    case HYBBX_CIRCUIT_BAL_BREAK:
        inst->flow_paused = 0;
        hybbx_circuit_decoder_init(&inst->circuit_dec);
        hybbx_log_stats("[packet_radio%u] circuit flow break (%s)",
                        inst->index + 1, reason);
        break;
    case HYBBX_CIRCUIT_BAL_CANCEL:
        inst->flow_cancel = 1;
        hybbx_log_warn("[packet_radio%u] circuit flow cancel (%s)",
                       inst->index + 1, reason);
        break;
    case HYBBX_CIRCUIT_BAL_RESUME:
        inst->flow_paused = 0;
        hybbx_log_stats("[packet_radio%u] circuit flow resume (%s)",
                        inst->index + 1, reason);
        break;
    default:
        break;
    }
}

static void on_circuit_downlink(hybbx_circuit_proto_t proto, uint16_t flags,
                                const uint8_t *payload, size_t len,
                                void *userdata)
{
    packet_radio_instance_t *inst = (packet_radio_instance_t *)userdata;

    if (inst == NULL) {
        return;
    }

    if (proto == HYBBX_CIRCUIT_PROTO_FLOW_CTRL) {
        instance_on_circuit_flow_ctrl(inst, proto, flags, payload, len);
        return;
    }

    if (inst->tnc == NULL || len == 0) {
        return;
    }

    switch (proto) {
    case HYBBX_CIRCUIT_PROTO_TERMINAL:
        (void)instance_send_bytes(inst, payload, len);
        break;
    case HYBBX_CIRCUIT_PROTO_AX25: {
        hybbx_result_t rc = hybbx_tnc_send_frame(inst->tnc, payload, len);
        if (rc != HYBBX_OK) {
            hybbx_log_warn("[packet_radio%u] RF TX frame failed (rc=%d, %zu bytes)",
                           inst->index + 1, (int)rc, len);
        }
        break;
    }
    case HYBBX_CIRCUIT_PROTO_AX25_UI: {
        uint8_t ui[HYBBX_AX25_PAYLOAD_MAX];
        hybbx_ax25_path_t path;
        size_t ui_len = hybbx_circuit_unpack_ax25_ui(payload, len, &path,
                                                     ui, sizeof(ui));
        if (ui_len > 0) {
            (void)instance_tx_ax25_ui(inst, &path, ui, ui_len);
        } else {
            hybbx_log_warn("[packet_radio%u] AX25_UI unpack failed (%zu bytes)",
                           inst->index + 1, len);
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

static hybbx_result_t instance_connect_circuit(packet_radio_instance_t *inst)
{
    const char *host;
    unsigned port;
    unsigned attempt;

    if (inst == NULL) {
        return HYBBX_ERR_INVALID;
    }

    host = inst->config.circuit_host;
    port = inst->config.circuit_port;

    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }
    if (port == 0) {
        port = HYBBX_CIRCUIT_DEFAULT_PORT;
    }

    if (inst->config.link_password == NULL ||
        inst->config.link_password[0] == '\0') {
        hybbx_log_warn("[packet_radio%u] missing link_password for HBX LINK_AUTH; "
                       "set link_password in [transport.packet_radioN] "
                       "(hub logs link_auth_fail reason=timeout_no_link_auth)",
                       inst->index + 1);
        return HYBBX_ERR_INVALID;
    }

    for (attempt = 0; attempt < 50; attempt++) {
        hybbx_result_t rc = hybbx_circuit_link_connect(host, port,
                                                     &inst->circuit_fd);
        if (rc == HYBBX_OK) {
            hybbx_circuit_decoder_init(&inst->circuit_dec);
            {
                const char *link_id = inst->config.link_id;
                const char *link_role = inst->config.link_role;

                if (link_id == NULL || link_id[0] == '\0') {
                    link_id = "packet-radio";
                }
                if (link_role == NULL || link_role[0] == '\0') {
                    link_role = "link";
                }

                {
                    hybbx_circuit_link_qos_t qos;
                    int duplex = (int)inst->config.params.duplex;

                    memset(&qos, 0, sizeof(qos));
                    qos.baud = inst->config.params.radio_baud;
                    if (duplex == HYBBX_PACKET_RADIO_DUPLEX_HALF) {
                        qos.duplex = 1;
                    } else if (duplex == HYBBX_PACKET_RADIO_DUPLEX_FULL) {
                        qos.duplex = 2;
                    }
                    qos.bandwidth = "low";
                    qos.frequency_mhz = inst->config.frequency_mhz;

                    rc = hybbx_circuit_link_authenticate_ex(
                        inst->circuit_fd, inst->config.link_password,
                        link_role, link_id, &qos);
                }
                if (rc != HYBBX_OK) {
                    close(inst->circuit_fd);
                    inst->circuit_fd = -1;
                    packet_radio_poll_sleep_ms(100);
                    continue;
                }
            }
            hybbx_log_info("[packet_radio%u] linked to internal circuit %s:%u (HBX)",
                           inst->index + 1, host, port);
            return HYBBX_OK;
        }
        packet_radio_poll_sleep_ms(100);
    }

    hybbx_log_warn("[packet_radio%u] could not connect to internal circuit %s:%u",
                   inst->index + 1, host, port);
    return HYBBX_ERR_IO;
}

static void instance_shutdown(packet_radio_instance_t *inst)
{
    if (inst == NULL) {
        return;
    }

    if (inst->circuit_fd >= 0) {
        close(inst->circuit_fd);
        inst->circuit_fd = -1;
    }

    if (inst->tnc != NULL) {
        hybbx_tnc_close(inst->tnc);
        inst->tnc = NULL;
    }

    hybbx_packet_radio_config_free(&inst->config);
    memset(inst, 0, sizeof(*inst));
    inst->circuit_fd = -1;
}

typedef enum {
    PR_POLL_CIRCUIT = 0,
    PR_POLL_SERIAL = 1
} packet_radio_poll_kind_t;

typedef struct {
    unsigned instance_index;
    packet_radio_poll_kind_t kind;
} packet_radio_poll_map_t;

static void instance_drain_circuit(packet_radio_instance_t *inst)
{
    uint8_t buf[512];

    if (inst == NULL || inst->circuit_fd < 0) {
        return;
    }

    for (;;) {
        size_t read_len = 0;
        hybbx_result_t rc = hybbx_circuit_link_read(inst->circuit_fd, buf,
                                                    sizeof(buf), &read_len);

        if (rc == HYBBX_ERR_IO) {
            hybbx_log_warn("[packet_radio%u] circuit disconnected — reconnecting",
                           inst->index + 1);
            instance_circuit_reconnect(inst);
            return;
        }
        if (rc != HYBBX_OK || read_len == 0) {
            break;
        }

        hybbx_circuit_decoder_feed(&inst->circuit_dec, buf, read_len,
                                   on_circuit_downlink, inst);
    }
}

#if !defined(_WIN32)
static void packet_radio_poll_instances(void)
{
    struct pollfd pfds[HYBBX_PACKET_RADIO_MAX_INSTANCES * 2u];
    packet_radio_poll_map_t map[HYBBX_PACKET_RADIO_MAX_INSTANCES * 2u];
    nfds_t nfds = 0;
    unsigned i;

    for (i = 0; i < g_instance_count; i++) {
        packet_radio_instance_t *inst = &g_instances[i];
        int circuit_fd = inst->circuit_fd;
        int serial_fd = inst->tnc != NULL ? hybbx_tnc_serial_fd(inst->tnc) : -1;

        if (circuit_fd >= 0) {
            pfds[nfds].fd = circuit_fd;
            pfds[nfds].events = POLLIN;
            pfds[nfds].revents = 0;
            map[nfds].instance_index = i;
            map[nfds].kind = PR_POLL_CIRCUIT;
            nfds++;
        }

        if (serial_fd >= 0) {
            pfds[nfds].fd = serial_fd;
            pfds[nfds].events = POLLIN;
            pfds[nfds].revents = 0;
            map[nfds].instance_index = i;
            map[nfds].kind = PR_POLL_SERIAL;
            nfds++;
        }
    }

    if (nfds > 0) {
        int pr = poll(pfds, nfds, (int)PACKET_RADIO_POLL_MS);

        if (pr < 0) {
            if (errno != EINTR) {
                hybbx_log_warn("[packet_radio] poll failed");
            }
        } else if (pr > 0) {
            nfds_t j;

            for (j = 0; j < nfds; j++) {
                short rev = pfds[j].revents;

                if (rev == 0) {
                    continue;
                }

                if ((rev & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                    if (map[j].kind == PR_POLL_CIRCUIT) {
                        packet_radio_instance_t *inst =
                            &g_instances[map[j].instance_index];

                        hybbx_log_warn("[packet_radio%u] circuit hangup — "
                                       "reconnecting",
                                       inst->index + 1);
                        instance_circuit_reconnect(inst);
                    }
                    continue;
                }

                if ((rev & POLLIN) == 0) {
                    continue;
                }

                if (map[j].kind == PR_POLL_CIRCUIT) {
                    instance_drain_circuit(&g_instances[map[j].instance_index]);
                } else {
                    packet_radio_instance_t *inst =
                        &g_instances[map[j].instance_index];

                    if (inst->tnc != NULL) {
                        (void)hybbx_tnc_poll(inst->tnc);
                    }
                }
            }
        }
    } else {
        packet_radio_poll_sleep_ms(PACKET_RADIO_POLL_MS);
    }
}
#endif

static void packet_radio_service_maintenance(void)
{
    unsigned i;

    for (i = 0; i < g_instance_count; i++) {
        packet_radio_instance_t *inst = &g_instances[i];

        if (inst->circuit_fd < 0) {
            inst->circuit_reconnect_polls++;
            if (inst->circuit_reconnect_polls >= 50u) {
                inst->circuit_reconnect_polls = 0;
                (void)instance_connect_circuit(inst);
            }
        }

        if (inst->flow_cancel) {
            inst->flow_cancel = 0;
            inst->flow_paused = 0;
            instance_circuit_reconnect(inst);
        }
    }
}

#if defined(_WIN32)
static void packet_radio_poll_win32(void)
{
    unsigned i;

    for (i = 0; i < g_instance_count; i++) {
        packet_radio_instance_t *inst = &g_instances[i];

        if (inst->tnc != NULL) {
            (void)hybbx_tnc_poll(inst->tnc);
        }

        if (inst->circuit_fd >= 0) {
            instance_drain_circuit(inst);
        }
    }
}
#endif

static void *packet_radio_poll_thread(void *arg)
{
    (void)arg;

    while (g_radio_running) {
#if !defined(_WIN32)
        packet_radio_poll_instances();
#else
        packet_radio_poll_win32();
        packet_radio_poll_sleep_ms(PACKET_RADIO_POLL_MS);
#endif
        packet_radio_service_maintenance();
    }

    return NULL;
}

static hybbx_result_t instance_start(packet_radio_instance_t *inst,
                                     const char *config,
                                     unsigned index)
{
    hybbx_result_t rc;
    hybbx_tnc_frame_cb frame_cb = on_tnc_ax25_frame;
    hybbx_tnc_ui_cb ui_cb = NULL;

    if (inst == NULL || config == NULL) {
        return HYBBX_ERR_INVALID;
    }

    memset(inst, 0, sizeof(*inst));
    inst->circuit_fd = -1;
    inst->index = index;

    rc = hybbx_packet_radio_config_parse(config, &inst->config);
    if (rc != HYBBX_OK) {
        hybbx_log_warn("[packet_radio%u] invalid configuration", index + 1);
        instance_shutdown(inst);
        return rc;
    }

    hybbx_log_info("[packet_radio%u] tnc=%s protocol=%s device_type=%s device=%s "
                   "band=%s duplex=%s modulation=%s circuit=%s:%u "
                   "(HBX over internal TCP)",
                   index + 1,
                   tnc_name(inst->config.tnc),
                   protocol_name(inst->config.protocol),
                   device_type_name(inst->config.device_type),
                   inst->config.device,
                   hybbx_packet_radio_band_name(inst->config.params.band),
                   hybbx_packet_radio_duplex_name(inst->config.params.duplex),
                   hybbx_packet_radio_modulation_name(inst->config.params.modulation),
                   inst->config.circuit_host,
                   inst->config.circuit_port);
    if (inst->config.frequency_mhz != NULL &&
        inst->config.frequency_mhz[0] != '\0') {
        hybbx_log_info("[packet_radio%u] frequency_mhz=%s", index + 1,
                       inst->config.frequency_mhz);
    }

    rc = instance_connect_circuit(inst);
    if (rc != HYBBX_OK) {
        instance_shutdown(inst);
        return rc;
    }

    if (inst->config.protocol == HYBBX_PACKET_RADIO_PROTO_HOSTMODE) {
        frame_cb = NULL;
        ui_cb = on_tnc_host_ui;
    }

    rc = hybbx_tnc_open(&inst->tnc, &inst->config, frame_cb, ui_cb, inst);
    if (rc != HYBBX_OK) {
        instance_shutdown(inst);
        return rc;
    }

    return HYBBX_OK;
}

static hybbx_result_t packet_radio_init(hybbx_service_t *service)
{
    g_service = service;
    g_instance_count = 0;
    g_radio_running = 0;
    return HYBBX_OK;
}

static void packet_radio_shutdown(void)
{
    unsigned i;

    g_radio_running = 0;
    if (g_poll_thread_started) {
        pthread_join(g_poll_thread, NULL);
        g_poll_thread_started = 0;
    }

    for (i = 0; i < g_instance_count; i++) {
        instance_shutdown(&g_instances[i]);
    }
    g_instance_count = 0;
}

static hybbx_result_t packet_radio_start(const char *config)
{
    char scratch[4096];
    const char *cursor = config;
    hybbx_result_t rc = HYBBX_OK;
    unsigned started = 0;
    unsigned section_num = 0;

    (void)g_service;

    if (config == NULL) {
        return HYBBX_ERR_INVALID;
    }

    packet_radio_shutdown();
    g_poll_thread = (pthread_t)0;

    while (*cursor != '\0' && g_instance_count < HYBBX_PACKET_RADIO_MAX_INSTANCES) {
        const char *sep = strchr(cursor, HYBBX_PACKET_RADIO_INSTANCE_SEP);
        size_t len = sep != NULL ? (size_t)(sep - cursor) : strlen(cursor);

        if (len == 0) {
            cursor = sep != NULL ? sep + 1 : cursor + len;
            if (sep == NULL) {
                break;
            }
            continue;
        }

        if (len >= sizeof(scratch)) {
            packet_radio_shutdown();
            return HYBBX_ERR_INVALID;
        }

        memcpy(scratch, cursor, len);
        scratch[len] = '\0';
        section_num++;

        if (!hybbx_packet_radio_section_is_local_edge(scratch)) {
            hybbx_log_info("[packet_radio%u] bridge registry only — no local TNC",
                           section_num);
            cursor = sep != NULL ? sep + 1 : cursor + len;
            if (sep == NULL) {
                break;
            }
            continue;
        }

        rc = instance_start(&g_instances[g_instance_count], scratch,
                            g_instance_count);
        if (rc != HYBBX_OK) {
            packet_radio_shutdown();
            return rc;
        }

        g_instance_count++;
        started++;
        cursor = sep != NULL ? sep + 1 : cursor + len;
        if (sep == NULL) {
            break;
        }
    }

    if (started == 0) {
        return HYBBX_OK;
    }

    g_radio_running = 1;
    if (pthread_create(&g_poll_thread, NULL, packet_radio_poll_thread,
                       NULL) != 0) {
        hybbx_log_warn("[packet_radio] poll thread failed");
        packet_radio_shutdown();
        return HYBBX_ERR_IO;
    }
    g_poll_thread_started = 1;

    hybbx_log_info("[packet_radio] %u TNC instance(s) active", started);
    return HYBBX_OK;
}

static hybbx_result_t packet_radio_stop(void)
{
    hybbx_log_info("[packet_radio] stop");
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
