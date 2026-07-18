#include "hybbx/broadcast.h"
#include "hybbx/messages.h"
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "hybbx/plugin.h"
#include "hybbx/circuit_tcp.h"
#include "hybbx/circuit.h"
#include "hybbx/ax25.h"
#include "hybbx/texts.h"
#include "hybbx/util.h"
#include "hybbx/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
        hybbx_log_info("[ax25] %u configured frequencies (%.3f … %.3f MHz)",
               table->count, table->mhz[0], table->mhz[table->count - 1]);
    } else {
        hybbx_log_info("[ax25] no frequencies in INI (set frequency1 … in [ax25])");
    }
}

void hybbx_broadcast_config_defaults(hybbx_broadcast_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->enabled = 1;
    cfg->ax25_enabled = 1;
    cfg->ax25_auto = 1;
    cfg->ax25_auto_interval_sec = HYBBX_BROADCAST_AX25_INTERVAL_MIN_SEC;
    cfg->ax25_auto_stagger_sec = 0;
    hybbx_strlcpy(cfg->ax25_mycall, "HYBBX", sizeof(cfg->ax25_mycall));
    hybbx_strlcpy(cfg->ax25_dest, "QST", sizeof(cfg->ax25_dest));
    hybbx_strlcpy(cfg->ax25_auto_message, HYBBX_BROADCAST_AUTO_MESSAGE_DEFAULT,
                  sizeof(cfg->ax25_auto_message));
    hybbx_ax25_frequency_table_clear(&cfg->frequencies);
}

void hybbx_broadcast_config_apply(hybbx_broadcast_config_t *cfg,
                                  const hybbx_config_t *config)
{
    const char *mycall;
    const char *dest;
    const char *auto_msg;
    unsigned interval;

    if (cfg == NULL || config == NULL) {
        return;
    }

    hybbx_broadcast_config_defaults(cfg);

    cfg->enabled = hybbx_config_get_bool(config, "broadcast", "enabled", 1);
    cfg->ax25_enabled = hybbx_config_get_bool(config, "broadcast", "ax25", 1);
    cfg->ax25_auto = hybbx_config_get_bool(config, "broadcast", "ax25_auto", 1);

    interval = hybbx_config_get_uint(config, "broadcast", "ax25_auto_interval",
                                     HYBBX_BROADCAST_AX25_INTERVAL_MIN_SEC,
                                     HYBBX_BROADCAST_AX25_INTERVAL_MIN_SEC,
                                     86400u);
    cfg->ax25_auto_interval_sec = interval;
    cfg->ax25_auto_stagger_sec = 0;

    mycall = hybbx_config_get(config, "broadcast", "ax25_mycall", NULL);
    dest = hybbx_config_get(config, "broadcast", "ax25_dest", NULL);
    auto_msg = hybbx_config_get(config, "broadcast", "ax25_auto_message", NULL);
    if (mycall != NULL && mycall[0] != '\0') {
        hybbx_ax25_address_t addr;

        if (hybbx_ax25_address_parse(mycall, &addr) == HYBBX_OK) {
            hybbx_strlcpy(cfg->ax25_mycall, mycall, sizeof(cfg->ax25_mycall));
        } else {
            hybbx_log_warn("[broadcast] ax25_mycall=%s invalid — using HYBBX",
                           mycall);
        }
    }
    if (dest != NULL && dest[0] != '\0') {
        hybbx_ax25_address_t addr;

        if (hybbx_ax25_address_parse(dest, &addr) == HYBBX_OK) {
            hybbx_strlcpy(cfg->ax25_dest, dest, sizeof(cfg->ax25_dest));
        } else {
            hybbx_log_warn("[broadcast] ax25_dest=%s invalid — using QST", dest);
            hybbx_strlcpy(cfg->ax25_dest, "QST", sizeof(cfg->ax25_dest));
        }
    }
    if (auto_msg != NULL && auto_msg[0] != '\0') {
        size_t msg_len = strlen(auto_msg);

        if (msg_len > HYBBX_BROADCAST_AX25_MESSAGE_MAX) {
            hybbx_log_warn("[broadcast] ax25_auto_message truncated (%zu > %u)",
                           msg_len, (unsigned)HYBBX_BROADCAST_AX25_MESSAGE_MAX);
        }
        hybbx_strlcpy(cfg->ax25_auto_message, auto_msg,
                      sizeof(cfg->ax25_auto_message));
    }

    hybbx_ax25_frequency_apply(&cfg->frequencies, config);

    hybbx_log_info("[broadcast] announce=%s ax25_auto=%s interval=%us "
                   "(min %us, band idle %us, link gap %us, per-link min %us)",
           cfg->enabled ? "yes" : "no",
           cfg->ax25_auto ? "yes" : "no",
           cfg->ax25_auto_interval_sec,
           (unsigned)HYBBX_BROADCAST_AX25_INTERVAL_MIN_SEC,
           (unsigned)HYBBX_BROADCAST_AX25_BAND_IDLE_SEC,
           (unsigned)HYBBX_BROADCAST_AX25_LINK_GAP_SEC,
           (unsigned)HYBBX_BROADCAST_AX25_LINK_MIN_SEC);
}

static hybbx_result_t broadcast_build_path(const hybbx_broadcast_config_t *cfg,
                                           hybbx_ax25_path_t *path)
{
    if (cfg == NULL || path == NULL) {
        return HYBBX_ERR_INVALID;
    }

    memset(path, 0, sizeof(*path));

    if (cfg->ax25_dest[0] == '\0' || strcmp(cfg->ax25_dest, "*") == 0) {
        hybbx_log_warn("[broadcast] ax25_dest=%s invalid — using QST",
                       cfg->ax25_dest[0] != '\0' ? cfg->ax25_dest : "(empty)");
        if (hybbx_ax25_address_parse("QST", &path->dest) != HYBBX_OK) {
            return HYBBX_ERR_INVALID;
        }
    } else if (hybbx_ax25_address_parse(cfg->ax25_dest, &path->dest) != HYBBX_OK) {
        return HYBBX_ERR_INVALID;
    }
    if (hybbx_ax25_address_parse(cfg->ax25_mycall, &path->source) != HYBBX_OK) {
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_OK;
}

static time_t g_ax25_last_sent;
static time_t g_ax25_link_last_sent[HYBBX_CIRCUIT_MAX_LINKS];
static unsigned g_ax25_auto_tick;
static unsigned g_ax25_defer_log_sec;

typedef struct broadcast_ax25_seq {
    int active;
    hybbx_service_t *service;
    hybbx_circuit_hub_t *hub;
    hybbx_circuit_broadcast_link_t links[HYBBX_CIRCUIT_MAX_LINKS];
    unsigned link_count;
    unsigned next_index;
    char message[HYBBX_BROADCAST_AX25_MESSAGE_MAX + 1];
    time_t resume_at;
} broadcast_ax25_seq_t;

static broadcast_ax25_seq_t g_ax25_seq;

static double broadcast_link_target_mhz(const hybbx_broadcast_config_t *cfg,
                                        const hybbx_circuit_broadcast_link_t *link,
                                        unsigned link_index)
{
    (void)cfg;
    (void)link_index;

    if (link == NULL) {
        return 0.0;
    }

    return link->frequency_mhz;
}

static void broadcast_ax25_log_deferred(double frequency_mhz, const char *reason)
{
    if (reason == NULL) {
        return;
    }

    g_ax25_defer_log_sec++;
    if (g_ax25_defer_log_sec < 60u) {
        return;
    }

    g_ax25_defer_log_sec = 0;
    if (frequency_mhz > 0.0) {
        hybbx_log_stats("[broadcast] ax25 %.3f MHz deferred (%s)",
               frequency_mhz, reason);
    } else {
        hybbx_log_stats("[broadcast] ax25 auto deferred (%s)", reason);
    }
}

static void broadcast_ax25_seq_defer_until(time_t resume_at, double target_mhz,
                                           const char *reason)
{
    time_t now = time(NULL);

    broadcast_ax25_log_deferred(target_mhz, reason);
    if (now != (time_t)-1 && (resume_at == (time_t)-1 || resume_at <= now)) {
        resume_at = now + (time_t)HYBBX_BROADCAST_AX25_DEFER_RETRY_SEC;
    }
    g_ax25_seq.resume_at = resume_at;
}

static int broadcast_hub_link_band_idle(hybbx_circuit_hub_t *hub,
                                        unsigned slot_index)
{
    if (hub == NULL) {
        return 0;
    }

    return hybbx_circuit_hub_link_band_idle(hub, slot_index,
                                            HYBBX_BROADCAST_AX25_BAND_IDLE_SEC);
}

static int broadcast_ax25_rate_ok(unsigned interval_sec)
{
    time_t now;
    time_t elapsed;

    if (g_ax25_last_sent == 0) {
        return 1;
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        return 0;
    }

    elapsed = now - g_ax25_last_sent;
    if (elapsed < 0) {
        return 1;
    }

    return (unsigned)elapsed >= interval_sec;
}

static int broadcast_ax25_link_rate_ok(unsigned slot_index,
                                       unsigned interval_sec)
{
    time_t now;
    time_t elapsed;

    if (slot_index >= HYBBX_CIRCUIT_MAX_LINKS) {
        return 0;
    }

    if (g_ax25_link_last_sent[slot_index] == 0) {
        return 1;
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        return 0;
    }

    elapsed = now - g_ax25_link_last_sent[slot_index];
    if (elapsed < 0) {
        return 1;
    }

    return (unsigned)elapsed >= interval_sec;
}

static void broadcast_ax25_mark_sent(void)
{
    time_t now = time(NULL);

    if (now != (time_t)-1) {
        g_ax25_last_sent = now;
    }
}

static void broadcast_ax25_mark_link_sent(unsigned slot_index)
{
    time_t now = time(NULL);

    if (slot_index >= HYBBX_CIRCUIT_MAX_LINKS || now == (time_t)-1) {
        return;
    }

    g_ax25_link_last_sent[slot_index] = now;
    g_ax25_last_sent = now;
}

static size_t broadcast_expand_service_token(char *out, size_t out_len,
                                             const char *tmpl,
                                             const char *service_name)
{
    size_t pos = 0;
    size_t i = 0;

    if (out == NULL || out_len == 0 || tmpl == NULL) {
        return 0;
    }

    if (service_name == NULL || service_name[0] == '\0') {
        service_name = HYBBX_DEFAULT_SERVICE_NAME;
    }

    out[0] = '\0';

    while (tmpl[i] != '\0' && pos + 1 < out_len) {
        if (strncmp(tmpl + i, HYBBX_BANNER_TOKEN_SERVICE,
                    strlen(HYBBX_BANNER_TOKEN_SERVICE)) == 0) {
            size_t n = strlen(service_name);

            if (pos + n >= out_len) {
                n = out_len - pos - 1;
            }
            memcpy(out + pos, service_name, n);
            pos += n;
            i += strlen(HYBBX_BANNER_TOKEN_SERVICE);
            continue;
        }

        out[pos++] = tmpl[i++];
    }

    out[pos] = '\0';
    return pos;
}

static hybbx_result_t broadcast_ax25_send_slot(hybbx_service_t *service,
                                               unsigned slot_index,
                                               const char *message,
                                               int enforce_rate_limit)
{
    const hybbx_broadcast_config_t *cfg;
    hybbx_circuit_hub_t *hub;
    hybbx_ax25_path_t path;
    uint8_t frame[HYBBX_CIRCUIT_MAX_FRAME];
    size_t frame_len;
    size_t msg_len;
    hybbx_result_t rc;
    hybbx_circuit_broadcast_link_t links[HYBBX_CIRCUIT_MAX_LINKS];
    unsigned link_count;
    unsigned i;
    const char *link_id = "";
    double target_mhz = 0.0;

    if (service == NULL || message == NULL || message[0] == '\0' ||
        slot_index >= HYBBX_CIRCUIT_MAX_LINKS) {
        return HYBBX_ERR_INVALID;
    }

    cfg = hybbx_service_get_broadcast(service);
    if (cfg == NULL || !cfg->enabled || !cfg->ax25_enabled) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    hub = hybbx_service_circuit_hub(service);
    if (hub == NULL || !hybbx_circuit_hub_running(hub)) {
        return HYBBX_ERR_BUSY;
    }

    link_count = hybbx_circuit_hub_broadcast_links(hub, links,
                                                   HYBBX_CIRCUIT_MAX_LINKS);
    for (i = 0; i < link_count; i++) {
        if (links[i].slot_index == slot_index) {
            link_id = links[i].link_id;
            target_mhz = links[i].frequency_mhz;
            break;
        }
    }
    if (link_id[0] == '\0') {
        return HYBBX_ERR_DENIED;
    }

    if (!hybbx_circuit_hub_link_band_idle(hub, slot_index,
                                          HYBBX_BROADCAST_AX25_BAND_IDLE_SEC)) {
        return HYBBX_ERR_BUSY;
    }

    if (enforce_rate_limit &&
        !broadcast_ax25_link_rate_ok(slot_index,
                                     HYBBX_BROADCAST_AX25_LINK_MIN_SEC)) {
        return HYBBX_ERR_BUSY;
    }

    msg_len = strlen(message);
    if (msg_len > HYBBX_BROADCAST_AX25_MESSAGE_MAX) {
        return HYBBX_ERR_INVALID;
    }

    if (!hybbx_circuit_hub_link_broadcast_qos(hub)) {
        return HYBBX_ERR_BUSY;
    }

    rc = broadcast_build_path(cfg, &path);
    if (rc != HYBBX_OK) {
        return rc;
    }

    {
        uint8_t ax25_frame[HYBBX_AX25_FRAME_MAX];
        size_t ax25_len = hybbx_ax25_build_ui(&path,
                                              (const uint8_t *)message, msg_len,
                                              ax25_frame, sizeof(ax25_frame));

        if (ax25_len == 0) {
            return HYBBX_ERR_IO;
        }

        hybbx_log_debug("[broadcast] ax25 path %s>%s payload=%zu slot=%u",
                        path.source.call, path.dest.call, msg_len, slot_index);

        frame_len = hybbx_circuit_encode(HYBBX_CIRCUIT_PROTO_AX25,
                                         HYBBX_CIRCUIT_FLAG_TX,
                                         ax25_frame, ax25_len,
                                         frame, sizeof(frame));
    }
    if (frame_len == 0) {
        return HYBBX_ERR_IO;
    }

    rc = hybbx_circuit_hub_send_hbx_slot(hub, slot_index, frame, frame_len, 1);
    if (rc != HYBBX_OK) {
        return rc;
    }

    broadcast_ax25_mark_link_sent(slot_index);

    if (target_mhz > 0.0) {
        hybbx_log_stats("[broadcast] ax25 link=%s %.3f MHz (slot %u): %s",
                        link_id, target_mhz, slot_index, message);
    } else {
        hybbx_log_stats("[broadcast] ax25 link=%s (slot %u): %s",
                        link_id, slot_index, message);
    }

    return HYBBX_OK;
}

static hybbx_result_t broadcast_ax25_send(hybbx_service_t *service,
                                            double frequency_mhz,
                                            const char *message,
                                            int enforce_rate_limit)
{
    const hybbx_broadcast_config_t *cfg;
    hybbx_circuit_hub_t *hub;
    hybbx_ax25_path_t path;
    uint8_t frame[HYBBX_CIRCUIT_MAX_FRAME];
    size_t frame_len;
    size_t msg_len;
    hybbx_result_t rc;
    unsigned sent_links;
    int per_link = 0;

    if (service == NULL || message == NULL || message[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    cfg = hybbx_service_get_broadcast(service);
    if (cfg == NULL || !cfg->enabled || !cfg->ax25_enabled) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    hub = hybbx_service_circuit_hub(service);
    if (hub == NULL || !hybbx_circuit_hub_running(hub)) {
        return HYBBX_ERR_BUSY;
    }

    if (frequency_mhz > 0.0) {
        hybbx_circuit_broadcast_link_t links[HYBBX_CIRCUIT_MAX_LINKS];
        unsigned link_count;
        unsigned i;
        int any_rate_ok = 0;

        link_count = hybbx_circuit_hub_broadcast_links(hub, links,
                                                       HYBBX_CIRCUIT_MAX_LINKS);
        for (i = 0; i < link_count; i++) {
            if (links[i].frequency_mhz <= 0.0 ||
                !hybbx_ax25_frequency_match(frequency_mhz,
                                            links[i].frequency_mhz)) {
                continue;
            }
            if (!enforce_rate_limit ||
                broadcast_ax25_link_rate_ok(links[i].slot_index,
                                            HYBBX_BROADCAST_AX25_LINK_MIN_SEC)) {
                any_rate_ok = 1;
                break;
            }
        }
        if (!any_rate_ok) {
            return HYBBX_ERR_BUSY;
        }
        per_link = 1;
    } else if (enforce_rate_limit &&
               !broadcast_ax25_rate_ok(cfg->ax25_auto_interval_sec)) {
        return HYBBX_ERR_BUSY;
    }

    msg_len = strlen(message);
    if (msg_len > HYBBX_BROADCAST_AX25_MESSAGE_MAX) {
        return HYBBX_ERR_INVALID;
    }

    if (!hybbx_circuit_hub_link_broadcast_qos(hub)) {
        return HYBBX_ERR_BUSY;
    }

    rc = broadcast_build_path(cfg, &path);
    if (rc != HYBBX_OK) {
        return rc;
    }

    {
        uint8_t ax25_frame[HYBBX_AX25_FRAME_MAX];
        size_t ax25_len = hybbx_ax25_build_ui(&path,
                                              (const uint8_t *)message, msg_len,
                                              ax25_frame, sizeof(ax25_frame));

        if (ax25_len == 0) {
            return HYBBX_ERR_IO;
        }

        hybbx_log_debug("[broadcast] ax25 path %s>%s payload=%zu",
                        path.source.call, path.dest.call, msg_len);

        frame_len = hybbx_circuit_encode(HYBBX_CIRCUIT_PROTO_AX25,
                                         HYBBX_CIRCUIT_FLAG_TX,
                                         ax25_frame, ax25_len,
                                         frame, sizeof(frame));
    }
    if (frame_len == 0) {
        return HYBBX_ERR_IO;
    }

    rc = hybbx_circuit_hub_multicast_hbx(hub, frame, frame_len,
                                         frequency_mhz, 1, &sent_links);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (per_link && frequency_mhz > 0.0) {
        hybbx_circuit_broadcast_link_t links[HYBBX_CIRCUIT_MAX_LINKS];
        unsigned link_count;
        unsigned i;

        link_count = hybbx_circuit_hub_broadcast_links(hub, links,
                                                       HYBBX_CIRCUIT_MAX_LINKS);
        for (i = 0; i < link_count; i++) {
            if (links[i].frequency_mhz <= 0.0 ||
                !hybbx_ax25_frequency_match(frequency_mhz,
                                            links[i].frequency_mhz)) {
                continue;
            }
            broadcast_ax25_mark_link_sent(links[i].slot_index);
        }
    } else {
        broadcast_ax25_mark_sent();
    }

    if (frequency_mhz > 0.0) {
        hybbx_log_stats("[broadcast] ax25 %.3f MHz (%u hub): %s",
               frequency_mhz, sent_links, message);
    } else {
        hybbx_log_stats("[broadcast] ax25 all qualifying links (%u hub): %s",
               sent_links, message);
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_broadcast_ax25(hybbx_service_t *service,
                                    double frequency_mhz,
                                    const char *message)
{
    return broadcast_ax25_send(service, frequency_mhz, message, 1);
}

static int broadcast_ax25_seq_start(hybbx_service_t *service,
                                    hybbx_circuit_hub_t *hub,
                                    const hybbx_circuit_broadcast_link_t *links,
                                    unsigned link_count,
                                    const char *message)
{
    if (g_ax25_seq.active || service == NULL || hub == NULL ||
        links == NULL || link_count == 0 || message == NULL ||
        message[0] == '\0' || link_count > HYBBX_CIRCUIT_MAX_LINKS) {
        return 0;
    }

    memset(&g_ax25_seq, 0, sizeof(g_ax25_seq));
    g_ax25_seq.active = 1;
    g_ax25_seq.service = service;
    g_ax25_seq.hub = hub;
    memcpy(g_ax25_seq.links, links,
           link_count * sizeof(g_ax25_seq.links[0]));
    g_ax25_seq.link_count = link_count;
    hybbx_strlcpy(g_ax25_seq.message, message, sizeof(g_ax25_seq.message));
    return 1;
}

static void broadcast_ax25_seq_advance(void)
{
    const hybbx_broadcast_config_t *cfg;
    time_t now;
    unsigned idx;
    double target_mhz;
    hybbx_result_t rc;
    int advance = 0;

    if (!g_ax25_seq.active) {
        return;
    }

    if (g_ax25_seq.hub == NULL ||
        !hybbx_circuit_hub_running(g_ax25_seq.hub)) {
        g_ax25_seq.active = 0;
        return;
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        return;
    }

    if (g_ax25_seq.resume_at != 0 && now < g_ax25_seq.resume_at) {
        return;
    }

    if (g_ax25_seq.next_index >= g_ax25_seq.link_count) {
        g_ax25_seq.active = 0;
        return;
    }

    cfg = hybbx_service_get_broadcast(g_ax25_seq.service);
    if (cfg == NULL) {
        g_ax25_seq.active = 0;
        return;
    }

    idx = g_ax25_seq.next_index;
    target_mhz = broadcast_link_target_mhz(cfg, &g_ax25_seq.links[idx], idx);

    if (target_mhz <= 0.0) {
        hybbx_log_stats("[broadcast] ax25 skip link=%s (no MHz)",
                        g_ax25_seq.links[idx].link_id);
        advance = 1;
    } else if (!broadcast_hub_link_band_idle(g_ax25_seq.hub,
                                             g_ax25_seq.links[idx].slot_index)) {
        time_t ready = hybbx_circuit_hub_link_band_ready_at(
            g_ax25_seq.hub, g_ax25_seq.links[idx].slot_index,
            HYBBX_BROADCAST_AX25_BAND_IDLE_SEC);

        broadcast_ax25_seq_defer_until(ready, target_mhz, "band busy");
        return;
    } else if (!broadcast_ax25_link_rate_ok(g_ax25_seq.links[idx].slot_index,
                                            HYBBX_BROADCAST_AX25_LINK_MIN_SEC)) {
        broadcast_ax25_seq_defer_until(
            now + (time_t)HYBBX_BROADCAST_AX25_DEFER_RETRY_SEC,
            target_mhz, "link rate");
        return;
    } else {
        rc = broadcast_ax25_send_slot(g_ax25_seq.service,
                                      g_ax25_seq.links[idx].slot_index,
                                      g_ax25_seq.message, 0);
        if (rc == HYBBX_OK) {
            g_ax25_defer_log_sec = 0;
            advance = 1;
        } else if (rc == HYBBX_ERR_BUSY) {
            if (!hybbx_circuit_hub_running(g_ax25_seq.hub)) {
                g_ax25_seq.active = 0;
                return;
            }
            {
                time_t ready = hybbx_circuit_hub_link_band_ready_at(
                    g_ax25_seq.hub, g_ax25_seq.links[idx].slot_index,
                    HYBBX_BROADCAST_AX25_BAND_IDLE_SEC);

                broadcast_ax25_seq_defer_until(ready, target_mhz,
                                               "band busy");
            }
            return;
        } else if (rc == HYBBX_ERR_DENIED) {
            broadcast_ax25_seq_defer_until(
                now + (time_t)HYBBX_BROADCAST_AX25_DEFER_RETRY_SEC,
                target_mhz, "no qualifying link");
            return;
        } else {
            hybbx_log_warn("[broadcast] ax25 %.3f MHz failed (%d)",
                           target_mhz, (int)rc);
            advance = 1;
        }
    }

    if (!advance) {
        return;
    }

    g_ax25_seq.next_index++;
    if (g_ax25_seq.next_index < g_ax25_seq.link_count) {
        g_ax25_seq.resume_at = now + (time_t)HYBBX_BROADCAST_AX25_LINK_GAP_SEC;
    } else {
        g_ax25_seq.active = 0;
        g_ax25_seq.resume_at = 0;
    }
}

void hybbx_broadcast_ax25_seq_cancel(void)
{
    g_ax25_seq.active = 0;
    g_ax25_seq.resume_at = 0;
}

hybbx_result_t hybbx_broadcast_ax25_manual(hybbx_service_t *service)
{
    const hybbx_broadcast_config_t *cfg;
    hybbx_circuit_hub_t *hub;
    hybbx_circuit_broadcast_link_t links[HYBBX_CIRCUIT_MAX_LINKS];
    char message[HYBBX_BROADCAST_AX25_MESSAGE_MAX + 1];
    unsigned link_count;

    if (service == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (g_ax25_seq.active) {
        return HYBBX_ERR_BUSY;
    }

    cfg = hybbx_service_get_broadcast(service);
    if (cfg == NULL || !cfg->enabled || !cfg->ax25_enabled) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    hub = hybbx_service_circuit_hub(service);
    if (hub == NULL || !hybbx_circuit_hub_running(hub)) {
        return HYBBX_ERR_BUSY;
    }

    link_count = hybbx_circuit_hub_broadcast_links(hub, links,
                                                   HYBBX_CIRCUIT_MAX_LINKS);
    if (link_count == 0) {
        return HYBBX_ERR_DENIED;
    }

    broadcast_expand_service_token(message, sizeof(message),
                                   cfg->ax25_auto_message,
                                   hybbx_service_get_name(service));
    if (message[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    hybbx_log_info("[broadcast] ax25 manual sequential (%u link(s)): %s",
                   link_count, message);

    if (!broadcast_ax25_seq_start(service, hub, links, link_count, message)) {
        return HYBBX_ERR_BUSY;
    }

    broadcast_ax25_seq_advance();
    return HYBBX_OK;
}

void hybbx_broadcast_ax25_tick(hybbx_service_t *service)
{
    const hybbx_broadcast_config_t *cfg;
    char message[HYBBX_BROADCAST_AX25_MESSAGE_MAX + 1];
    hybbx_circuit_hub_t *hub;
    hybbx_circuit_broadcast_link_t links[HYBBX_CIRCUIT_MAX_LINKS];
    unsigned link_count;
    unsigned interval;

    if (service == NULL) {
        return;
    }

    if (g_ax25_seq.active) {
        broadcast_ax25_seq_advance();
        return;
    }

    cfg = hybbx_service_get_broadcast(service);
    if (cfg == NULL || !cfg->enabled || !cfg->ax25_enabled || !cfg->ax25_auto) {
        return;
    }

    hub = hybbx_service_circuit_hub(service);
    if (hub == NULL || !hybbx_circuit_hub_running(hub)) {
        return;
    }

    interval = cfg->ax25_auto_interval_sec;
    if (interval == 0) {
        return;
    }

    link_count = hybbx_circuit_hub_broadcast_links(hub, links,
                                                   HYBBX_CIRCUIT_MAX_LINKS);
    if (link_count == 0) {
        return;
    }

    g_ax25_auto_tick++;

    broadcast_expand_service_token(message, sizeof(message),
                                   cfg->ax25_auto_message,
                                   hybbx_service_get_name(service));
    if (message[0] == '\0') {
        return;
    }

    if (g_ax25_auto_tick < interval) {
        return;
    }
    if (!broadcast_ax25_rate_ok(interval)) {
        return;
    }

    hybbx_log_info("[broadcast] ax25 auto sequential (%u link(s)): %s",
                   link_count, message);

    if (!broadcast_ax25_seq_start(service, hub, links, link_count, message)) {
        return;
    }

    g_ax25_auto_tick = 0;
    broadcast_ax25_seq_advance();
}

typedef struct broadcast_announce_ctx {
    hybbx_session_t *from;
    const char *from_name;
    const char *message;
} broadcast_announce_ctx_t;

static void broadcast_announce_visitor(hybbx_session_t *session, void *userdata)
{
    broadcast_announce_ctx_t *ctx = (broadcast_announce_ctx_t *)userdata;
    char line[HYBBX_LINE_MAX];

    if (session == NULL || ctx == NULL || ctx->message == NULL) {
        return;
    }

    /*
     * Local /broadcast is for online human users only. Circuit link adapter
     * sessions (HBX bridges to TNCs) must not receive terminal fan-out —
     * it floods low-bandwidth circuit queues and triggers load-balance
     * pause/break on packet-radio links.
     */
    if (!hybbx_session_is_interactive_user(session)) {
        return;
    }
    if (!hybbx_session_logged_in(session)) {
        return;
    }

    if (hybbx_msg_format_sysop(line, sizeof(line),
                             ctx->from_name != NULL ? ctx->from_name : "Announce",
                             ctx->message) != HYBBX_OK) {
        return;
    }
    if (ctx->from == NULL || session != ctx->from) {
        (void)hybbx_session_command_gap(session);
    }
    hybbx_session_write_line(session, line);
}

hybbx_result_t hybbx_broadcast_announce(hybbx_service_t *service,
                                        hybbx_session_t *from,
                                        const char *message)
{
    broadcast_announce_ctx_t ctx;
    const hybbx_broadcast_config_t *cfg;
    size_t msg_len;

    if (service == NULL || message == NULL || message[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    cfg = hybbx_service_get_broadcast(service);
    if (cfg == NULL || !cfg->enabled) {
        return HYBBX_ERR_UNSUPPORTED;
    }

    msg_len = strlen(message);
    if (msg_len > HYBBX_BROADCAST_MESSAGE_MAX) {
        return HYBBX_ERR_INVALID;
    }

    ctx.from = from;
    ctx.from_name = (from != NULL) ? hybbx_session_display_name(from) : "Announce";
    ctx.message = message;
    hybbx_service_visit_sessions(service, broadcast_announce_visitor, &ctx);

    hybbx_log_stats("[broadcast] announce to local Main (%zu bytes): %s",
           msg_len, message);
    return HYBBX_OK;
}
