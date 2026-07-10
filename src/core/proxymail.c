#include "hybbx/proxymail.h"
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "hybbx/util.h"

#include <stdio.h>
#include <string.h>

int hybbx_proxymail_parse_address(const char *address,
                                  char *user, size_t user_len,
                                  char *remote_service, size_t remote_len)
{
    const char *at;
    size_t ulen;
    size_t slen;

    if (user != NULL && user_len > 0) {
        user[0] = '\0';
    }
    if (remote_service != NULL && remote_len > 0) {
        remote_service[0] = '\0';
    }

    if (address == NULL || address[0] == '\0') {
        return 0;
    }

    at = strchr(address, '@');
    if (at == NULL || at == address || at[1] == '\0') {
        return 0;
    }

    ulen = (size_t)(at - address);
    slen = strlen(at + 1);
    if (ulen == 0 || slen == 0 || ulen >= HYBBX_USER_NAME_MAX ||
        slen >= HYBBX_PROXYMAIL_SERVICE_NAME_MAX) {
        return 0;
    }

    if (user != NULL && user_len > 0) {
        memcpy(user, address, ulen);
        user[ulen] = '\0';
        hybbx_username_normalize(user);
        if (user[0] == '\0') {
            return 0;
        }
    }

    if (remote_service != NULL && remote_len > 0) {
        hybbx_strlcpy(remote_service, at + 1, remote_len);
    }

    return 1;
}

static void proxymail_unavailable_notice(hybbx_session_t *session)
{
    hybbx_session_write_line(session,
        "Proxymail — send mail to user@another-main.");
    hybbx_session_write_line(session,
        "Remote delivery is not available yet.");
}

void hybbx_proxymail_list_inbox(hybbx_service_t *service,
                                hybbx_session_t *session)
{
    (void)service;
    proxymail_unavailable_notice(session);
    hybbx_session_write_line(session, "Inbox is empty.");
    hybbx_session_write_line(session,
        "Try: /proxymail send user@mainname Your subject");
}

void hybbx_proxymail_list_inbox_range(hybbx_service_t *service,
                                      hybbx_session_t *session,
                                      unsigned from, unsigned to)
{
    (void)from;
    (void)to;
    hybbx_proxymail_list_inbox(service, session);
}

hybbx_result_t hybbx_proxymail_read(hybbx_service_t *service,
                                    hybbx_session_t *session,
                                    unsigned list_index)
{
    (void)service;

    if (list_index == 0) {
        return HYBBX_ERR_INVALID;
    }

    proxymail_unavailable_notice(session);
    hybbx_session_write_line(session, "No message at that index.");
    return HYBBX_ERR_NOT_FOUND;
}

hybbx_result_t hybbx_proxymail_delete_range(hybbx_service_t *service,
                                            hybbx_session_t *session,
                                            unsigned from, unsigned to)
{
    (void)service;
    (void)from;
    (void)to;
    proxymail_unavailable_notice(session);
    hybbx_session_write_line(session, "Nothing to delete.");
    return HYBBX_OK;
}

hybbx_result_t hybbx_proxymail_recycle_empty(hybbx_service_t *service,
                                             hybbx_session_t *session)
{
    (void)service;
    proxymail_unavailable_notice(session);
    hybbx_session_write_line(session, "Recycle bin empty.");
    return HYBBX_OK;
}

hybbx_result_t hybbx_proxymail_deliver(hybbx_service_t *service,
                                       const char *from_user,
                                       const char *to_address,
                                       const char *subject,
                                       const char *body)
{
    char user[HYBBX_USER_NAME_MAX];
    char remote[HYBBX_PROXYMAIL_SERVICE_NAME_MAX];

    (void)service;
    (void)from_user;
    (void)subject;
    (void)body;

    if (!hybbx_proxymail_parse_address(to_address, user, sizeof(user),
                                       remote, sizeof(remote))) {
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_ERR_UNSUPPORTED;
}
