#include "hybbx/bandwidth_policy.h"
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "hybbx/plugin.h"
#include "hybbx/log.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define BW_USER_MAX 128u

/** Lower = sacrificed first under pressure (AX.25 users before full-duplex TCP). */
#define BW_PRIORITY_AX25_USER   0
#define BW_PRIORITY_TCP_USER    1

typedef struct bw_user_entry {
    hybbx_session_t *session;
    time_t connected_at;
    int sacrifice_priority;
} bw_user_entry_t;

typedef struct bw_collect_ctx {
    bw_user_entry_t entries[BW_USER_MAX];
    unsigned count;
} bw_collect_ctx_t;

static int session_sacrifice_priority(const hybbx_session_t *session)
{
    const hybbx_transport_plugin_t *transport;

    if (session == NULL) {
        return -1;
    }

    transport = session->transport;
    if (transport == NULL) {
        return -1;
    }

    switch (transport->kind) {
    case HYBBX_TRANSPORT_PACKET_RADIO:
        return BW_PRIORITY_AX25_USER;
    case HYBBX_TRANSPORT_TELNET:
        return BW_PRIORITY_TCP_USER;
    case HYBBX_TRANSPORT_CIRCUIT:
    default:
        return -1;
    }
}

static void bw_collect_visitor(hybbx_session_t *session, void *userdata)
{
    bw_collect_ctx_t *ctx = (bw_collect_ctx_t *)userdata;
    int priority;

    if (ctx == NULL || session == NULL) {
        return;
    }

    priority = session_sacrifice_priority(session);
    if (priority < 0 || !hybbx_session_logged_in(session)) {
        return;
    }

    if (ctx->count >= BW_USER_MAX) {
        return;
    }

    ctx->entries[ctx->count].session = session;
    ctx->entries[ctx->count].connected_at = hybbx_session_connected_at(session);
    ctx->entries[ctx->count].sacrifice_priority = priority;
    ctx->count++;
}

static void bw_sort_victims_first(bw_collect_ctx_t *ctx)
{
    unsigned i;
    unsigned j;

    if (ctx == NULL) {
        return;
    }

    /* Lowest sacrifice_priority first (AX.25), then newest connected_at. */
    for (i = 0; i + 1 < ctx->count; i++) {
        for (j = i + 1; j < ctx->count; j++) {
            int swap = 0;

            if (ctx->entries[j].sacrifice_priority <
                ctx->entries[i].sacrifice_priority) {
                swap = 1;
            } else if (ctx->entries[j].sacrifice_priority ==
                       ctx->entries[i].sacrifice_priority) {
                if (ctx->entries[j].connected_at >
                    ctx->entries[i].connected_at) {
                    swap = 1;
                } else if (ctx->entries[j].connected_at ==
                           ctx->entries[i].connected_at &&
                           hybbx_session_id(ctx->entries[j].session) >
                           hybbx_session_id(ctx->entries[i].session)) {
                    swap = 1;
                }
            }

            if (swap) {
                bw_user_entry_t tmp = ctx->entries[i];

                ctx->entries[i] = ctx->entries[j];
                ctx->entries[j] = tmp;
            }
        }
    }
}

static hybbx_session_t *bw_first_unpaused(const bw_collect_ctx_t *ctx)
{
    unsigned i;

    if (ctx == NULL) {
        return NULL;
    }

    for (i = 0; i < ctx->count; i++) {
        if (!hybbx_session_bandwidth_paused(ctx->entries[i].session)) {
            return ctx->entries[i].session;
        }
    }

    return NULL;
}

static hybbx_session_t *bw_first_paused(const bw_collect_ctx_t *ctx)
{
    unsigned i;

    if (ctx == NULL) {
        return NULL;
    }

    for (i = 0; i < ctx->count; i++) {
        if (hybbx_session_bandwidth_paused(ctx->entries[i].session)) {
            return ctx->entries[i].session;
        }
    }

    return NULL;
}

unsigned hybbx_bandwidth_policy_user_count(hybbx_service_t *service)
{
    bw_collect_ctx_t ctx;

    if (service == NULL) {
        return 0;
    }

    memset(&ctx, 0, sizeof(ctx));
    hybbx_service_visit_sessions(service, bw_collect_visitor, &ctx);
    return ctx.count;
}

unsigned hybbx_bandwidth_policy_apply(hybbx_service_t *service,
                                      hybbx_circuit_balance_action_t action)
{
    bw_collect_ctx_t ctx;
    unsigned i;
    unsigned affected = 0;

    if (service == NULL) {
        return 0;
    }

    memset(&ctx, 0, sizeof(ctx));
    hybbx_service_visit_sessions(service, bw_collect_visitor, &ctx);
    if (ctx.count == 0) {
        return 0;
    }

    bw_sort_victims_first(&ctx);

    switch (action) {
    case HYBBX_CIRCUIT_BAL_PAUSE: {
        hybbx_session_t *target = bw_first_unpaused(&ctx);

        if (target != NULL) {
            hybbx_session_set_bandwidth_paused(target, 1);
            hybbx_log_stats("[bandwidth] paused user %s (QoS: AX.25 before TCP)",
                   hybbx_session_display_name(target));
            affected = 1;
        }
        break;
    }
    case HYBBX_CIRCUIT_BAL_BREAK: {
        hybbx_session_t *target = bw_first_paused(&ctx);

        if (target == NULL && ctx.count > 0) {
            target = ctx.entries[0].session;
        }

        if (target != NULL) {
            hybbx_log_stats("[bandwidth] disconnecting user %s (break)",
                   hybbx_session_display_name(target));
            hybbx_session_disconnect_bandwidth(target);
            affected = 1;
        }
        break;
    }
    case HYBBX_CIRCUIT_BAL_CANCEL:
        for (i = 0; i + 1 < ctx.count; i++) {
            hybbx_log_stats("[bandwidth] disconnecting user %s (cancel)",
                   hybbx_session_display_name(ctx.entries[i].session));
            hybbx_session_disconnect_bandwidth(ctx.entries[i].session);
            affected++;
        }
        break;
    case HYBBX_CIRCUIT_BAL_RESUME:
        for (i = 0; i < ctx.count; i++) {
            hybbx_session_set_bandwidth_paused(ctx.entries[i].session, 0);
        }
        affected = ctx.count;
        break;
    case HYBBX_CIRCUIT_BAL_NONE:
    default:
        break;
    }

    return affected;
}
