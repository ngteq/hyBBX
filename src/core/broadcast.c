#include "hybbx/broadcast.h"
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "hybbx/circuit_tcp.h"
#include "hybbx/circuit.h"
#include "hybbx/ax25.h"
#include "hybbx/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hybbx_ax25_frequency_table_clear(hybbx_ax25_frequency_table_t *table)
{
    if (table == NULL) {
        return;
    }

    memset(table, 0, sizeof(*table));
}

int hybbx_ax25_frequency_match(double a_mhz, double b_mhz)
{
    double diff;

    if (a_mhz <= 0.0 || b_mhz <= 0.0) {
        return 0;
    }

    diff = a_mhz - b_mhz;
    if (diff < 0.0) {
        diff = -diff;
    }

    return diff < 0.001;
}

void hybbx_ax25_frequency_apply(hybbx_ax25_frequency_table_t *table,
                                const hybbx_config_t *config)
{
    unsigned count;
    unsigned i;
    unsigned loaded = 0;
    char key[24];
    const char *value;

    if (table == NULL || config == NULL) {
        return;
    }

    hybbx_ax25_frequency_table_clear(table);

    count = hybbx_config_get_uint(config, "ax25", "frequency_count",
                                  0u, 0u, HYBBX_AX25_FREQUENCY_MAX);

    for (i = 1; i <= HYBBX_AX25_FREQUENCY_MAX; i++) {
        snprintf(key, sizeof(key), "frequency%u", i);
        value = hybbx_config_get(config, "ax25", key, NULL);
        if (value == NULL || value[0] == '\0') {
            if (count > 0 && i > count) {
                break;
            }
            continue;
        }

        {
            double mhz = strtod(value, NULL);

            if (mhz <= 0.0) {
                continue;
            }
            if (loaded >= HYBBX_AX25_FREQUENCY_MAX) {
                break;
            }

            table->mhz[loaded] = mhz;

            snprintf(key, sizeof(key), "frequency%u_label", i);
            value = hybbx_config_get(config, "ax25", key, NULL);
            if (value == NULL || value[0] == '\0') {
                snprintf(key, sizeof(key), "frequency%u_name", i);
                value = hybbx_config_get(config, "ax25", key, NULL);
            }
            if (value != NULL && value[0] != '\0') {
                hybbx_strlcpy(table->labels[loaded], value,
                              sizeof(table->labels[loaded]));
            } else {
                snprintf(table->labels[loaded], sizeof(table->labels[loaded]),
                         "%.3f MHz", mhz);
            }
            loaded++;
        }

        if (count > 0 && i >= count) {
            break;
        }
    }

    table->count = loaded;

    if (table->count > 0) {
        printf("[ax25] %u configured frequencies (%.3f … %.3f MHz)\n",
               table->count, table->mhz[0], table->mhz[table->count - 1]);
    } else {
        printf("[ax25] no frequencies in INI (set frequency1 … in [ax25])\n");
    }
}

void hybbx_broadcast_config_defaults(hybbx_broadcast_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->enabled = 1;
    cfg->tcp_enabled = 1;
    cfg->ax25_enabled = 1;
    hybbx_strlcpy(cfg->ax25_mycall, "HYBBX", sizeof(cfg->ax25_mycall));
    hybbx_strlcpy(cfg->ax25_dest, "QST", sizeof(cfg->ax25_dest));
    hybbx_ax25_frequency_table_clear(&cfg->frequencies);
}

void hybbx_broadcast_config_apply(hybbx_broadcast_config_t *cfg,
                                  const hybbx_config_t *config)
{
    const char *mycall;
    const char *dest;

    if (cfg == NULL || config == NULL) {
        return;
    }

    hybbx_broadcast_config_defaults(cfg);

    cfg->enabled = hybbx_config_get_bool(config, "broadcast", "enabled", 1);
    cfg->tcp_enabled = hybbx_config_get_bool(config, "broadcast", "tcp", 1);
    cfg->ax25_enabled = hybbx_config_get_bool(config, "broadcast", "ax25", 1);

    mycall = hybbx_config_get(config, "broadcast", "ax25_mycall", NULL);
    dest = hybbx_config_get(config, "broadcast", "ax25_dest", NULL);
    if (mycall != NULL && mycall[0] != '\0') {
        hybbx_strlcpy(cfg->ax25_mycall, mycall, sizeof(cfg->ax25_mycall));
    }
    if (dest != NULL && dest[0] != '\0') {
        hybbx_strlcpy(cfg->ax25_dest, dest, sizeof(cfg->ax25_dest));
    }

    hybbx_ax25_frequency_apply(&cfg->frequencies, config);

    printf("[broadcast] enabled=%s ax25=%s (low+half-duplex QoS only) tcp=%s (stub)\n",
           cfg->enabled ? "yes" : "no",
           cfg->ax25_enabled ? "yes" : "no",
           cfg->tcp_enabled ? "yes" : "no");
}

static hybbx_result_t broadcast_build_path(const hybbx_broadcast_config_t *cfg,
                                           hybbx_ax25_path_t *path)
{
    if (cfg == NULL || path == NULL) {
        return HYBBX_ERR_INVALID;
    }

    memset(path, 0, sizeof(*path));

    if (hybbx_ax25_address_parse(cfg->ax25_dest, &path->dest) != HYBBX_OK) {
        return HYBBX_ERR_INVALID;
    }
    if (hybbx_ax25_address_parse(cfg->ax25_mycall, &path->source) != HYBBX_OK) {
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_broadcast_ax25(hybbx_service_t *service,
                                    double frequency_mhz,
                                    const char *message)
{
    const hybbx_broadcast_config_t *cfg;
    hybbx_circuit_hub_t *hub;
    hybbx_ax25_path_t path;
    uint8_t frame[HYBBX_CIRCUIT_MAX_FRAME];
    size_t frame_len;
    size_t msg_len;
    hybbx_result_t rc;
    unsigned sent_links;

    if (service == NULL || message == NULL || message[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    cfg = hybbx_service_get_broadcast(service);
    if (cfg == NULL || !cfg->enabled || !cfg->ax25_enabled) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    msg_len = strlen(message);
    if (msg_len > HYBBX_BROADCAST_MESSAGE_MAX) {
        return HYBBX_ERR_INVALID;
    }

    hub = hybbx_service_circuit_hub(service);
    if (hub == NULL || !hybbx_circuit_hub_running(hub)) {
        return HYBBX_ERR_BUSY;
    }

    rc = broadcast_build_path(cfg, &path);
    if (rc != HYBBX_OK) {
        return rc;
    }

    frame_len = hybbx_circuit_encode_ax25_ui(&path,
                                             (const uint8_t *)message, msg_len,
                                             HYBBX_CIRCUIT_FLAG_TX,
                                             frame, sizeof(frame));
    if (frame_len == 0) {
        return HYBBX_ERR_IO;
    }

    rc = hybbx_circuit_hub_multicast_hbx(hub, frame, frame_len,
                                         frequency_mhz, 1);
    if (rc != HYBBX_OK) {
        return rc;
    }

    sent_links = hybbx_circuit_hub_active_link_count(hub);
    if (frequency_mhz > 0.0) {
        printf("[broadcast] ax25 %.3f MHz (%u link(s), low+half): %s\n",
               frequency_mhz, sent_links, message);
    } else {
        printf("[broadcast] ax25 all qualifying links (%u active): %s\n",
               sent_links, message);
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_broadcast_tcp_stub(hybbx_service_t *service,
                                          const char *message)
{
    const hybbx_broadcast_config_t *cfg;
    hybbx_circuit_hub_t *hub;
    unsigned links;

    if (service == NULL || message == NULL || message[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    cfg = hybbx_service_get_broadcast(service);
    if (cfg == NULL || !cfg->enabled || !cfg->tcp_enabled) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    hub = hybbx_service_circuit_hub(service);
    links = (hub != NULL) ? hybbx_circuit_hub_active_link_count(hub) : 0u;

    printf("[broadcast] tcp stub (%u circuit link(s); not on wire yet): %s\n",
           links, message);
    return HYBBX_OK;
}

void hybbx_broadcast_list_ax25_frequencies(hybbx_session_t *session,
                                           const hybbx_broadcast_config_t *cfg)
{
    unsigned i;
    char line[96];

    if (session == NULL || cfg == NULL) {
        return;
    }

    if (cfg->frequencies.count == 0) {
        hybbx_session_write_line(session,
            "No AX.25 frequencies configured ([ax25] frequency1 = MHz …).");
        return;
    }

    hybbx_session_write_line(session,
        "AX.25 broadcast frequencies (MHz only — tune radio manually):");

    for (i = 0; i < cfg->frequencies.count; i++) {
        if (cfg->frequencies.labels[i][0] != '\0') {
            snprintf(line, sizeof(line), "  %.3f MHz  %s",
                     cfg->frequencies.mhz[i], cfg->frequencies.labels[i]);
        } else {
            snprintf(line, sizeof(line), "  %.3f MHz",
                     cfg->frequencies.mhz[i]);
        }
        hybbx_session_write_line(session, line);
    }

    hybbx_session_write_line(session,
        "Broadcast uses low-bandwidth + half-duplex links only (QoS).");
}
