#include "hybbx/circuit_balance.h"
#include "hybbx/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct balance_frame {
    uint8_t data[HYBBX_CIRCUIT_MAX_FRAME];
    size_t len;
} balance_frame_t;

struct hybbx_circuit_balance {
    hybbx_circuit_balance_config_t cfg;
    hybbx_circuit_link_profile_t profile;
    balance_frame_t queue[HYBBX_CIRCUIT_BALANCE_QUEUE_MAX];
    size_t q_head;
    size_t q_count;
    size_t queue_bytes;
    hybbx_circuit_balance_action_t action;
    hybbx_circuit_balance_action_t last_sent_action;
    double drain_accum;
    time_t oldest_enqueue;
    int link_low_bw;
};

static int str_ieq_local(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }

    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;

        if (ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return 0;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static const char *find_kv_line(const char *payload, size_t len,
                                const char *key,
                                char *scratch, size_t scratch_len)
{
    const char *cursor = payload;
    const char *end = payload + len;
    size_t key_len = strlen(key);

    while (cursor < end) {
        const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
        const char *line_stop = line_end != NULL ? line_end : end;
        const char *eq = memchr(cursor, '=', (size_t)(line_stop - cursor));

        if (eq != NULL && (size_t)(eq - cursor) == key_len &&
            memcmp(cursor, key, key_len) == 0) {
            const char *value = eq + 1;
            size_t value_len = (size_t)(line_stop - value);

            while (value_len > 0 &&
                   (value[value_len - 1] == '\r' ||
                    value[value_len - 1] == '\n' ||
                    value[value_len - 1] == ' ')) {
                value_len--;
            }
            if (value_len >= scratch_len) {
                value_len = scratch_len - 1;
            }
            memcpy(scratch, value, value_len);
            scratch[value_len] = '\0';
            return scratch;
        }

        if (line_end == NULL) {
            break;
        }
        cursor = line_end + 1;
    }

    return NULL;
}

void hybbx_circuit_balance_config_defaults(hybbx_circuit_balance_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    cfg->enabled = 1;
    cfg->lag_sec = 5u;
    cfg->queue_pause = 4096u;
    cfg->queue_break = 16384u;
    cfg->queue_cancel = 65536u;
}

void hybbx_circuit_link_profile_from_auth(const hybbx_link_auth_t *auth,
                                          hybbx_circuit_link_profile_t *out)
{
    unsigned baud;

    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->bandwidth = HYBBX_CIRCUIT_BW_HIGH;
    out->duplex = HYBBX_CIRCUIT_DUPLEX_FULL;
    out->baud = HYBBX_CIRCUIT_BALANCE_DEFAULT_BAUD;

    if (auth == NULL) {
        return;
    }

    baud = auth->baud;
    out->duplex = (hybbx_circuit_duplex_t)auth->duplex;

    if (auth->bandwidth[0] != '\0') {
        if (str_ieq_local(auth->bandwidth, "low")) {
            out->bandwidth = HYBBX_CIRCUIT_BW_LOW;
        } else if (str_ieq_local(auth->bandwidth, "high")) {
            out->bandwidth = HYBBX_CIRCUIT_BW_HIGH;
        }
    } else if (baud > 0 && baud <= HYBBX_CIRCUIT_BALANCE_LOW_BAUD_THRESHOLD) {
        out->bandwidth = HYBBX_CIRCUIT_BW_LOW;
    }

    if (baud > 0) {
        out->baud = baud;
    } else if (out->bandwidth == HYBBX_CIRCUIT_BW_LOW) {
        out->baud = 1200u;
    }

    if (out->duplex == HYBBX_CIRCUIT_DUPLEX_UNSET) {
        out->duplex = out->bandwidth == HYBBX_CIRCUIT_BW_LOW ?
                      HYBBX_CIRCUIT_DUPLEX_HALF : HYBBX_CIRCUIT_DUPLEX_FULL;
    }

    if (auth->frequency_mhz[0] != '\0') {
        out->frequency_mhz = strtod(auth->frequency_mhz, NULL);
    }
}

const char *hybbx_circuit_balance_action_name(hybbx_circuit_balance_action_t action)
{
    switch (action) {
    case HYBBX_CIRCUIT_BAL_PAUSE:
        return "pause";
    case HYBBX_CIRCUIT_BAL_BREAK:
        return "break";
    case HYBBX_CIRCUIT_BAL_CANCEL:
        return "cancel";
    case HYBBX_CIRCUIT_BAL_RESUME:
        return "resume";
    case HYBBX_CIRCUIT_BAL_NONE:
    default:
        return "none";
    }
}

hybbx_result_t hybbx_circuit_flow_ctrl_parse(const char *payload, size_t len,
                                             hybbx_circuit_balance_action_t *action_out,
                                             char *reason_out, size_t reason_cap)
{
    char scratch[128];
    const char *action_str;

    if (payload == NULL || action_out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    action_str = find_kv_line(payload, len, "action", scratch, sizeof(scratch));
    if (action_str == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (str_ieq_local(action_str, "pause")) {
        *action_out = HYBBX_CIRCUIT_BAL_PAUSE;
    } else if (str_ieq_local(action_str, "break")) {
        *action_out = HYBBX_CIRCUIT_BAL_BREAK;
    } else if (str_ieq_local(action_str, "cancel")) {
        *action_out = HYBBX_CIRCUIT_BAL_CANCEL;
    } else if (str_ieq_local(action_str, "resume")) {
        *action_out = HYBBX_CIRCUIT_BAL_RESUME;
    } else {
        return HYBBX_ERR_INVALID;
    }

    if (reason_out != NULL && reason_cap > 0) {
        reason_out[0] = '\0';
        if (find_kv_line(payload, len, "reason", scratch, sizeof(scratch)) != NULL) {
            hybbx_strlcpy(reason_out, scratch, reason_cap);
        }
    }

    return HYBBX_OK;
}

size_t hybbx_circuit_flow_ctrl_format(hybbx_circuit_balance_action_t action,
                                      const char *reason,
                                      char *out, size_t out_cap)
{
    int n;
    const char *reason_str = reason != NULL && reason[0] != '\0' ? reason : "-";

    if (out == NULL || out_cap == 0) {
        return 0;
    }

    n = snprintf(out, out_cap, "action=%s\nreason=%s\n",
                 hybbx_circuit_balance_action_name(action), reason_str);
    if (n < 0 || (size_t)n >= out_cap) {
        return 0;
    }

    return (size_t)n;
}

hybbx_circuit_balance_t *hybbx_circuit_balance_create(
    const hybbx_circuit_balance_config_t *cfg)
{
    hybbx_circuit_balance_t *bal;

    bal = calloc(1, sizeof(*bal));
    if (bal == NULL) {
        return NULL;
    }

    if (cfg != NULL) {
        bal->cfg = *cfg;
    } else {
        hybbx_circuit_balance_config_defaults(&bal->cfg);
    }

    hybbx_circuit_link_profile_from_auth(NULL, &bal->profile);
    bal->link_low_bw = 0;
    return bal;
}

void hybbx_circuit_balance_destroy(hybbx_circuit_balance_t *bal)
{
    free(bal);
}

void hybbx_circuit_balance_set_profile(hybbx_circuit_balance_t *bal,
                                       const hybbx_circuit_link_profile_t *profile)
{
    if (bal == NULL || profile == NULL) {
        return;
    }

    bal->profile = *profile;
    bal->link_low_bw = profile->bandwidth == HYBBX_CIRCUIT_BW_LOW;
}

hybbx_circuit_balance_action_t hybbx_circuit_balance_action(
    const hybbx_circuit_balance_t *bal)
{
    if (bal == NULL) {
        return HYBBX_CIRCUIT_BAL_NONE;
    }

    return bal->action;
}

size_t hybbx_circuit_balance_queued_bytes(const hybbx_circuit_balance_t *bal)
{
    if (bal == NULL) {
        return 0;
    }

    return bal->queue_bytes;
}

void hybbx_circuit_balance_spared_cancel(hybbx_circuit_balance_t *bal)
{
    if (bal == NULL) {
        return;
    }

    if (bal->action == HYBBX_CIRCUIT_BAL_CANCEL) {
        bal->action = HYBBX_CIRCUIT_BAL_PAUSE;
        bal->last_sent_action = HYBBX_CIRCUIT_BAL_NONE;
    }
}

static unsigned balance_effective_baud(const hybbx_circuit_balance_t *bal)
{
    unsigned baud;

    if (bal == NULL) {
        return HYBBX_CIRCUIT_BALANCE_DEFAULT_BAUD;
    }

    baud = bal->profile.baud;
    if (baud == 0) {
        baud = HYBBX_CIRCUIT_BALANCE_DEFAULT_BAUD;
    }
    return baud;
}

static unsigned balance_estimated_lag_sec(const hybbx_circuit_balance_t *bal)
{
    unsigned baud;
    unsigned lag;

    if (bal == NULL || bal->queue_bytes == 0) {
        return 0;
    }

    baud = balance_effective_baud(bal);
    if (baud == 0) {
        return 0;
    }

    lag = (unsigned)((bal->queue_bytes * 8u) / baud);
    if (bal->oldest_enqueue > 0) {
        time_t now = time(NULL);
        if (now > bal->oldest_enqueue) {
            unsigned wall = (unsigned)(now - bal->oldest_enqueue);
            if (wall > lag) {
                lag = wall;
            }
        }
    }
    return lag;
}

static void balance_send_flow(hybbx_circuit_balance_t *bal,
                              hybbx_circuit_balance_action_t action,
                              const char *reason,
                              hybbx_circuit_balance_flow_fn flow_fn,
                              void *flow_ctx)
{
    if (bal == NULL || flow_fn == NULL) {
        return;
    }
    if (action == bal->last_sent_action) {
        return;
    }

    flow_fn(flow_ctx, action, reason);
    bal->last_sent_action = action;
}

static void balance_clear_queue(hybbx_circuit_balance_t *bal)
{
    if (bal == NULL) {
        return;
    }

    bal->q_head = 0;
    bal->q_count = 0;
    bal->queue_bytes = 0;
    bal->oldest_enqueue = 0;
    bal->drain_accum = 0.0;
}

static void balance_apply_escalation(hybbx_circuit_balance_t *bal,
                                     hybbx_circuit_balance_flow_fn flow_fn,
                                     void *flow_ctx)
{
    unsigned lag;
    const hybbx_circuit_balance_config_t *cfg;

    if (bal == NULL || !bal->cfg.enabled) {
        return;
    }

    cfg = &bal->cfg;
    lag = balance_estimated_lag_sec(bal);

    if (bal->link_low_bw &&
        (bal->queue_bytes >= cfg->queue_cancel ||
         lag >= cfg->lag_sec * 2u)) {
        if (bal->action != HYBBX_CIRCUIT_BAL_CANCEL) {
            bal->action = HYBBX_CIRCUIT_BAL_CANCEL;
            balance_clear_queue(bal);
            balance_send_flow(bal, HYBBX_CIRCUIT_BAL_CANCEL, "queue_overload",
                              flow_fn, flow_ctx);
        }
        return;
    }

    if (bal->queue_bytes >= cfg->queue_break || lag >= cfg->lag_sec) {
        if (bal->action != HYBBX_CIRCUIT_BAL_BREAK &&
            bal->action != HYBBX_CIRCUIT_BAL_CANCEL) {
            bal->action = HYBBX_CIRCUIT_BAL_BREAK;
            balance_clear_queue(bal);
            balance_send_flow(bal, HYBBX_CIRCUIT_BAL_BREAK, "queue_lag",
                              flow_fn, flow_ctx);
            bal->action = HYBBX_CIRCUIT_BAL_PAUSE;
            balance_send_flow(bal, HYBBX_CIRCUIT_BAL_PAUSE, "stabilize",
                              flow_fn, flow_ctx);
        }
        return;
    }

    if (bal->queue_bytes >= cfg->queue_pause ||
        lag >= (cfg->lag_sec > 0 ? cfg->lag_sec / 2u : 0u)) {
        if (bal->action == HYBBX_CIRCUIT_BAL_NONE) {
            bal->action = HYBBX_CIRCUIT_BAL_PAUSE;
            balance_send_flow(bal, HYBBX_CIRCUIT_BAL_PAUSE, "queue_pressure",
                              flow_fn, flow_ctx);
        }
        return;
    }

    if (bal->action == HYBBX_CIRCUIT_BAL_PAUSE &&
        bal->queue_bytes < cfg->queue_pause / 2u &&
        lag < (cfg->lag_sec > 0 ? cfg->lag_sec / 2u : 0u)) {
        bal->action = HYBBX_CIRCUIT_BAL_NONE;
        balance_send_flow(bal, HYBBX_CIRCUIT_BAL_RESUME, "stabilized",
                          flow_fn, flow_ctx);
    }
}

hybbx_result_t hybbx_circuit_balance_submit(hybbx_circuit_balance_t *bal,
                                            const uint8_t *frame, size_t len,
                                            hybbx_circuit_balance_send_fn send_fn,
                                            void *send_ctx,
                                            hybbx_circuit_balance_flow_fn flow_fn,
                                            void *flow_ctx)
{
    size_t slot;
    balance_frame_t *entry;

    if (bal == NULL || frame == NULL || len == 0 || send_fn == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (!bal->cfg.enabled ||
        bal->profile.bandwidth == HYBBX_CIRCUIT_BW_HIGH) {
        return send_fn(send_ctx, frame, len);
    }

    if (bal->action == HYBBX_CIRCUIT_BAL_CANCEL) {
        return HYBBX_ERR_BUSY;
    }

    if (bal->q_count >= HYBBX_CIRCUIT_BALANCE_QUEUE_MAX) {
        balance_apply_escalation(bal, flow_fn, flow_ctx);
        if (bal->action == HYBBX_CIRCUIT_BAL_CANCEL) {
            return HYBBX_ERR_BUSY;
        }
        return HYBBX_ERR_NOMEM;
    }

    if (len > sizeof(entry->data)) {
        return HYBBX_ERR_INVALID;
    }

    slot = (bal->q_head + bal->q_count) % HYBBX_CIRCUIT_BALANCE_QUEUE_MAX;
    entry = &bal->queue[slot];
    memcpy(entry->data, frame, len);
    entry->len = len;
    bal->q_count++;
    bal->queue_bytes += len;
    if (bal->oldest_enqueue == 0) {
        bal->oldest_enqueue = time(NULL);
    }

    balance_apply_escalation(bal, flow_fn, flow_ctx);
    return HYBBX_OK;
}

hybbx_circuit_balance_tick_result_t hybbx_circuit_balance_tick(
    hybbx_circuit_balance_t *bal, unsigned poll_ms,
    hybbx_circuit_balance_send_fn send_fn, void *send_ctx,
    hybbx_circuit_balance_flow_fn flow_fn, void *flow_ctx)
{
    unsigned baud;
    double budget;
    size_t sent_budget;

    if (bal == NULL || send_fn == NULL) {
        return HYBBX_CIRCUIT_BAL_TICK_OK;
    }

    if (bal->action == HYBBX_CIRCUIT_BAL_CANCEL) {
        return HYBBX_CIRCUIT_BAL_TICK_CANCEL_LINK;
    }

    if (!bal->cfg.enabled || bal->q_count == 0) {
        balance_apply_escalation(bal, flow_fn, flow_ctx);
        return HYBBX_CIRCUIT_BAL_TICK_OK;
    }

    if (bal->action == HYBBX_CIRCUIT_BAL_PAUSE) {
        balance_apply_escalation(bal, flow_fn, flow_ctx);
        if (bal->action == HYBBX_CIRCUIT_BAL_CANCEL) {
            return HYBBX_CIRCUIT_BAL_TICK_CANCEL_LINK;
        }
        return HYBBX_CIRCUIT_BAL_TICK_OK;
    }

    baud = balance_effective_baud(bal);
    if (poll_ms == 0) {
        poll_ms = 1;
    }

    budget = ((double)baud * (double)poll_ms) / 8000.0;
    bal->drain_accum += budget;
    sent_budget = (size_t)bal->drain_accum;
    bal->drain_accum -= (double)sent_budget;

    while (sent_budget > 0 && bal->q_count > 0) {
        balance_frame_t *entry = &bal->queue[bal->q_head];

        if (entry->len > sent_budget) {
            break;
        }

        if (send_fn(send_ctx, entry->data, entry->len) != HYBBX_OK) {
            break;
        }

        sent_budget -= entry->len;
        bal->queue_bytes -= entry->len;
        bal->q_head = (bal->q_head + 1u) % HYBBX_CIRCUIT_BALANCE_QUEUE_MAX;
        bal->q_count--;
    }

    if (bal->q_count == 0) {
        bal->oldest_enqueue = 0;
    }

    balance_apply_escalation(bal, flow_fn, flow_ctx);
    if (bal->action == HYBBX_CIRCUIT_BAL_CANCEL) {
        return HYBBX_CIRCUIT_BAL_TICK_CANCEL_LINK;
    }

    return HYBBX_CIRCUIT_BAL_TICK_OK;
}
