#include "hybbx/session.h"
#include "hybbx/service.h"
#include "hybbx/command.h"
#include "hybbx/storage.h"
#include "hybbx/texts.h"
#include "hybbx/auth.h"
#include "hybbx/chat.h"
#include "hybbx/terminal.h"
#include "hybbx/traffic.h"
#include "hybbx/hybbx.h"
#include "hybbx/limits.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

static int session_str_ieq(const char *a, const char *b)
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

typedef struct hybbx_session_core {
    hybbx_session_t pub;
    hybbx_session_record_t record;
    hybbx_user_record_t user;
    hybbx_service_t *service;
    char line_buf[HYBBX_LINE_MAX];
    size_t line_len;
    int expect_lf;
    int logged_in;
    time_t guest_expires_at;
    hybbx_session_area_t area;
    unsigned chat_channel;
    int chat_max_notice_shown;
    unsigned out_col;
} hybbx_session_core_t;

static unsigned session_chat_message_max(const hybbx_session_core_t *core)
{
    const hybbx_chat_config_t *chat;

    if (core == NULL || core->service == NULL) {
        return HYBBX_CHAT_MESSAGE_MAX;
    }

    chat = hybbx_service_get_chat(core->service);
    if (chat == NULL) {
        return HYBBX_CHAT_MESSAGE_MAX;
    }

    return chat->message_max;
}

static void session_arm_guest_timer(hybbx_session_core_t *core)
{
    unsigned timeout_sec;

    if (core == NULL || !hybbx_user_level_is_guest(core->user.level)) {
        if (core != NULL) {
            core->guest_expires_at = 0;
        }
        return;
    }

    timeout_sec = hybbx_service_guest_timeout_seconds(core->service);
    if (timeout_sec == 0) {
        core->guest_expires_at = 0;
        return;
    }

    core->guest_expires_at = time(NULL) + (time_t)timeout_sec;
}

static hybbx_result_t session_check_guest_expiry(hybbx_session_core_t *core)
{
    time_t now;

    if (core == NULL || core->guest_expires_at == 0) {
        return HYBBX_OK;
    }

    now = time(NULL);
    if (now < core->guest_expires_at) {
        return HYBBX_OK;
    }

    hybbx_session_write_line(&core->pub, "Guest time limit. Goodbye.");
    return HYBBX_SESSION_END;
}

static void session_process_line(hybbx_session_core_t *core, const char *line);

hybbx_result_t hybbx_session_write(hybbx_session_t *session, const char *text)
{
    hybbx_session_core_t *core;

    if (session == NULL || text == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    return hybbx_traffic_emit(session,
                                core != NULL ? &core->out_col : NULL,
                                text, strlen(text));
}

hybbx_result_t hybbx_session_write_line(hybbx_session_t *session,
                                        const char *text)
{
    hybbx_result_t rc;

    if (text != NULL) {
        rc = hybbx_session_write(session, text);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    return hybbx_session_write(session, "\n");
}

hybbx_result_t hybbx_session_show_prompt(hybbx_session_t *session)
{
    hybbx_session_core_t *core;
    const char *prompt;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL || !core->logged_in) {
        return HYBBX_ERR_INVALID;
    }

    prompt = hybbx_service_get_prompt(core->service);
    if (prompt == NULL || prompt[0] == '\0') {
        return HYBBX_OK;
    }

    return hybbx_session_write(session, prompt);
}

static void session_submit_line(hybbx_session_core_t *core)
{
    core->line_buf[core->line_len] = '\0';
    session_process_line(core, core->line_buf);
    core->line_len = 0;
    if (core->logged_in && core->area != HYBBX_AREA_CHAT) {
        hybbx_session_show_prompt(&core->pub);
    }
}

static void session_erase_char(hybbx_session_core_t *core)
{
    if (core->line_len > 0) {
        core->line_len--;
    }
}

static hybbx_result_t session_handle_user_byte(hybbx_session_core_t *core,
                                               unsigned char byte)
{
    char ch = (char)byte;

    if (ch == '\0') {
        return HYBBX_OK;
    }

    if (ch == '\b' || ch == 127) {
        session_erase_char(core);
        return HYBBX_OK;
    }

    if (ch == '\r') {
        session_submit_line(core);
        if (!core->logged_in) {
            return HYBBX_SESSION_END;
        }
        core->expect_lf = 1;
        return HYBBX_OK;
    }

    if (ch == '\n') {
        if (core->expect_lf) {
            core->expect_lf = 0;
            return HYBBX_OK;
        }

        session_submit_line(core);
        if (!core->logged_in) {
            return HYBBX_SESSION_END;
        }
        return HYBBX_OK;
    }

    core->expect_lf = 0;

    {
        size_t line_max = sizeof(core->line_buf) - 1;

        if (core->area == HYBBX_AREA_CHAT) {
            line_max = session_chat_message_max(core);
        }

        if (core->line_len < line_max) {
            core->line_buf[core->line_len++] = ch;
        }
    }

    return HYBBX_OK;
}

static void session_send_banner(hybbx_session_core_t *core)
{
    const hybbx_texts_config_t *texts;
    const char *service_name;

    texts = hybbx_service_get_texts(core->service);
    if (texts == NULL) {
        return;
    }

    service_name = hybbx_service_get_name(core->service);
    hybbx_texts_send_banner(texts, &core->pub, HYBBX_VERSION_STRING, service_name);
}

static void session_send_motd(hybbx_session_core_t *core)
{
    const hybbx_texts_config_t *texts;
    char welcome[HYBBX_USER_NAME_MAX + 16];

    texts = hybbx_service_get_texts(core->service);
    if (texts == NULL) {
        return;
    }

    if (hybbx_texts_send_file(texts, &core->pub, HYBBX_TEXT_MOTD) != HYBBX_OK) {
        hybbx_session_write_line(&core->pub, "(no motd available)");
    }

    snprintf(welcome, sizeof(welcome), "Welcome %s.", core->record.username);
    hybbx_session_write_line(&core->pub, welcome);
}

static void session_process_line(hybbx_session_core_t *core, const char *line)
{
    hybbx_parsed_command_t cmd;
    hybbx_command_scope_t scope;
    hybbx_result_t rc;

    if (line[0] == '\0') {
        return;
    }

    scope = hybbx_command_classify(line);
    if (scope == HYBBX_CMD_SCOPE_COMMENT) {
        return;
    }

    if (core->area == HYBBX_AREA_CHAT && scope == HYBBX_CMD_SCOPE_LOCAL) {
        if (hybbx_session_is_guest(&core->pub)) {
            hybbx_session_write_line(&core->pub, "Guests cannot use chat.");
            return;
        }

        if (strlen(line) > session_chat_message_max(core)) {
            hybbx_session_write_line(&core->pub, "Message too long.");
            return;
        }

        (void)hybbx_chat_post(core->service, &core->pub, line);
        return;
    }

    if (scope == HYBBX_CMD_SCOPE_LOCAL) {
        return;
    }

    rc = hybbx_command_parse(line, &cmd);
    if (rc != HYBBX_OK) {
        hybbx_session_write_line(&core->pub, "Command parse error.");
        return;
    }

    rc = hybbx_command_dispatch(core->service, &core->pub, &cmd);
    if (rc == HYBBX_SESSION_END) {
        core->logged_in = 0;
    } else if (rc == HYBBX_ERR_DENIED) {
        /* access message already sent */
    } else if (rc != HYBBX_OK && rc != HYBBX_ERR_NOT_FOUND) {
        hybbx_session_write_line(&core->pub, "Command failed.");
    }

    hybbx_command_free(&cmd);
}

hybbx_result_t hybbx_session_open(hybbx_service_t *service,
                                  const hybbx_transport_plugin_t *transport,
                                  void *transport_data,
                                  hybbx_session_t **out)
{
    hybbx_session_core_t *core;
    hybbx_storage_t *storage;
    const hybbx_auth_config_t *auth;
    hybbx_result_t rc;

    if (service == NULL || transport == NULL || out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_service_acquire_node(service);
    if (rc != HYBBX_OK) {
        return rc;
    }

    storage = hybbx_service_get_storage(service);
    if (storage == NULL) {
        hybbx_service_release_node(service);
        return HYBBX_ERR_INVALID;
    }

    auth = hybbx_service_get_auth(service);
    if (auth == NULL) {
        hybbx_service_release_node(service);
        return HYBBX_ERR_INVALID;
    }

    core = calloc(1, sizeof(*core));
    if (core == NULL) {
        hybbx_service_release_node(service);
        return HYBBX_ERR_NOMEM;
    }

    core->service = service;
    core->pub.transport = transport;
    core->pub.transport_data = transport_data;
    core->pub.core_data = core;
    core->area = HYBBX_AREA_MAIN;

    (void)hybbx_term_init_session(&core->pub);

    session_send_banner(core);

    if (auth->auto_login) {
        rc = hybbx_storage_create_guest(storage, &core->user);
        if (rc != HYBBX_OK) {
            free(core);
            hybbx_service_release_node(service);
            return rc;
        }

        rc = hybbx_storage_session_begin(storage, &core->user,
                                         transport->name, &core->record);
        if (rc != HYBBX_OK) {
            free(core);
            hybbx_service_release_node(service);
            return rc;
        }

        core->logged_in = 1;
        session_arm_guest_timer(core);
        printf("[session] auto-login %s on %s (session %llu)\n",
               core->record.username, transport->name,
               (unsigned long long)core->record.session_id);

        session_send_motd(core);
        hybbx_session_show_prompt(&core->pub);
    }

    *out = &core->pub;

    if (hybbx_service_attach_session(service, &core->pub) != HYBBX_OK) {
        if (core->record.session_id != 0) {
            hybbx_storage_session_end(storage, core->record.session_id);
        }
        free(core);
        hybbx_service_release_node(service);
        return HYBBX_ERR_NOMEM;
    }

    return HYBBX_OK;
}

void hybbx_session_close(hybbx_session_t *session)
{
    hybbx_session_core_t *core;
    hybbx_storage_t *storage;

    if (session == NULL) {
        return;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return;
    }

    storage = hybbx_service_get_storage(core->service);
    if (storage != NULL && core->record.session_id != 0) {
        hybbx_storage_session_end(storage, core->record.session_id);
        printf("[session] %s disconnected (session %llu)\n",
               core->record.username,
               (unsigned long long)core->record.session_id);
    }

    if (core->service != NULL) {
        hybbx_service_detach_session(core->service, session);
        hybbx_service_release_node(core->service);
    }

    free(core);
}

hybbx_result_t hybbx_session_switch_user(hybbx_session_t *session,
                                         const hybbx_user_record_t *user)
{
    hybbx_session_core_t *core;
    hybbx_storage_t *storage;
    hybbx_session_record_t new_record;
    const char *transport_name;
    hybbx_result_t rc;

    if (session == NULL || user == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (hybbx_user_level_is_guest(user->level)) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL || core->service == NULL) {
        return HYBBX_ERR_INVALID;
    }

    storage = hybbx_service_get_storage(core->service);
    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    transport_name = core->record.transport;
    if (transport_name[0] == '\0' && session->transport != NULL) {
        transport_name = session->transport->name;
    }
    if (transport_name == NULL || transport_name[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    if (core->record.session_id != 0) {
        hybbx_storage_session_end(storage, core->record.session_id);
    }

    rc = hybbx_storage_session_begin(storage, user, transport_name, &new_record);
    if (rc != HYBBX_OK) {
        return rc;
    }

    core->user = *user;
    core->record = new_record;
    core->guest_expires_at = 0;
    core->logged_in = 1;

    printf("[session] switched to %s on %s (session %llu)\n",
           core->record.username, transport_name,
           (unsigned long long)core->record.session_id);

    return HYBBX_OK;
}

hybbx_result_t hybbx_session_tick(hybbx_session_t *session)
{
    hybbx_session_core_t *core;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL || !core->logged_in) {
        return HYBBX_ERR_INVALID;
    }

    return session_check_guest_expiry(core);
}

hybbx_result_t hybbx_session_handle_input(hybbx_session_t *session,
                                         const uint8_t *data, size_t len)
{
    hybbx_session_core_t *core;
    size_t i;
    hybbx_result_t expiry_rc;

    if (session == NULL || data == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL || !core->logged_in) {
        return HYBBX_ERR_INVALID;
    }

    expiry_rc = session_check_guest_expiry(core);
    if (expiry_rc != HYBBX_OK) {
        return expiry_rc;
    }

    for (i = 0; i < len; i++) {
        hybbx_result_t rc = session_handle_user_byte(core, data[i]);

        if (rc == HYBBX_SESSION_END) {
            return HYBBX_SESSION_END;
        }
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    return HYBBX_OK;
}

const char *hybbx_session_username(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return "";
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return "";
    }

    return core->record.username;
}

uint64_t hybbx_session_id(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return 0;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return 0;
    }

    return core->record.session_id;
}

const hybbx_session_record_t *hybbx_session_record(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return NULL;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return NULL;
    }

    return &core->record;
}

hybbx_user_level_t hybbx_session_user_level(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return HYBBX_LEVEL_GUEST;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return HYBBX_LEVEL_GUEST;
    }

    return core->user.level;
}

int hybbx_session_is_guest(const hybbx_session_t *session)
{
    return hybbx_user_level_is_guest(hybbx_session_user_level(session));
}

hybbx_session_area_t hybbx_session_area(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return HYBBX_AREA_MAIN;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return HYBBX_AREA_MAIN;
    }

    return core->area;
}

const char *hybbx_session_area_name(hybbx_session_area_t area)
{
    switch (area) {
    case HYBBX_AREA_MAIN:
        return "main";
    case HYBBX_AREA_MAIL:
        return "mail";
    case HYBBX_AREA_CHAT:
        return "chat";
    default:
        return "main";
    }
}

hybbx_session_area_t hybbx_session_area_parse(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return HYBBX_AREA_MAIN;
    }

    if (session_str_ieq(name, "main")) {
        return HYBBX_AREA_MAIN;
    }

    if (session_str_ieq(name, "mail")) {
        return HYBBX_AREA_MAIL;
    }

    if (session_str_ieq(name, "chat")) {
        return HYBBX_AREA_CHAT;
    }

    return HYBBX_AREA_MAIN;
}

hybbx_result_t hybbx_session_enter_area(hybbx_session_t *session,
                                        hybbx_session_area_t area)
{
    hybbx_session_core_t *core;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core->area = area;
    return HYBBX_OK;
}

hybbx_result_t hybbx_session_leave_area(hybbx_session_t *session)
{
    hybbx_session_core_t *core;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (core->area == HYBBX_AREA_MAIN) {
        return HYBBX_OK;
    }

    core->area = HYBBX_AREA_MAIN;
    core->chat_channel = 0;
    return HYBBX_OK;
}

hybbx_result_t hybbx_session_join_chat_channel(hybbx_session_t *session,
                                               unsigned channel_index)
{
    hybbx_session_core_t *core;
    const hybbx_chat_config_t *chat;
    const char *name;

    if (session == NULL || channel_index == 0) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL || core->service == NULL) {
        return HYBBX_ERR_INVALID;
    }

    chat = hybbx_service_get_chat(core->service);
    name = hybbx_chat_channel_name(chat, channel_index);
    if (name == NULL) {
        return HYBBX_ERR_NOT_FOUND;
    }

    core->area = HYBBX_AREA_CHAT;
    core->chat_channel = channel_index;

    if (!core->chat_max_notice_shown) {
        char notice[96];

        snprintf(notice, sizeof(notice), "Max %u chars per message.",
                 chat->message_max);
        hybbx_session_write_line(session, notice);
        core->chat_max_notice_shown = 1;
    }

    return HYBBX_OK;
}

unsigned hybbx_session_chat_channel(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return 0;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL || core->area != HYBBX_AREA_CHAT) {
        return 0;
    }

    return core->chat_channel;
}
