#include "hybbx/conference.h"
#include "hybbx/chat.h"
#include "hybbx/messages.h"
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "hybbx/storage.h"
#include "hybbx/auth.h"
#include "hybbx/traffic.h"
#include "hybbx/util.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static int str_ieq(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }

    while (*a != '\0' && *b != '\0') {
        char ca = (char)(*a >= 'A' && *a <= 'Z' ? *a + 32 : *a);
        char cb = (char)(*b >= 'A' && *b <= 'Z' ? *b + 32 : *b);

        if (ca != cb) {
            return 0;
        }
        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static int invite_reply_yes(const char *line)
{
    return line != NULL &&
           (str_ieq(line, "y") || str_ieq(line, "yes"));
}

static int invite_reply_no(const char *line)
{
    return line != NULL &&
           (str_ieq(line, "n") || str_ieq(line, "no"));
}

static int topic_char_ok(char ch)
{
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') ||
           ch == ' ' || ch == '-' || ch == '_';
}

static int conference_topic_valid(const char *topic)
{
    size_t len;
    size_t i;

    if (topic == NULL) {
        return 0;
    }

    len = strlen(topic);
    if (len < 2 || len >= HYBBX_CONFERENCE_TOPIC_MAX) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        if (!topic_char_ok(topic[i])) {
            return 0;
        }
    }

    return 1;
}

typedef struct find_user_session_ctx {
    const char *username;
    hybbx_session_t *found;
} find_user_session_ctx_t;

static void find_user_session_visitor(hybbx_session_t *session, void *userdata)
{
    find_user_session_ctx_t *ctx = (find_user_session_ctx_t *)userdata;

    if (ctx == NULL || session == NULL || ctx->found != NULL) {
        return;
    }

    if (!hybbx_session_logged_in(session) || hybbx_session_is_guest(session)) {
        return;
    }

    if (str_ieq(hybbx_session_username(session), ctx->username)) {
        ctx->found = session;
    }
}

static hybbx_session_t *conference_find_online_user(hybbx_service_t *service,
                                                    const char *username)
{
    find_user_session_ctx_t ctx;

    if (service == NULL || username == NULL || username[0] == '\0') {
        return NULL;
    }

    ctx.username = username;
    ctx.found = NULL;
    hybbx_service_visit_sessions(service, find_user_session_visitor, &ctx);
    return ctx.found;
}

static int conference_session_active(const hybbx_session_t *session)
{
    return session != NULL &&
           hybbx_session_area(session) == HYBBX_AREA_CONFERENCE;
}

typedef struct invite_cancel_ctx {
    const char *from_username;
} invite_cancel_ctx_t;

static void invite_cancel_visitor(hybbx_session_t *session, void *userdata)
{
    invite_cancel_ctx_t *ctx = (invite_cancel_ctx_t *)userdata;

    if (session == NULL || ctx == NULL || ctx->from_username == NULL) {
        return;
    }

    if (!hybbx_conference_invite_pending(session)) {
        return;
    }

    if (!str_ieq(hybbx_session_conference_invite_from(session),
                 ctx->from_username)) {
        return;
    }

    hybbx_session_clear_conference_invite(session);
    hybbx_session_write_line(session, "Conference invite cancelled.");
}

static void conference_cancel_invites_from(hybbx_service_t *service,
                                           const char *from_username)
{
    invite_cancel_ctx_t ctx;

    if (service == NULL || from_username == NULL || from_username[0] == '\0') {
        return;
    }

    ctx.from_username = from_username;
    hybbx_service_visit_sessions(service, invite_cancel_visitor, &ctx);
}

static void conference_notify_partner_left(hybbx_session_t *session,
                                         const char *partner_username)
{
    hybbx_service_t *service;
    hybbx_session_t *partner;
    char line[HYBBX_LINE_MAX];

    if (session == NULL || partner_username == NULL ||
        partner_username[0] == '\0') {
        return;
    }

    service = hybbx_session_service(session);
    if (service == NULL) {
        return;
    }

    partner = conference_find_online_user(service, partner_username);
    if (partner == NULL || !conference_session_active(partner)) {
        return;
    }

    if (!str_ieq(hybbx_session_conference_partner(partner),
                 hybbx_session_username(session))) {
        return;
    }

    hybbx_session_clear_conference(partner);
    snprintf(line, sizeof(line), "%s left the conference.",
             hybbx_session_display_name(session));
    hybbx_session_write_line(partner, line);
    if (hybbx_session_area(partner) == HYBBX_AREA_CONFERENCE) {
        (void)hybbx_session_leave_area(partner);
    }
}

static hybbx_result_t conference_activate(hybbx_service_t *service,
                                          hybbx_session_t *initiator,
                                          hybbx_session_t *partner,
                                          const char *topic)
{
    char line[HYBBX_LINE_MAX];
    hybbx_result_t rc;

    if (service == NULL || initiator == NULL || partner == NULL ||
        topic == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_session_join_conference(initiator, topic,
                                       hybbx_session_username(partner));
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = hybbx_session_join_conference(partner, topic,
                                       hybbx_session_username(initiator));
    if (rc != HYBBX_OK) {
        hybbx_session_clear_conference(initiator);
        (void)hybbx_session_leave_area(initiator);
        return rc;
    }

    snprintf(line, sizeof(line), "Conference: %s", topic);
    hybbx_session_write_line(initiator, line);
    hybbx_session_write_line(initiator,
        "Each line is a message; /leave or /main to exit.");

    snprintf(line, sizeof(line), "Conference: %s", topic);
    hybbx_session_write_line(partner, line);
    hybbx_session_write_line(partner,
        "Each line is a message; /leave or /main to exit.");

    return HYBBX_OK;
}

hybbx_result_t hybbx_conference_start(hybbx_service_t *service,
                                      hybbx_session_t *initiator,
                                      const char *topic,
                                      const char *partner_spec)
{
    hybbx_storage_t *storage;
    hybbx_user_record_t partner_user;
    hybbx_session_t *partner_session;
    char line[HYBBX_LINE_MAX];
    hybbx_result_t rc;

    if (service == NULL || initiator == NULL || topic == NULL ||
        partner_spec == NULL || topic[0] == '\0' || partner_spec[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    if (hybbx_session_is_guest(initiator)) {
        hybbx_session_write_line(initiator, "Guests cannot use conference.");
        return HYBBX_ERR_DENIED;
    }

    if (!conference_topic_valid(topic)) {
        hybbx_session_write_line(initiator, "Invalid conference topic.");
        return HYBBX_ERR_INVALID;
    }

    if (conference_session_active(initiator)) {
        hybbx_session_write_line(initiator, "Already in a conference.");
        return HYBBX_OK;
    }

    if (hybbx_conference_invite_pending(initiator)) {
        hybbx_session_write_line(initiator,
            "Answer the pending conference invite first (y/n).");
        return HYBBX_OK;
    }

    storage = hybbx_service_get_storage(service);
    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_storage_resolve_user(storage, partner_spec, &partner_user);
    if (rc == HYBBX_ERR_NOT_FOUND) {
        hybbx_session_write_line(initiator, "Unknown user.");
        return HYBBX_OK;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (!partner_user.active) {
        hybbx_session_write_line(initiator, "User is not active.");
        return HYBBX_OK;
    }

    if (hybbx_user_level_is_guest(partner_user.level)) {
        hybbx_session_write_line(initiator, "Cannot conference with a guest.");
        return HYBBX_OK;
    }

    if (str_ieq(partner_user.username, hybbx_session_username(initiator))) {
        hybbx_session_write_line(initiator, "Cannot conference with yourself.");
        return HYBBX_OK;
    }

    if (!hybbx_session_conference_may_invite(initiator, partner_user.username)) {
        hybbx_session_write_line(initiator,
            "Invite limit: 2 per user per 30 minutes.");
        return HYBBX_OK;
    }

    partner_session = conference_find_online_user(service, partner_user.username);
    if (partner_session == NULL) {
        hybbx_session_write_line(initiator, "User is not online.");
        return HYBBX_OK;
    }

    if (conference_session_active(partner_session)) {
        hybbx_session_write_line(initiator, "User is already in a conference.");
        return HYBBX_OK;
    }

    if (hybbx_conference_invite_pending(partner_session)) {
        hybbx_session_write_line(initiator,
            "User has a pending conference invite.");
        return HYBBX_OK;
    }

    hybbx_session_set_conference_invite(partner_session,
                                        hybbx_session_username(initiator),
                                        topic);
    hybbx_session_conference_invite_sent(initiator, partner_user.username);

    snprintf(line, sizeof(line), "Conference invite: %s", topic);
    (void)hybbx_msg_send_private(partner_session,
                                 hybbx_session_display_name(initiator), line);
    hybbx_session_write_line(partner_session, "Accept? y/n or yes/no");

    snprintf(line, sizeof(line), "Conference invite sent to %s.",
             hybbx_user_display_name(&partner_user));
    hybbx_session_write_line(initiator, line);

    return HYBBX_OK;
}

hybbx_result_t hybbx_conference_reply_invite(hybbx_service_t *service,
                                             hybbx_session_t *session,
                                             const char *line)
{
    char from_username[HYBBX_USER_NAME_MAX];
    char topic[HYBBX_CONFERENCE_TOPIC_MAX];
    hybbx_session_t *initiator;
    char msg[HYBBX_LINE_MAX];

    if (service == NULL || session == NULL || line == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (!hybbx_conference_invite_pending(session)) {
        return HYBBX_ERR_INVALID;
    }

    if (!invite_reply_yes(line) && !invite_reply_no(line)) {
        return HYBBX_ERR_NOT_FOUND;
    }

    {
        const char *from_ptr = hybbx_session_conference_invite_from(session);
        const char *topic_ptr = hybbx_session_conference_invite_topic(session);

        if (from_ptr == NULL || from_ptr[0] == '\0' ||
            topic_ptr == NULL || topic_ptr[0] == '\0') {
            hybbx_session_clear_conference_invite(session);
            return HYBBX_ERR_INVALID;
        }

        hybbx_strlcpy(from_username, from_ptr, sizeof(from_username));
        hybbx_strlcpy(topic, topic_ptr, sizeof(topic));
    }

    hybbx_session_clear_conference_invite(session);

    if (invite_reply_no(line)) {
        initiator = conference_find_online_user(service, from_username);
        if (initiator != NULL) {
            snprintf(msg, sizeof(msg), "%s declined the conference invite.",
                     hybbx_session_display_name(session));
            (void)hybbx_msg_send_system(initiator, msg);
        }
        hybbx_session_write_line(session, "Conference invite declined.");
        return HYBBX_OK;
    }

    if (conference_session_active(session)) {
        hybbx_session_write_line(session, "Already in a conference.");
        return HYBBX_OK;
    }

    initiator = conference_find_online_user(service, from_username);
    if (initiator == NULL) {
        hybbx_session_write_line(session, "Inviter is no longer online.");
        return HYBBX_OK;
    }

    if (conference_session_active(initiator)) {
        hybbx_session_write_line(session, "Inviter is already in a conference.");
        return HYBBX_OK;
    }

    if (conference_activate(service, initiator, session, topic) != HYBBX_OK) {
        hybbx_session_write_line(session, "Conference could not be started.");
        return HYBBX_OK;
    }

    return HYBBX_OK;
}

typedef struct conference_post_ctx {
    hybbx_session_t *from;
    const char *message;
    const char *from_user;
    const char *from_login;
} conference_post_ctx_t;

static void conference_post_visitor(hybbx_session_t *session, void *userdata)
{
    conference_post_ctx_t *ctx = (conference_post_ctx_t *)userdata;
    char line[HYBBX_USER_NAME_MAX + HYBBX_LINE_MAX + 16];
    const char *session_login;

    if (session == NULL || ctx == NULL || ctx->message == NULL ||
        ctx->from == NULL || ctx->from_login == NULL) {
        return;
    }

    if (!conference_session_active(session)) {
        return;
    }

    if (session == ctx->from) {
        snprintf(line, sizeof(line), "ME: %s", ctx->message);
        hybbx_session_write_line(session, line);
        return;
    }

    session_login = hybbx_session_username(session);
    if (session_login == NULL ||
        !str_ieq(hybbx_session_conference_partner(session), ctx->from_login) ||
        !str_ieq(hybbx_session_conference_partner(ctx->from), session_login)) {
        return;
    }

    snprintf(line, sizeof(line), "%s: %s", ctx->from_user, ctx->message);
    hybbx_session_write_line(session, line);
}

hybbx_result_t hybbx_conference_post(hybbx_service_t *service,
                                     hybbx_session_t *from,
                                     const char *message)
{
    conference_post_ctx_t ctx;
    const hybbx_chat_config_t *chat;

    if (service == NULL || from == NULL || message == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (message[0] == '\0') {
        return HYBBX_OK;
    }

    if (!conference_session_active(from)) {
        return HYBBX_ERR_INVALID;
    }

    chat = hybbx_service_get_chat(service);
    if (chat == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (strlen(message) > chat->message_max) {
        return HYBBX_ERR_INVALID;
    }

    ctx.from = from;
    ctx.message = message;
    ctx.from_user = hybbx_session_display_name(from);
    ctx.from_login = hybbx_session_username(from);
    if (ctx.from_login == NULL || ctx.from_login[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    hybbx_service_visit_sessions(service, conference_post_visitor, &ctx);
    return HYBBX_OK;
}

void hybbx_conference_area_leaving(hybbx_session_t *session)
{
    const char *partner_name;

    if (session == NULL) {
        return;
    }

    partner_name = hybbx_session_conference_partner(session);
    hybbx_session_clear_conference(session);

    if (partner_name != NULL && partner_name[0] != '\0') {
        conference_notify_partner_left(session, partner_name);
    }
}

void hybbx_conference_session_closed(hybbx_session_t *session)
{
    hybbx_service_t *service;
    const char *username;

    if (session == NULL) {
        return;
    }

    if (hybbx_conference_invite_pending(session)) {
        hybbx_session_clear_conference_invite(session);
    }

    if (!conference_session_active(session)) {
        service = hybbx_session_service(session);
        username = hybbx_session_username(session);
        if (service != NULL && username != NULL && username[0] != '\0') {
            conference_cancel_invites_from(service, username);
        }
        return;
    }

    hybbx_conference_area_leaving(session);

    service = hybbx_session_service(session);
    username = hybbx_session_username(session);
    if (service != NULL && username != NULL && username[0] != '\0') {
        conference_cancel_invites_from(service, username);
    }
}
