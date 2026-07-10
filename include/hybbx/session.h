#ifndef HYBBX_SESSION_H
#define HYBBX_SESSION_H

#include "hybbx/plugin.h"
#include "hybbx/storage.h"
#include "hybbx/mail.h"
#include "hybbx/proxymail.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_service;

typedef enum hybbx_session_area {
    HYBBX_AREA_MAIN = 1,
    HYBBX_AREA_MAIL = 2,
    HYBBX_AREA_CHAT = 3,
    HYBBX_AREA_CONFERENCE = 4,
    /** Inter-Main mail sub-area under mail (`/proxymail`, `/mail proxymail`). */
    HYBBX_AREA_PROXYMAIL = 5,
    /** Inter-Main chat sub-area under chat (`/proxychat`, `/chat proxychat`). */
    HYBBX_AREA_PROXYCHAT = 6
} hybbx_session_area_t;

/** Maximum nested area depth (main + sub-areas). */
#define HYBBX_AREA_STACK_MAX 8u

/**
 * Open a new connection session.
 * When auto-login is enabled (default), assigns the next free Guest1 … Guest25
 * slot in memory (not written to user files).
 * When auto-login is disabled, the session stays at a login prompt for
 * registered `/login` and `/register` only (no guest slots).
 */
hybbx_result_t hybbx_session_open(struct hybbx_service *service,
                                  const hybbx_transport_plugin_t *transport,
                                  void *transport_data,
                                  hybbx_session_t **out);

void hybbx_session_close(hybbx_session_t *session);

hybbx_result_t hybbx_session_handle_input(hybbx_session_t *session,
                                         const uint8_t *data, size_t len);

const char *hybbx_session_username(const hybbx_session_t *session);
/** User-facing name (nickname) for the logged-in account. */
const char *hybbx_session_display_name(const hybbx_session_t *session);
uint64_t hybbx_session_id(const hybbx_session_t *session);
const hybbx_session_record_t *hybbx_session_record(const hybbx_session_t *session);
/** Store the client peer address on the session record (for security.log). */
hybbx_result_t hybbx_session_set_remote(hybbx_session_t *session,
                                        const char *remote);
hybbx_user_level_t hybbx_session_user_level(const hybbx_session_t *session);
int hybbx_session_is_guest(const hybbx_session_t *session);
/** Non-zero when the session has completed login (guest or registered). */
int hybbx_session_logged_in(const hybbx_session_t *session);
/** Non-zero when auto_login is off: banner + prompt, registered login only. */
int hybbx_session_login_prompt(const hybbx_session_t *session);

/** Write raw text to the connected client (no line ending, no prompt). */
hybbx_result_t hybbx_session_write(hybbx_session_t *session, const char *text);

/** Write a line and append CRLF. */
hybbx_result_t hybbx_session_write_line(hybbx_session_t *session,
                                        const char *text);

/** One blank line before `/command` output (not chat/mail compose). */
hybbx_result_t hybbx_session_command_gap(hybbx_session_t *session);

/**
 * Show the global input prompt when configured.
 * Default (empty prompt): no output — users type on a blank line.
 */
hybbx_result_t hybbx_session_show_prompt(hybbx_session_t *session);

/** Clear screen and discard the current input line; show prompt when set. */
hybbx_result_t hybbx_session_clear_terminal(hybbx_session_t *session);

/** Non-zero when typed characters are echoed back to the client. */
int hybbx_session_input_echo(const hybbx_session_t *session);

/** Enable or disable per-session input echo. */
hybbx_result_t hybbx_session_set_input_echo(hybbx_session_t *session, int enabled);

/** Current session area (main, mail, chat, …). */
hybbx_session_area_t hybbx_session_area(const hybbx_session_t *session);

/** Area name: main, mail, chat. */
const char *hybbx_session_area_name(hybbx_session_area_t area);

/** Parse area name (main, mail, chat). */
hybbx_session_area_t hybbx_session_area_parse(const char *name);

/** Go up one area level (alias: /back). Clears state for the area being left. */
hybbx_result_t hybbx_session_leave_area(hybbx_session_t *session);

/** Return to main from any depth (alias: /menu). Clears all sub-area state. */
hybbx_result_t hybbx_session_go_main(hybbx_session_t *session);

/** Enter an area (pushes onto the area stack). */
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

struct hybbx_service *hybbx_session_service(hybbx_session_t *session);

/** Join a private conference (conference area). Internal — use hybbx_conference_start. */
hybbx_result_t hybbx_session_join_conference(hybbx_session_t *session,
                                             const char *topic,
                                             const char *partner_username);

void hybbx_session_clear_conference(hybbx_session_t *session);

int hybbx_session_conference_may_invite(hybbx_session_t *session,
                                        const char *target);
void hybbx_session_conference_invite_sent(hybbx_session_t *session,
                                          const char *target);
void hybbx_session_set_conference_invite(hybbx_session_t *session,
                                         const char *from_username,
                                         const char *topic);
void hybbx_session_clear_conference_invite(hybbx_session_t *session);
const char *hybbx_session_conference_invite_from(const hybbx_session_t *session);
const char *hybbx_session_conference_invite_topic(const hybbx_session_t *session);
const char *hybbx_session_conference_partner(const hybbx_session_t *session);

/** Enter local mail area (does not enter proxymail). */
hybbx_result_t hybbx_session_enter_mail(hybbx_session_t *session);

/** Enter proxymail sub-area (pushes mail when needed). */
hybbx_result_t hybbx_session_enter_proxymail(hybbx_session_t *session);

/** Enter local chat area without joining a channel. */
hybbx_result_t hybbx_session_enter_chat(hybbx_session_t *session);

/** Enter proxychat sub-area (pushes chat when needed). */
hybbx_result_t hybbx_session_enter_proxychat(hybbx_session_t *session);

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

/** Non-zero when composing proxymail (`user@remote-main` recipient). */
int hybbx_session_proxymail_composing(const hybbx_session_t *session);

hybbx_result_t hybbx_session_proxymail_compose_start(hybbx_session_t *session,
                                                     const char *to_address,
                                                     const char *subject);

void hybbx_session_proxymail_compose_cancel(hybbx_session_t *session);

const char *hybbx_session_proxymail_compose_body(const hybbx_session_t *session);
const char *hybbx_session_proxymail_compose_to(const hybbx_session_t *session);
const char *hybbx_session_proxymail_compose_subject(
    const hybbx_session_t *session);

/** Wall-clock time when this session connected (for bandwidth policy ordering). */
time_t hybbx_session_connected_at(const hybbx_session_t *session);

/** Non-zero when the session is frozen by circuit bandwidth limits. */
int hybbx_session_bandwidth_paused(const hybbx_session_t *session);

void hybbx_session_set_bandwidth_paused(hybbx_session_t *session, int paused);

/**
 * End the session after sending @ref HYBBX_BANDWIDTH_DISCONNECT_MSG.
 * Newest-first under low-bandwidth link pressure.
 */
void hybbx_session_disconnect_bandwidth(hybbx_session_t *session);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_SESSION_H */
