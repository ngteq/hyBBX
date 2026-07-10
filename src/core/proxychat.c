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
        "Your message was not sent — chat across mains is not available yet.");
    return HYBBX_OK;
}

void hybbx_proxychat_show_banner(hybbx_session_t *session)
{
    if (session == NULL) {
        return;
    }

    hybbx_session_write_line(session,
        "Proxychat — talk with users on other mains.");
    hybbx_session_write_line(session,
        "Chat across mains is not available yet.");
    hybbx_session_write_line(session,
        "Type a line to try; /leave or /main to exit.");
}
