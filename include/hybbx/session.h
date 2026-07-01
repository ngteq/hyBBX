#ifndef HYBBX_SESSION_H
#define HYBBX_SESSION_H

#include "hybbx/plugin.h"
#include "hybbx/storage.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_service;

typedef enum hybbx_session_area {
    HYBBX_AREA_MAIN = 1,
    HYBBX_AREA_MAIL = 2,
    HYBBX_AREA_CHAT = 3
} hybbx_session_area_t;

/**
 * Open a new connection session.
 * When auto-login is enabled (default), assigns Guest1, Guest2, … Guest111.
 */
hybbx_result_t hybbx_session_open(struct hybbx_service *service,
                                  const hybbx_transport_plugin_t *transport,
                                  void *transport_data,
                                  hybbx_session_t **out);

void hybbx_session_close(hybbx_session_t *session);

hybbx_result_t hybbx_session_handle_input(hybbx_session_t *session,
                                         const uint8_t *data, size_t len);

const char *hybbx_session_username(const hybbx_session_t *session);
uint64_t hybbx_session_id(const hybbx_session_t *session);
const hybbx_session_record_t *hybbx_session_record(const hybbx_session_t *session);
hybbx_user_level_t hybbx_session_user_level(const hybbx_session_t *session);
int hybbx_session_is_guest(const hybbx_session_t *session);

/** Write raw text to the connected client (no line ending, no prompt). */
hybbx_result_t hybbx_session_write(hybbx_session_t *session, const char *text);

/** Write a line and append CRLF. */
hybbx_result_t hybbx_session_write_line(hybbx_session_t *session,
                                        const char *text);

/**
 * Show the global input prompt when configured.
 * Default (empty prompt): no output — users type on a blank line.
 */
hybbx_result_t hybbx_session_show_prompt(hybbx_session_t *session);

/** Current session area (main, mail, chat, …). */
hybbx_session_area_t hybbx_session_area(const hybbx_session_t *session);

/** Area name: main, mail, chat. */
const char *hybbx_session_area_name(hybbx_session_area_t area);

/** Parse area name (main, mail, chat). */
hybbx_session_area_t hybbx_session_area_parse(const char *name);

/** Leave the current area and return to main. */
hybbx_result_t hybbx_session_leave_area(hybbx_session_t *session);

/** Enter an area (mail, chat, … — for future use). */
hybbx_result_t hybbx_session_enter_area(hybbx_session_t *session,
                                        hybbx_session_area_t area);

/** Switch the session to a registered (non-guest) account after login. */
hybbx_result_t hybbx_session_switch_user(hybbx_session_t *session,
                                         const hybbx_user_record_t *user);

/**
 * Periodic session maintenance (guest time limit, etc.).
 * @return HYBBX_SESSION_END when the session should be closed.
 */
hybbx_result_t hybbx_session_tick(hybbx_session_t *session);

/** Join a chat channel (enters chat area). @p channel_index is 1-based. */
hybbx_result_t hybbx_session_join_chat_channel(hybbx_session_t *session,
                                               unsigned channel_index);

/** Current chat channel (1-based), or 0 when not in chat. */
unsigned hybbx_session_chat_channel(const hybbx_session_t *session);

/** Non-zero when composing an outbound mail message. */
int hybbx_session_mail_composing(const hybbx_session_t *session);

/** Start mail compose to @p to_user with @p subject; enters mail area. */
hybbx_result_t hybbx_session_mail_compose_start(hybbx_session_t *session,
                                                const char *to_user,
                                                const char *subject);

/** Cancel an in-progress mail compose. */
void hybbx_session_mail_compose_cancel(hybbx_session_t *session);

/** Body accumulated so far during compose (NUL-terminated). */
const char *hybbx_session_mail_compose_body(const hybbx_session_t *session);

const char *hybbx_session_mail_compose_to(const hybbx_session_t *session);
const char *hybbx_session_mail_compose_subject(const hybbx_session_t *session);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_SESSION_H */
