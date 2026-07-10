#include "hybbx/proxychat.h"
#include "hybbx/mains_proxy.h"
#include "hybbx/service.h"
#include "hybbx/session.h"

#include <stdio.h>
#include <string.h>

typedef struct proxychat_fanout_ctx {
    hybbx_session_t *from;
    const char *line;
    const char *from_address;
} proxychat_fanout_ctx_t;

static void proxychat_local_visitor(hybbx_session_t *session, void *userdata)
{
    proxychat_fanout_ctx_t *ctx = (proxychat_fanout_ctx_t *)userdata;
    char line[HYBBX_PROXYMAIL_ADDRESS_MAX + HYBBX_LINE_MAX + 8];

    if (session == NULL || ctx == NULL || ctx->line == NULL) {
        return;
    }

    if (session == ctx->from) {
        return;
    }

    if (hybbx_session_area(session) != HYBBX_AREA_PROXYCHAT) {
        return;
    }

    if (hybbx_session_is_guest(session)) {
        return;
    }

    snprintf(line, sizeof(line), "<%s> %s",
             ctx->from_address != NULL ? ctx->from_address : "?",
             ctx->line);
    hybbx_session_write_line(session, line);
}

static void proxychat_show_local(hybbx_session_t *session,
                                 const char *from_address,
                                 const char *line)
{
    char out[HYBBX_PROXYMAIL_ADDRESS_MAX + HYBBX_LINE_MAX + 8];

    snprintf(out, sizeof(out), "<%s> %s",
             from_address != NULL ? from_address : "?", line);
    hybbx_session_write_line(session, out);
}

hybbx_result_t hybbx_proxychat_post(hybbx_service_t *service,
                                    hybbx_session_t *session,
                                    const char *line)
{
    char from_address[HYBBX_PROXYMAIL_ADDRESS_MAX];
    proxychat_fanout_ctx_t ctx;
    hybbx_result_t rc;

    if (session == NULL || line == NULL || line[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    if (service == NULL) {
        return HYBBX_ERR_INVALID;
    }

    snprintf(from_address, sizeof(from_address), "%s@%s",
             hybbx_session_display_name(session),
             hybbx_service_get_name(service));

    proxychat_show_local(session, from_address, line);

    if (!hybbx_mains_proxy_mesh_active()) {
        hybbx_session_write_line(session,
            "Mains proxy is not running — message shown locally only.");
        return HYBBX_OK;
    }

    ctx.from = session;
    ctx.line = line;
    ctx.from_address = from_address;
    hybbx_service_visit_sessions(service, proxychat_local_visitor, &ctx);

    rc = hybbx_mains_proxy_send_chat(service, from_address, line);
    if (rc == HYBBX_ERR_UNSUPPORTED) {
        hybbx_session_write_line(session,
            "No outbound peer links — message shown locally only.");
        return HYBBX_OK;
    }
    if (rc == HYBBX_ERR_NOT_FOUND) {
        hybbx_session_write_line(session,
            "No peer links available to relay your message.");
        return HYBBX_OK;
    }

    return rc;
}

void hybbx_proxychat_receive(hybbx_service_t *service,
                             const char *from_address,
                             const char *line)
{
    proxychat_fanout_ctx_t ctx;

    if (service == NULL || line == NULL || line[0] == '\0') {
        return;
    }

    ctx.from = NULL;
    ctx.line = line;
    ctx.from_address = from_address;
    hybbx_service_visit_sessions(service, proxychat_local_visitor, &ctx);
}

void hybbx_proxychat_show_banner(hybbx_session_t *session)
{
    if (session == NULL) {
        return;
    }

    hybbx_session_write_line(session,
        "Proxychat — talk with users on linked mains.");
    if (hybbx_mains_proxy_mesh_active()) {
        hybbx_session_write_line(session,
            "Type a line to send; /leave or /main to exit.");
    } else {
        hybbx_session_write_line(session,
            "No peer links active — check mains_proxy configuration.");
        hybbx_session_write_line(session,
            "Type a line to try; /leave or /main to exit.");
    }
}
