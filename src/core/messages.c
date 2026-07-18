#include "hybbx/messages.h"
#include "hybbx/session.h"
#include "hybbx/traffic.h"

#include <stdio.h>
#include <string.h>

static hybbx_result_t msg_format_prefixed(char *out, size_t out_len,
                                          const char *prefix,
                                          const char *from_label,
                                          const char *body)
{
    int n;

    if (out == NULL || out_len == 0 || prefix == NULL || body == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (from_label != NULL && from_label[0] != '\0') {
        n = snprintf(out, out_len, "%s%s: %s", prefix, from_label, body);
    } else {
        n = snprintf(out, out_len, "%s%s", prefix, body);
    }

    if (n < 0 || (size_t)n >= out_len) {
        return HYBBX_ERR_INVALID;
    }

    return HYBBX_OK;
}

hybbx_result_t hybbx_msg_format_system(char *out, size_t out_len,
                                       const char *body)
{
    return msg_format_prefixed(out, out_len, HYBBX_MSG_PREFIX_SYSTEM,
                               NULL, body);
}

hybbx_result_t hybbx_msg_format_sysop(char *out, size_t out_len,
                                      const char *from_label,
                                      const char *body)
{
    return msg_format_prefixed(out, out_len, HYBBX_MSG_PREFIX_SYSOP,
                               from_label, body);
}

hybbx_result_t hybbx_msg_format_private(char *out, size_t out_len,
                                        const char *from_label,
                                        const char *body)
{
    return msg_format_prefixed(out, out_len, HYBBX_MSG_PREFIX_PRIVATE,
                               from_label, body);
}

static hybbx_result_t msg_send_formatted(struct hybbx_session *session,
                                         hybbx_result_t (*format_fn)(char *,
                                                                     size_t,
                                                                     const char *,
                                                                     const char *),
                                         const char *from_label,
                                         const char *body)
{
    char line[HYBBX_LINE_MAX];
    hybbx_result_t rc;

    if (session == NULL || body == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = format_fn(line, sizeof(line), from_label, body);
    if (rc != HYBBX_OK) {
        return rc;
    }

    return hybbx_session_write_line(session, line);
}

hybbx_result_t hybbx_msg_send_system(struct hybbx_session *session,
                                     const char *body)
{
    char line[HYBBX_LINE_MAX];
    hybbx_result_t rc;

    if (session == NULL || body == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_msg_format_system(line, sizeof(line), body);
    if (rc != HYBBX_OK) {
        return rc;
    }

    return hybbx_session_write_line(session, line);
}

hybbx_result_t hybbx_msg_send_sysop(struct hybbx_session *session,
                                    const char *from_label,
                                    const char *body)
{
    return msg_send_formatted(session, hybbx_msg_format_sysop,
                              from_label, body);
}

hybbx_result_t hybbx_msg_send_private(struct hybbx_session *session,
                                      const char *from_label,
                                      const char *body)
{
    return msg_send_formatted(session, hybbx_msg_format_private,
                              from_label, body);
}
