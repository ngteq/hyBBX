/*
 * baycom — BayCom PR-Stack link adapter (Linux kernel SER12/PAR96/EPP or KISS serial).
 * INI: [transport.baycom] or [transport.baycomN]. See docs/BAYCOM.md.
 */
#include "hybbx/plugin.h"
#include "hybbx/service.h"
#include "hybbx/baycom.h"
#include "hybbx/circuit.h"
#include "hybbx/circuit_tcp.h"
#include "hybbx/circuit_balance.h"
#include "hybbx/limits.h"
#include "hybbx/util.h"
#include "hybbx/log.h"

#include "baycom_modem.h"

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

typedef struct baycom_instance {
    hybbx_baycom_config_t config;
    baycom_modem_t *modem;
    int circuit_fd;
    hybbx_circuit_decoder_t circuit_dec;
    volatile int flow_paused;
    volatile int flow_cancel;
    unsigned index;
} baycom_instance_t;

static baycom_instance_t g_instances[HYBBX_BAYCOM_MAX_INSTANCES];
static unsigned g_instance_count;
static pthread_t g_poll_thread;
static volatile int g_running;
static int g_poll_thread_started;

static int instance_circuit_uplink_allowed(const baycom_instance_t *inst)
{
    return inst != NULL && !inst->flow_paused && !inst->flow_cancel;
}

static hybbx_result_t instance_circuit_uplink(baycom_instance_t *inst,
                                              const uint8_t *frame,
                                              size_t frame_len)
{
    uint8_t hbx[HYBBX_CIRCUIT_MAX_FRAME];
    size_t hbx_len;

    if (inst == NULL || inst->circuit_fd < 0 || frame_len == 0) {
        return HYBBX_ERR_IO;
    }

    if (!instance_circuit_uplink_allowed(inst)) {
        return HYBBX_OK;
    }

    hbx_len = hybbx_circuit_encode_ax25(frame, frame_len, HYBBX_CIRCUIT_FLAG_RX,
                                        hbx, sizeof(hbx));
    if (hbx_len == 0) {
        return HYBBX_ERR_IO;
    }

    return hybbx_circuit_link_write(inst->circuit_fd, hbx, hbx_len);
}

static void on_modem_ax25_frame(const uint8_t *frame, size_t len, void *userdata)
{
    baycom_instance_t *inst = (baycom_instance_t *)userdata;

    if (len == 0 || inst == NULL) {
        return;
    }

    (void)instance_circuit_uplink(inst, frame, len);
}

static void baycom_poll_sleep_ms(unsigned ms)
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

static hybbx_result_t instance_connect_circuit(baycom_instance_t *inst)
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

    for (attempt = 0; attempt < 50; attempt++) {
        hybbx_result_t rc = hybbx_circuit_link_connect(host, port,
                                                       &inst->circuit_fd);
        if (rc == HYBBX_OK) {
            hybbx_circuit_decoder_init(&inst->circuit_dec);
            if (inst->config.link_password != NULL &&
                inst->config.link_password[0] != '\0') {
                const char *link_id = inst->config.link_id;
                const char *link_role = inst->config.link_role;
                hybbx_circuit_link_qos_t qos;

                if (link_id == NULL || link_id[0] == '\0') {
                    link_id = "baycom-link";
                }
                if (link_role == NULL || link_role[0] == '\0') {
                    link_role = "link";
                }

                memset(&qos, 0, sizeof(qos));
                qos.baud = inst->config.radio_baud;
                qos.duplex = inst->config.full_duplex ? 2 : 1;
                qos.bandwidth = "low";
                qos.frequency_mhz = inst->config.frequency_mhz;

                rc = hybbx_circuit_link_authenticate_ex(
                    inst->circuit_fd, inst->config.link_password,
                    link_role, link_id, &qos);
                if (rc != HYBBX_OK) {
                    close(inst->circuit_fd);
                    inst->circuit_fd = -1;
                    baycom_poll_sleep_ms(100);
                    continue;
                }
            }
            hybbx_log_info("[baycom%u] linked to internal circuit %s:%u (HBX)",
                           inst->index + 1, host, port);
            return HYBBX_OK;
        }
        baycom_poll_sleep_ms(100);
    }

    hybbx_log_warn("[baycom%u] could not connect to internal circuit %s:%u",
                   inst->index + 1, host, port);
    return HYBBX_ERR_IO;
}

static void instance_on_circuit_flow_ctrl(baycom_instance_t *inst,
                                          const uint8_t *payload, size_t len)
{
    hybbx_circuit_balance_action_t action;
    char reason[64];

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
        hybbx_log_stats("[baycom%u] circuit flow pause (%s)", inst->index + 1, reason);
        break;
    case HYBBX_CIRCUIT_BAL_BREAK:
        inst->flow_paused = 0;
        hybbx_circuit_decoder_init(&inst->circuit_dec);
        hybbx_log_stats("[baycom%u] circuit flow break (%s)", inst->index + 1, reason);
        break;
    case HYBBX_CIRCUIT_BAL_CANCEL:
        inst->flow_cancel = 1;
        hybbx_log_warn("[baycom%u] circuit flow cancel (%s)",
                       inst->index + 1, reason);
        break;
    case HYBBX_CIRCUIT_BAL_RESUME:
        inst->flow_paused = 0;
        hybbx_log_stats("[baycom%u] circuit flow resume (%s)", inst->index + 1, reason);
        break;
    default:
        break;
    }
}

static void on_circuit_downlink(hybbx_circuit_proto_t proto, uint16_t flags,
                                const uint8_t *payload, size_t len,
                                void *userdata)
{
    baycom_instance_t *inst = (baycom_instance_t *)userdata;

    (void)flags;

    if (inst == NULL || inst->modem == NULL || len == 0 || inst->flow_paused) {
        return;
    }

    if (proto == HYBBX_CIRCUIT_PROTO_FLOW_CTRL) {
        instance_on_circuit_flow_ctrl(inst, payload, len);
        return;
    }

    if (proto == HYBBX_CIRCUIT_PROTO_AX25) {
        (void)baycom_modem_send_frame(inst->modem, payload, len);
    }
}

static void instance_shutdown(baycom_instance_t *inst)
{
    if (inst == NULL) {
        return;
    }

    if (inst->modem != NULL) {
        baycom_modem_close(inst->modem);
        inst->modem = NULL;
    }

    if (inst->circuit_fd >= 0) {
        close(inst->circuit_fd);
        inst->circuit_fd = -1;
    }

    hybbx_baycom_config_free(&inst->config);
    memset(inst, 0, sizeof(*inst));
    inst->circuit_fd = -1;
}

static void *baycom_poll_thread(void *arg)
{
    uint8_t buf[512];
    unsigned i;

    (void)arg;

    while (g_running) {
        for (i = 0; i < g_instance_count; i++) {
            baycom_instance_t *inst = &g_instances[i];

            if (inst->modem != NULL) {
                (void)baycom_modem_poll(inst->modem);
            }

            if (inst->circuit_fd >= 0) {
                size_t read_len = 0;
                hybbx_result_t rc = hybbx_circuit_link_read(inst->circuit_fd,
                                                            buf, sizeof(buf),
                                                            &read_len);
                if (rc == HYBBX_OK && read_len > 0) {
                    hybbx_circuit_decoder_feed(&inst->circuit_dec, buf,
                                               read_len, on_circuit_downlink,
                                               inst);
                }
            }

            if (inst->flow_cancel) {
                inst->flow_cancel = 0;
                inst->flow_paused = 0;
                if (inst->circuit_fd >= 0) {
                    close(inst->circuit_fd);
                    inst->circuit_fd = -1;
                }
                (void)instance_connect_circuit(inst);
            }
        }

        baycom_poll_sleep_ms(20);
    }

    return NULL;
}

static hybbx_result_t instance_start(baycom_instance_t *inst,
                                     const char *config,
                                     unsigned index)
{
    hybbx_result_t rc;

    if (inst == NULL || config == NULL) {
        return HYBBX_ERR_INVALID;
    }

    memset(inst, 0, sizeof(*inst));
    inst->circuit_fd = -1;
    inst->index = index;

    rc = hybbx_baycom_config_parse(config, &inst->config);
    if (rc != HYBBX_OK) {
        hybbx_log_warn("[baycom%u] invalid configuration", index + 1);
        instance_shutdown(inst);
        return rc;
    }

    {
        char msg[512];
        size_t pos = 0;

        pos += (size_t)snprintf(msg + pos, sizeof(msg) - pos,
                                "[baycom%u] backend=%s mode=%s",
                                index + 1,
                                hybbx_baycom_backend_name(inst->config.backend),
                                hybbx_baycom_modem_mode_name(inst->config.mode));

        if (inst->config.backend == HYBBX_BAYCOM_BACKEND_KERNEL) {
            pos += (size_t)snprintf(msg + pos, sizeof(msg) - pos,
                                    " interface=%s module=%s iobase=0x%x irq=%u radio_baud=%u",
                                    inst->config.interface != NULL ? inst->config.interface : "?",
                                    inst->config.kernel_module != NULL ? inst->config.kernel_module
                                                                       : "?",
                                    inst->config.iobase, inst->config.irq, inst->config.radio_baud);
        } else {
            pos += (size_t)snprintf(msg + pos, sizeof(msg) - pos,
                                    " device=%s serial_baud=%u",
                                    inst->config.device != NULL ? inst->config.device : "?",
                                    inst->config.serial_baud);
        }

        pos += (size_t)snprintf(msg + pos, sizeof(msg) - pos,
                                " circuit=%s:%u (HBX over internal TCP)",
                                inst->config.circuit_host, inst->config.circuit_port);
        hybbx_log_info("%s", msg);
    }

    if (inst->config.frequency_mhz != NULL &&
        inst->config.frequency_mhz[0] != '\0') {
        hybbx_log_info("[baycom%u] frequency_mhz=%s", index + 1,
                       inst->config.frequency_mhz);
    }

    rc = instance_connect_circuit(inst);
    if (rc != HYBBX_OK) {
        instance_shutdown(inst);
        return rc;
    }

    rc = baycom_modem_open(&inst->modem, &inst->config, on_modem_ax25_frame,
                           inst);
    if (rc != HYBBX_OK) {
        hybbx_log_warn("[baycom%u] modem open failed (%s)", index + 1,
                       hybbx_result_name(rc));
        instance_shutdown(inst);
        return rc;
    }

    return HYBBX_OK;
}

static hybbx_result_t baycom_init(hybbx_service_t *service)
{
    (void)service;
    g_instance_count = 0;
    g_running = 0;
    return HYBBX_OK;
}

static void baycom_shutdown(void)
{
    unsigned i;

    g_running = 0;
    if (g_poll_thread_started) {
        pthread_join(g_poll_thread, NULL);
        g_poll_thread_started = 0;
    }

    for (i = 0; i < g_instance_count; i++) {
        instance_shutdown(&g_instances[i]);
    }
    g_instance_count = 0;
}

static hybbx_result_t baycom_start(const char *config)
{
    const char *cursor = config;
    hybbx_result_t rc = HYBBX_OK;
    unsigned started = 0;

    if (config == NULL) {
        return HYBBX_ERR_INVALID;
    }

    baycom_shutdown();
    g_poll_thread = (pthread_t)0;

    while (*cursor != '\0' && g_instance_count < HYBBX_BAYCOM_MAX_INSTANCES) {
        char scratch[4096];
        const char *sep = strchr(cursor, HYBBX_BAYCOM_INSTANCE_SEP);
        size_t chunk_len;

        if (sep != NULL) {
            chunk_len = (size_t)(sep - cursor);
        } else {
            chunk_len = strlen(cursor);
        }

        if (chunk_len >= sizeof(scratch)) {
            return HYBBX_ERR_INVALID;
        }

        memcpy(scratch, cursor, chunk_len);
        scratch[chunk_len] = '\0';

        rc = instance_start(&g_instances[g_instance_count], scratch,
                            g_instance_count);
        if (rc == HYBBX_OK) {
            g_instance_count++;
            started++;
        }

        if (sep == NULL) {
            break;
        }
        cursor = sep + 1;
    }

    if (started == 0) {
        return rc != HYBBX_OK ? rc : HYBBX_ERR_INVALID;
    }

    g_running = 1;
    if (pthread_create(&g_poll_thread, NULL, baycom_poll_thread, NULL) != 0) {
        baycom_shutdown();
        return HYBBX_ERR_IO;
    }
    g_poll_thread_started = 1;

    return HYBBX_OK;
}

static hybbx_result_t baycom_stop(void)
{
    baycom_shutdown();
    return HYBBX_OK;
}

const hybbx_transport_plugin_t hybbx_plugin_baycom = {
    .name = "baycom",
    .kind = HYBBX_TRANSPORT_BAYCOM,
    .version = 1,
    .init = baycom_init,
    .shutdown = baycom_shutdown,
    .start = baycom_start,
    .stop = baycom_stop,
    .write = NULL,
};
