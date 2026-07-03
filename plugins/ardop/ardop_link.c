#include "ardop_link.h"
#include "ardop_host.h"

#include "hybbx/ardop.h"
#include "hybbx/circuit.h"
#include "hybbx/circuit_tcp.h"
#include "hybbx/circuit_balance.h"
#include "hybbx/crdop.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/select.h>
#endif

static const ardop_link_plugin_t *g_plugin;
static hybbx_ardop_config_t g_active_config;
static ardop_host_t *g_host;
static int g_circuit_fd = -1;
static hybbx_circuit_decoder_t g_circuit_dec;
static pthread_t g_poll_thread;
static volatile int g_running;
static int g_poll_thread_started;
static volatile int g_circuit_flow_paused;
static volatile int g_circuit_flow_cancel;
static unsigned g_modem_reconnect_backoff_ms;

static const char *link_tag(void)
{
    return g_plugin != NULL && g_plugin->name != NULL ? g_plugin->name : "ardop";
}

static int circuit_uplink_allowed(void)
{
    return !g_circuit_flow_paused && !g_circuit_flow_cancel;
}

static hybbx_result_t circuit_uplink_terminal(const uint8_t *payload, size_t len)
{
    uint8_t hbx[HYBBX_CIRCUIT_MAX_FRAME];
    size_t hbx_len;

    if (g_circuit_fd < 0 || len == 0) {
        return HYBBX_ERR_IO;
    }

    if (!circuit_uplink_allowed()) {
        return HYBBX_OK;
    }

    hbx_len = hybbx_circuit_encode(HYBBX_CIRCUIT_PROTO_TERMINAL,
                                   HYBBX_CIRCUIT_FLAG_RX,
                                   payload, len, hbx, sizeof(hbx));
    if (hbx_len == 0) {
        return HYBBX_ERR_IO;
    }

    return hybbx_circuit_link_write(g_circuit_fd, hbx, hbx_len);
}

static void on_modem_data(const uint8_t *data, size_t len, void *userdata)
{
    (void)userdata;

    if (len == 0 || g_circuit_fd < 0 || !circuit_uplink_allowed()) {
        return;
    }

    (void)circuit_uplink_terminal(data, len);
}

static void on_modem_event(const char *event, const char *detail, void *userdata)
{
    (void)userdata;

    if (event == NULL) {
        return;
    }

    if (strcmp(event, "connected") == 0) {
        if (detail != NULL && detail[0] != '\0') {
            printf("[%s] RF link up (%s)\n", link_tag(), detail);
        } else {
            printf("[%s] RF link up\n", link_tag());
        }
        g_modem_reconnect_backoff_ms = 1000u;
        return;
    }

    if (strcmp(event, "disconnected") == 0) {
        printf("[%s] RF link down\n", link_tag());
    }
}

static void on_circuit_flow_ctrl(hybbx_circuit_proto_t proto, uint16_t flags,
                                 const uint8_t *payload, size_t len,
                                 void *userdata)
{
    hybbx_circuit_balance_action_t action;
    char reason[64];

    (void)proto;
    (void)flags;
    (void)userdata;

    if (hybbx_circuit_flow_ctrl_parse((const char *)payload, len, &action,
                                      reason, sizeof(reason)) != HYBBX_OK) {
        return;
    }

    switch (action) {
    case HYBBX_CIRCUIT_BAL_PAUSE:
        g_circuit_flow_paused = 1;
        printf("[%s] circuit flow pause (%s)\n", link_tag(), reason);
        break;
    case HYBBX_CIRCUIT_BAL_BREAK:
        g_circuit_flow_paused = 0;
        hybbx_circuit_decoder_init(&g_circuit_dec);
        printf("[%s] circuit flow break (%s)\n", link_tag(), reason);
        break;
    case HYBBX_CIRCUIT_BAL_CANCEL:
        g_circuit_flow_cancel = 1;
        fprintf(stderr, "[%s] circuit flow cancel (%s)\n", link_tag(), reason);
        break;
    case HYBBX_CIRCUIT_BAL_RESUME:
        g_circuit_flow_paused = 0;
        printf("[%s] circuit flow resume (%s)\n", link_tag(), reason);
        break;
    default:
        break;
    }
}

static void on_circuit_downlink(hybbx_circuit_proto_t proto, uint16_t flags,
                                const uint8_t *payload, size_t len,
                                void *userdata)
{
    (void)flags;
    (void)userdata;

    if (proto == HYBBX_CIRCUIT_PROTO_FLOW_CTRL) {
        on_circuit_flow_ctrl(proto, flags, payload, len, userdata);
        return;
    }

    if (g_host == NULL || len == 0 || g_circuit_flow_paused) {
        return;
    }

    if (proto != HYBBX_CIRCUIT_PROTO_TERMINAL) {
        return;
    }

    if (!ardop_host_link_up(g_host)) {
        return;
    }

    (void)ardop_host_send_data(g_host, payload, len);
}

static void link_poll_sleep_ms(unsigned ms)
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

static hybbx_result_t link_connect_circuit(void);

static void link_maybe_reconnect_modem(void)
{
    static unsigned ticks;

    if (g_host == NULL) {
        return;
    }

    if (ardop_host_modem_connected(g_host)) {
        ticks = 0;
        return;
    }

    ticks++;
    if (ticks < (g_modem_reconnect_backoff_ms / 20u)) {
        return;
    }

    ticks = 0;
    if (ardop_host_connect(g_host) != HYBBX_OK) {
        if (g_modem_reconnect_backoff_ms < 30000u) {
            g_modem_reconnect_backoff_ms += 1000u;
        }
    }
}

static void *link_poll_thread(void *arg)
{
    uint8_t buf[512];

    (void)arg;

    while (g_running) {
        if (g_host != NULL) {
            ardop_host_poll(g_host);
            link_maybe_reconnect_modem();
        }

        if (g_circuit_fd >= 0) {
            size_t read_len = 0;
            hybbx_result_t rc = hybbx_circuit_link_read(g_circuit_fd, buf,
                                                        sizeof(buf), &read_len);
            if (rc == HYBBX_OK && read_len > 0) {
                hybbx_circuit_decoder_feed(&g_circuit_dec, buf, read_len,
                                           on_circuit_downlink, NULL);
            }
        } else {
            static unsigned circuit_ticks;

            circuit_ticks++;
            if (circuit_ticks >= 50u) {
                circuit_ticks = 0;
                (void)link_connect_circuit();
            }
        }

        if (g_circuit_flow_cancel) {
            g_circuit_flow_cancel = 0;
            g_circuit_flow_paused = 0;
            if (g_circuit_fd >= 0) {
                close(g_circuit_fd);
                g_circuit_fd = -1;
            }
            (void)link_connect_circuit();
        }

        link_poll_sleep_ms(20);
    }

    return NULL;
}

static hybbx_result_t link_connect_circuit(void)
{
    const char *host = g_active_config.circuit_host;
    unsigned port = g_active_config.circuit_port;
    const char *link_id = g_active_config.link_id;
    unsigned attempt;

    if (host == NULL || host[0] == '\0') {
        host = "127.0.0.1";
    }
    if (port == 0) {
        port = HYBBX_CIRCUIT_DEFAULT_PORT;
    }
    if (link_id == NULL || link_id[0] == '\0') {
        link_id = g_plugin != NULL && g_plugin->default_link_id != NULL ?
                  g_plugin->default_link_id : "ardop-link";
    }

    for (attempt = 0; attempt < 50; attempt++) {
        hybbx_result_t rc = hybbx_circuit_link_connect(host, port,
                                                     &g_circuit_fd);
        if (rc == HYBBX_OK) {
            hybbx_circuit_decoder_init(&g_circuit_dec);
            if (g_active_config.link_password != NULL &&
                g_active_config.link_password[0] != '\0') {
                const char *link_role = g_active_config.link_role;
                hybbx_circuit_link_qos_t qos;
                char freq_buf[16];
                const char *freq_ptr = NULL;

                if (link_role == NULL || link_role[0] == '\0') {
                    link_role = "link";
                }

                memset(&qos, 0, sizeof(qos));
                qos.bandwidth = "low";
                qos.duplex = 1;
                if (g_active_config.frequency_mhz > 0.0) {
                    snprintf(freq_buf, sizeof(freq_buf), "%.3f",
                             g_active_config.frequency_mhz);
                    freq_ptr = freq_buf;
                }
                qos.frequency_mhz = freq_ptr;

                rc = hybbx_circuit_link_authenticate_ex(
                    g_circuit_fd, g_active_config.link_password,
                    link_role, link_id, &qos);
                if (rc != HYBBX_OK) {
                    close(g_circuit_fd);
                    g_circuit_fd = -1;
                    link_poll_sleep_ms(100);
                    continue;
                }
            }
            printf("[%s] linked to internal circuit %s:%u (HBX)\n",
                   link_tag(), host, port);
            return HYBBX_OK;
        }
        link_poll_sleep_ms(100);
    }

    fprintf(stderr,
            "[%s] could not connect to internal circuit %s:%u\n",
            link_tag(), host, port);
    return HYBBX_ERR_IO;
}

static void link_shutdown(void)
{
    g_running = 0;
    if (g_poll_thread_started) {
        pthread_join(g_poll_thread, NULL);
        g_poll_thread_started = 0;
    }

    if (g_circuit_fd >= 0) {
        close(g_circuit_fd);
        g_circuit_fd = -1;
    }

    if (g_host != NULL) {
        ardop_host_destroy(g_host);
        g_host = NULL;
    }

    if (g_plugin != NULL && g_plugin->config_free != NULL) {
        g_plugin->config_free(&g_active_config);
    }
}

hybbx_result_t ardop_link_init(struct hybbx_service *service,
                               const ardop_link_plugin_t *plugin)
{
    (void)service;
    g_plugin = plugin;
    g_host = NULL;
    g_circuit_fd = -1;
    g_running = 0;
    g_circuit_flow_paused = 0;
    g_circuit_flow_cancel = 0;
    g_modem_reconnect_backoff_ms = 1000u;
    return HYBBX_OK;
}

void ardop_link_shutdown(const ardop_link_plugin_t *plugin)
{
    (void)plugin;
    link_shutdown();
}

hybbx_result_t ardop_link_start(const ardop_link_plugin_t *plugin,
                                const char *config)
{
    hybbx_result_t rc;
    const char *modem_hint;

    if (plugin == NULL || plugin->parse_config == NULL) {
        return HYBBX_ERR_INVALID;
    }

    g_plugin = plugin;
    link_shutdown();
    memset(&g_active_config, 0, sizeof(g_active_config));
    g_poll_thread = (pthread_t)0;

    rc = plugin->parse_config(config, &g_active_config);
    if (rc != HYBBX_OK) {
        fprintf(stderr, "[%s] invalid configuration\n", link_tag());
        return rc;
    }

    printf("[%s] external modem %s:%u mycall=%s arq=%s profile=%s circuit=%s:%u "
           "link_id=%s\n",
           link_tag(),
           g_active_config.ardop_host,
           g_active_config.ardop_port,
           g_active_config.mycall,
           g_active_config.arq_bandwidth,
           hybbx_crdop_profile_name(g_active_config.radio_profile),
           g_active_config.circuit_host,
           g_active_config.circuit_port,
           g_active_config.link_id != NULL ? g_active_config.link_id : "?");

    if (g_active_config.radio_profile == HYBBX_CRDOP_PROFILE_CB) {
        printf("[%s] CB half-duplex profile (experimental Level 2)\n",
               link_tag());
    }
    if (g_active_config.frequency_mhz > 0.0) {
        printf("[%s] frequency_mhz=%.3f\n", link_tag(),
               g_active_config.frequency_mhz);
    }

    modem_hint = plugin->modem_hint != NULL ? plugin->modem_hint : "ARDOPC";
    printf("[%s] operator must run external %s separately (e.g. ardopc TCPIP %u)\n",
           link_tag(), modem_hint, g_active_config.ardop_port);

    g_host = ardop_host_create(&g_active_config, on_modem_data, on_modem_event,
                               NULL, link_tag());
    if (g_host == NULL) {
        link_shutdown();
        return HYBBX_ERR_NOMEM;
    }

    rc = ardop_host_connect(g_host);
    if (rc != HYBBX_OK) {
        fprintf(stderr,
                "[%s] external modem not reachable yet — will retry in poll loop\n",
                link_tag());
    }

    rc = link_connect_circuit();
    if (rc != HYBBX_OK) {
        fprintf(stderr,
                "[%s] circuit hub not reachable — HBX uplink deferred\n",
                link_tag());
    }

    g_running = 1;
    if (pthread_create(&g_poll_thread, NULL, link_poll_thread, NULL) != 0) {
        fprintf(stderr, "[%s] poll thread failed\n", link_tag());
        link_shutdown();
        return HYBBX_ERR_IO;
    }
    g_poll_thread_started = 1;

    return HYBBX_OK;
}

hybbx_result_t ardop_link_stop(const ardop_link_plugin_t *plugin)
{
    (void)plugin;
    printf("[%s] stop\n", link_tag());
    link_shutdown();
    return HYBBX_OK;
}
