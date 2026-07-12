#include "hybbx/broadcast.h"
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
    unsigned stagger;

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

    stagger = hybbx_config_get_uint(config, "broadcast", "ax25_auto_stagger",
                                    0u, 0u, interval > 0u ? interval - 1u : 0u);
    cfg->ax25_auto_stagger_sec = stagger;

    mycall = hybbx_config_get(config, "broadcast", "ax25_mycall", NULL);
    dest = hybbx_config_get(config, "broadcast", "ax25_dest", NULL);
    auto_msg = hybbx_config_get(config, "broadcast", "ax25_auto_message", NULL);
    if (mycall != NULL && mycall[0] != '\0') {
        hybbx_strlcpy(cfg->ax25_mycall, mycall, sizeof(cfg->ax25_mycall));
    }
    if (dest != NULL && dest[0] != '\0') {
        hybbx_strlcpy(cfg->ax25_dest, dest, sizeof(cfg->ax25_dest));
    }
    if (auto_msg != NULL && auto_msg[0] != '\0') {
        hybbx_strlcpy(cfg->ax25_auto_message, auto_msg,
                      sizeof(cfg->ax25_auto_message));
    }

    hybbx_ax25_frequency_apply(&cfg->frequencies, config);

    hybbx_log_info("[broadcast] announce=%s ax25_auto=%s interval=%us stagger=%us (min %us)",
           cfg->enabled ? "yes" : "no",
           cfg->ax25_auto ? "yes" : "no",
           cfg->ax25_auto_interval_sec,
           cfg->ax25_auto_stagger_sec,
           (unsigned)HYBBX_BROADCAST_AX25_INTERVAL_MIN_SEC);
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

static time_t g_ax25_last_sent;
static time_t g_ax25_link_last_sent[HYBBX_CIRCUIT_MAX_LINKS];
static unsigned g_ax25_auto_tick;
static unsigned g_ax25_link_retry[HYBBX_CIRCUIT_MAX_LINKS];
static unsigned g_ax25_defer_log_sec;

static double broadcast_link_target_mhz(const hybbx_broadcast_config_t *cfg,
                                        const hybbx_circuit_broadcast_link_t *link,
                                        unsigned link_index)
{
    double mhz;

    if (link == NULL) {
        return 0.0;
    }

    mhz = link->frequency_mhz;
    if (mhz <= 0.0 && cfg != NULL && link_index < cfg->frequencies.count) {
        mhz = cfg->frequencies.mhz[link_index];
    }

    return mhz;
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
                                            cfg->ax25_auto_interval_sec)) {
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

    frame_len = hybbx_circuit_encode_ax25_ui(&path,
                                             (const uint8_t *)message, msg_len,
                                             HYBBX_CIRCUIT_FLAG_TX,
                                             frame, sizeof(frame));
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
            g_ax25_link_retry[links[i].slot_index] = 0;
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

hybbx_result_t hybbx_broadcast_ax25_manual(hybbx_service_t *service)
{
    const hybbx_broadcast_config_t *cfg;
    hybbx_circuit_hub_t *hub;
    hybbx_circuit_broadcast_link_t links[HYBBX_CIRCUIT_MAX_LINKS];
    char message[HYBBX_BROADCAST_AX25_MESSAGE_MAX + 1];
    unsigned link_count;
    unsigned sent = 0;
    unsigned i;

    if (service == NULL) {
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

    for (i = 0; i < link_count; i++) {
        double target_mhz = broadcast_link_target_mhz(cfg, &links[i], i);
        hybbx_result_t rc;

        if (target_mhz <= 0.0) {
            hybbx_log_stats("[broadcast] ax25 manual skip link=%s (no MHz)",
                            links[i].link_id);
            continue;
        }

        rc = broadcast_ax25_send(service, target_mhz, message, 0);
        if (rc == HYBBX_OK) {
            sent++;
            continue;
        }

        if (rc == HYBBX_ERR_BUSY) {
            hybbx_log_stats("[broadcast] ax25 manual %.3f MHz deferred (balancer busy)",
                   target_mhz);
            continue;
        }

        hybbx_log_warn("[broadcast] ax25 manual %.3f MHz failed (%d)",
               target_mhz, (int)rc);
    }

    if (sent == 0) {
        return HYBBX_ERR_BUSY;
    }

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
    unsigned stagger;
    unsigned i;

    if (service == NULL) {
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

    if (!hybbx_circuit_hub_link_broadcast_qos(hub)) {
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

    stagger = cfg->ax25_auto_stagger_sec;
    g_ax25_auto_tick++;

    broadcast_expand_service_token(message, sizeof(message),
                                   cfg->ax25_auto_message,
                                   hybbx_service_get_name(service));
    if (message[0] == '\0') {
        return;
    }

    if (stagger == 0 || link_count == 1) {
        if (g_ax25_auto_tick < interval) {
            return;
        }
        if (!broadcast_ax25_rate_ok(interval)) {
            return;
        }

        {
            hybbx_result_t rc = hybbx_broadcast_ax25(service, 0.0, message);

            if (rc == HYBBX_OK) {
                g_ax25_auto_tick = 0;
                return;
            }

            if (rc == HYBBX_ERR_BUSY && interval > 0) {
                g_ax25_auto_tick = interval - 1u;
                broadcast_ax25_log_deferred(0.0, "balancer busy");
            }
        }
        return;
    }

    for (i = 0; i < link_count; i++) {
        unsigned phase = (i * stagger) % interval;
        double target_mhz = broadcast_link_target_mhz(cfg, &links[i], i);
        hybbx_result_t rc;

        if (!g_ax25_link_retry[links[i].slot_index] &&
            g_ax25_auto_tick % interval != phase) {
            continue;
        }
        if (!broadcast_ax25_link_rate_ok(links[i].slot_index, interval)) {
            continue;
        }
        if (target_mhz <= 0.0) {
            hybbx_log_stats("[broadcast] ax25 skip link=%s (no MHz)",
                            links[i].link_id);
            continue;
        }

        rc = hybbx_broadcast_ax25(service, target_mhz, message);
        if (rc == HYBBX_OK) {
            g_ax25_link_retry[links[i].slot_index] = 0;
            g_ax25_defer_log_sec = 0;
            continue;
        }

        if (rc == HYBBX_ERR_BUSY) {
            g_ax25_link_retry[links[i].slot_index] = 1;
            broadcast_ax25_log_deferred(target_mhz, "balancer busy");
        } else if (rc == HYBBX_ERR_DENIED) {
            g_ax25_link_retry[links[i].slot_index] = 1;
            broadcast_ax25_log_deferred(target_mhz, "no qualifying link");
        }
    }

    if (g_ax25_auto_tick >= interval) {
        g_ax25_auto_tick = 0;
    }
}

typedef struct broadcast_announce_ctx {
    hybbx_session_t *from;
    const char *from_name;
    const char *message;
} broadcast_announce_ctx_t;

static int broadcast_announce_is_local_user(const hybbx_session_t *session)
{
    hybbx_transport_kind_t kind;

    if (session == NULL || session->transport == NULL) {
        return 1;
    }

    kind = session->transport->kind;
    return kind == HYBBX_TRANSPORT_TELNET ||
           kind == HYBBX_TRANSPORT_SSH ||
           kind == HYBBX_TRANSPORT_WEBSOCKET;
}

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
    if (!broadcast_announce_is_local_user(session)) {
        return;
    }
    if (!hybbx_session_logged_in(session)) {
        return;
    }

    snprintf(line, sizeof(line), "*** %s: %s ***",
             ctx->from_name != NULL ? ctx->from_name : "Announce",
             ctx->message);
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
