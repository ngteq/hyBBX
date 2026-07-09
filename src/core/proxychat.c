#include "hybbx/proxychat.h"
#include "hybbx/session.h"

hybbx_result_t hybbx_proxychat_post_stub(hybbx_session_t *session,
                                           const char *line)
{
    (void)line;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_session_write_line(session,
        "Proxychat stub — message not relayed (HBX/mains_proxy mesh inactive).");
    return HYBBX_OK;
}

void hybbx_proxychat_show_banner(hybbx_session_t *session)
{
    if (session == NULL) {
        return;
    }

    hybbx_session_write_line(session,
        "Proxychat (stub) — inter-Main chat via HBX/mains_proxy; not active.");
    hybbx_session_write_line(session,
        "Type chat lines here; delivery is stubbed until mesh is implemented.");
}
