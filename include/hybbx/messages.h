#ifndef HYBBX_MESSAGES_H
#define HYBBX_MESSAGES_H

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_session;

/** Automatic system notices (login, mail, timeouts, conference status, …). */
#define HYBBX_MSG_PREFIX_SYSTEM "*** "

/** Sysop / granted manual system-wide notices (`/broadcast`, `/announce`). */
#define HYBBX_MSG_PREFIX_SYSOP "+++ "

/** Private directed messages (system or Sysop+granted); v1 half-stub. */
#define HYBBX_MSG_PREFIX_PRIVATE "### "

/**
 * Format a system line: `*** <body>`
 */
hybbx_result_t hybbx_msg_format_system(char *out, size_t out_len,
                                     const char *body);

/**
 * Format a Sysop/granted system-wide line: `+++ <from>: <body>`
 * @p from_label may be NULL → `+++ <body>`
 */
hybbx_result_t hybbx_msg_format_sysop(char *out, size_t out_len,
                                      const char *from_label,
                                      const char *body);

/**
 * Format a private line: `### <from>: <body>` or `### <body>` when @p from_label is NULL.
 */
hybbx_result_t hybbx_msg_format_private(char *out, size_t out_len,
                                        const char *from_label,
                                        const char *body);

hybbx_result_t hybbx_msg_send_system(struct hybbx_session *session,
                                     const char *body);

hybbx_result_t hybbx_msg_send_sysop(struct hybbx_session *session,
                                    const char *from_label,
                                    const char *body);

/** Half-stub: directed private message to one session. */
hybbx_result_t hybbx_msg_send_private(struct hybbx_session *session,
                                      const char *from_label,
                                      const char *body);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_MESSAGES_H */
