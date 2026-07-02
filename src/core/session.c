#include "hybbx/session.h"
#include "hybbx/service.h"
#include "hybbx/command.h"
#include "hybbx/storage.h"
#include "hybbx/texts.h"
#include "hybbx/auth.h"
#include "hybbx/chat.h"
#include "hybbx/mail.h"
#include "hybbx/terminal.h"
#include "hybbx/traffic.h"
#include "hybbx/log.h"
#include "hybbx/hybbx.h"
#include "hybbx/limits.h"
#include "hybbx/util.h"

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
    int login_prompt;
    unsigned guest_slot;
    time_t guest_expires_at;
    hybbx_session_area_t area;
    hybbx_session_area_t area_stack[HYBBX_AREA_STACK_MAX];
    unsigned area_depth;
    unsigned chat_channel;
    int chat_max_notice_shown;
    int mail_composing;
    char mail_compose_to[HYBBX_USER_NAME_MAX];
    char mail_compose_subject[HYBBX_MAIL_SUBJECT_MAX + 1];
    char mail_compose_body[HYBBX_MAIL_BODY_MAX + 1];
    size_t mail_compose_body_len;
    unsigned out_col;
    int input_echo;
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

static void session_area_clear_state(hybbx_session_core_t *core,
                                   hybbx_session_area_t area)
{
    if (core == NULL) {
        return;
    }

    if (area == HYBBX_AREA_CHAT) {
        core->chat_channel = 0;
    }

    if (area == HYBBX_AREA_MAIL) {
        core->mail_composing = 0;
        core->mail_compose_body[0] = '\0';
        core->mail_compose_body_len = 0;
        core->mail_compose_to[0] = '\0';
        core->mail_compose_subject[0] = '\0';
    }
}

static void session_area_sync(hybbx_session_core_t *core)
{
    if (core == NULL || core->area_depth == 0) {
        return;
    }

    core->area = core->area_stack[core->area_depth - 1];
}

static hybbx_result_t session_area_push(hybbx_session_core_t *core,
                                        hybbx_session_area_t area)
{
    if (core == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (core->area_depth >= HYBBX_AREA_STACK_MAX) {
        return HYBBX_ERR_BUSY;
    }

    if (core->area_depth > 0 &&
        core->area_stack[core->area_depth - 1] == area) {
        core->area = area;
        return HYBBX_OK;
    }

    core->area_stack[core->area_depth] = area;
    core->area_depth++;
    core->area = area;
    return HYBBX_OK;
}

static void session_release_guest_slot(hybbx_session_core_t *core)
{
    if (core == NULL || core->guest_slot == 0 || core->service == NULL) {
        return;
    }

    hybbx_service_guest_release(core->service, core->guest_slot);
    core->guest_slot = 0;
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

static hybbx_result_t session_activate_guest(hybbx_session_core_t *core,
                                             const hybbx_user_record_t *guest,
                                             unsigned guest_slot)
{
    hybbx_storage_t *storage;
    const char *transport_name;
    hybbx_result_t rc;

    if (core == NULL || guest == NULL || guest_slot < 1) {
        return HYBBX_ERR_INVALID;
    }

    storage = hybbx_service_get_storage(core->service);
    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (core->guest_slot != 0 && core->guest_slot != guest_slot) {
        session_release_guest_slot(core);
    }

    transport_name = core->pub.transport != NULL ?
                     core->pub.transport->name : "unknown";

    if (core->record.session_id != 0) {
        hybbx_storage_session_end(storage, core->record.session_id);
        memset(&core->record, 0, sizeof(core->record));
    }

    rc = hybbx_storage_session_begin(storage, guest, transport_name,
                                     &core->record);
    if (rc != HYBBX_OK) {
        hybbx_service_guest_release(core->service, guest_slot);
        return rc;
    }

    core->user = *guest;
    core->guest_slot = guest_slot;
    core->logged_in = 1;
    core->login_prompt = 0;
    session_arm_guest_timer(core);
    return HYBBX_OK;
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

static int session_accepts_input(const hybbx_session_core_t *core)
{
    return core != NULL && (core->logged_in != 0 || core->login_prompt != 0);
}

hybbx_result_t hybbx_session_show_prompt(hybbx_session_t *session)
{
    hybbx_session_core_t *core;
    const char *prompt;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (!core->logged_in && !core->login_prompt) {
        return HYBBX_ERR_INVALID;
    }

    prompt = hybbx_service_get_prompt(core->service);
    if (prompt == NULL || prompt[0] == '\0') {
        return HYBBX_OK;
    }

    return hybbx_session_write(session, prompt);
}

hybbx_result_t hybbx_session_clear_terminal(hybbx_session_t *session)
{
    hybbx_session_core_t *core;
    hybbx_result_t rc;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL || !session_accepts_input(core)) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_term_clear_screen(session);
    if (rc != HYBBX_OK) {
        return rc;
    }

    core->line_len = 0;
    core->line_buf[0] = '\0';
    core->out_col = 0;
    core->expect_lf = 0;

    return hybbx_session_show_prompt(session);
}

int hybbx_session_input_echo(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return 0;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return 0;
    }

    return core->input_echo != 0;
}

hybbx_result_t hybbx_session_set_input_echo(hybbx_session_t *session, int enabled)
{
    hybbx_session_core_t *core;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL || !session_accepts_input(core)) {
        return HYBBX_ERR_INVALID;
    }

    core->input_echo = enabled ? 1 : 0;
    return HYBBX_OK;
}

static void session_submit_line(hybbx_session_core_t *core)
{
    core->line_buf[core->line_len] = '\0';
    session_process_line(core, core->line_buf);
    core->line_len = 0;
    if ((core->logged_in || core->login_prompt) &&
        core->area != HYBBX_AREA_CHAT &&
        !(core->area == HYBBX_AREA_MAIL && core->mail_composing)) {
        hybbx_session_show_prompt(&core->pub);
    }
}

static void session_erase_char(hybbx_session_core_t *core)
{
    if (core->line_len > 0) {
        core->line_len--;
    }
}

static int session_char_echoable(char ch)
{
    unsigned char byte = (unsigned char)ch;

    return byte >= 0x20 && byte != 0x7f;
}

static hybbx_result_t session_echo_backspace(hybbx_session_core_t *core)
{
    hybbx_result_t rc;

    if (core == NULL || !core->input_echo) {
        return HYBBX_OK;
    }

    rc = hybbx_session_write(&core->pub, "\b \b");
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (core->out_col > 0) {
        core->out_col--;
    }

    return HYBBX_OK;
}

static hybbx_result_t session_echo_char(hybbx_session_core_t *core, char ch)
{
    char buf[2];

    if (core == NULL || !core->input_echo || !session_char_echoable(ch)) {
        return HYBBX_OK;
    }

    buf[0] = ch;
    buf[1] = '\0';
    return hybbx_session_write(&core->pub, buf);
}

static hybbx_result_t session_echo_newline(hybbx_session_core_t *core)
{
    if (core == NULL || !core->input_echo) {
        return HYBBX_OK;
    }

    return hybbx_session_write(&core->pub, "\n");
}

static hybbx_result_t session_handle_user_byte(hybbx_session_core_t *core,
                                               unsigned char byte)
{
    char ch = (char)byte;

    if (ch == '\0') {
        return HYBBX_OK;
    }

    if (ch == '\b' || ch == 127) {
        if (core->line_len > 0) {
            hybbx_result_t rc = session_echo_backspace(core);

            if (rc != HYBBX_OK) {
                return rc;
            }
        }
        session_erase_char(core);
        return HYBBX_OK;
    }

    if (ch == '\r') {
        {
            hybbx_result_t rc = session_echo_newline(core);

            if (rc != HYBBX_OK) {
                return rc;
            }
        }
        session_submit_line(core);
        if (!core->logged_in && !core->login_prompt) {
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

        {
            hybbx_result_t rc = session_echo_newline(core);

            if (rc != HYBBX_OK) {
                return rc;
            }
        }

        session_submit_line(core);
        if (!core->logged_in && !core->login_prompt) {
            return HYBBX_SESSION_END;
        }
        return HYBBX_OK;
    }

    core->expect_lf = 0;

    {
        size_t line_max = sizeof(core->line_buf) - 1;

        if (core->area == HYBBX_AREA_CHAT) {
            line_max = session_chat_message_max(core);
        } else if (core->area == HYBBX_AREA_MAIL && core->mail_composing) {
            line_max = HYBBX_LINE_MAX - 1;
        }

        if (core->line_len < line_max) {
            hybbx_result_t rc;

            core->line_buf[core->line_len++] = ch;
            rc = session_echo_char(core, ch);
            if (rc != HYBBX_OK) {
                core->line_len--;
                return rc;
            }
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

    if (core->area == HYBBX_AREA_MAIL && core->mail_composing &&
        scope == HYBBX_CMD_SCOPE_LOCAL) {
        const hybbx_mail_config_t *mail;
        size_t line_len;
        size_t body_max;

        mail = hybbx_service_get_mail(core->service);
        body_max = mail != NULL ? mail->body_max : HYBBX_MAIL_BODY_MAX;
        line_len = strlen(line);

        if (core->mail_compose_body_len + line_len + 2 > body_max) {
            hybbx_session_write_line(&core->pub, "Message body too long.");
            return;
        }

        if (core->mail_compose_body_len > 0) {
            core->mail_compose_body[core->mail_compose_body_len++] = '\n';
        }
        memcpy(core->mail_compose_body + core->mail_compose_body_len,
               line, line_len + 1);
        core->mail_compose_body_len += line_len;
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
    core->area_stack[0] = HYBBX_AREA_MAIN;
    core->area_depth = 1;

    {
        const hybbx_traffic_config_t *traffic = hybbx_traffic_config_get();

        core->input_echo = traffic != NULL && traffic->input_echo;
    }

    (void)hybbx_term_init_session(&core->pub);

    session_send_banner(core);

    if (auth->auto_login) {
        unsigned guest_slot = 0;

        rc = hybbx_service_guest_assign(service, auth->guest_prefix,
                                        &core->user, &guest_slot);
        if (rc != HYBBX_OK) {
            free(core);
            hybbx_service_release_node(service);
            return rc;
        }

        rc = session_activate_guest(core, &core->user, guest_slot);
        if (rc != HYBBX_OK) {
            free(core);
            hybbx_service_release_node(service);
            return rc;
        }

        printf("[session] auto-login %s on %s (session %llu)\n",
               core->record.username, transport->name,
               (unsigned long long)core->record.session_id);
        hybbx_log_info("auto-login %s on %s (session %llu)",
                       core->record.username, transport->name,
                       (unsigned long long)core->record.session_id);

        hybbx_session_show_prompt(&core->pub);
    } else {
        core->login_prompt = 1;
        hybbx_session_show_prompt(&core->pub);
    }

    *out = &core->pub;

    if (hybbx_service_attach_session(service, &core->pub) != HYBBX_OK) {
        if (core->record.session_id != 0) {
            hybbx_storage_session_end(storage, core->record.session_id);
        }
        session_release_guest_slot(core);
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
        hybbx_log_info("%s disconnected (session %llu)",
                       core->record.username,
                       (unsigned long long)core->record.session_id);
    }

    session_release_guest_slot(core);

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

    session_release_guest_slot(core);

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
    core->login_prompt = 0;

    printf("[session] switched to %s on %s (session %llu)\n",
           core->record.username, transport_name,
           (unsigned long long)core->record.session_id);
    hybbx_log_info("switched to %s on %s (session %llu)",
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
    if (core == NULL || !session_accepts_input(core)) {
        return HYBBX_ERR_INVALID;
    }

    if (core->logged_in) {
        expiry_rc = session_check_guest_expiry(core);
        if (expiry_rc != HYBBX_OK) {
            return expiry_rc;
        }
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

const char *hybbx_session_display_name(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return "";
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return "";
    }

    return hybbx_user_display_name(&core->user);
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

hybbx_result_t hybbx_session_set_remote(hybbx_session_t *session,
                                        const char *remote)
{
    hybbx_session_core_t *core;

    if (session == NULL || remote == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_strlcpy(core->record.remote, remote, sizeof(core->record.remote));
    return HYBBX_OK;
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

int hybbx_session_logged_in(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return 0;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return 0;
    }

    return core->logged_in != 0;
}

int hybbx_session_login_prompt(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return 0;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return 0;
    }

    return core->login_prompt != 0;
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

    return session_area_push(core, area);
}

hybbx_result_t hybbx_session_leave_area(hybbx_session_t *session)
{
    hybbx_session_core_t *core;
    hybbx_session_area_t leaving;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (core->area_depth <= 1) {
        return HYBBX_OK;
    }

    leaving = core->area_stack[core->area_depth - 1];
    session_area_clear_state(core, leaving);
    core->area_depth--;
    session_area_sync(core);
    return HYBBX_OK;
}

hybbx_result_t hybbx_session_go_main(hybbx_session_t *session)
{
    hybbx_session_core_t *core;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return HYBBX_ERR_INVALID;
    }

    while (core->area_depth > 1) {
        hybbx_session_area_t leaving = core->area_stack[core->area_depth - 1];

        session_area_clear_state(core, leaving);
        core->area_depth--;
    }

    core->area_stack[0] = HYBBX_AREA_MAIN;
    core->area_depth = 1;
    core->area = HYBBX_AREA_MAIN;
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

    if (core->area != HYBBX_AREA_CHAT) {
        hybbx_result_t push_rc = session_area_push(core, HYBBX_AREA_CHAT);

        if (push_rc != HYBBX_OK) {
            return push_rc;
        }
    }

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

int hybbx_session_mail_composing(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return 0;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return 0;
    }

    return core->mail_composing;
}

hybbx_result_t hybbx_session_mail_compose_start(hybbx_session_t *session,
                                                const char *to_user,
                                                const char *subject)
{
    hybbx_session_core_t *core;
    const hybbx_mail_config_t *mail;
    hybbx_user_record_t recipient;
    hybbx_storage_t *storage;
    size_t subject_len;

    if (session == NULL || to_user == NULL || subject == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return HYBBX_ERR_INVALID;
    }

    mail = hybbx_service_get_mail(core->service);
    subject_len = strlen(subject);
    if (mail != NULL && subject_len > mail->subject_max) {
        return HYBBX_ERR_INVALID;
    }

    storage = hybbx_service_get_storage(core->service);
    if (storage != NULL &&
        hybbx_storage_resolve_user(storage, to_user, &recipient) == HYBBX_OK) {
        hybbx_strlcpy(core->mail_compose_to, recipient.username,
                      sizeof(core->mail_compose_to));
    } else {
        hybbx_strlcpy(core->mail_compose_to, to_user,
                      sizeof(core->mail_compose_to));
        hybbx_username_normalize(core->mail_compose_to);
    }
    hybbx_strlcpy(core->mail_compose_subject, subject,
                  sizeof(core->mail_compose_subject));
    core->mail_compose_body[0] = '\0';
    core->mail_compose_body_len = 0;
    core->mail_composing = 1;

    if (core->area != HYBBX_AREA_MAIL) {
        return session_area_push(core, HYBBX_AREA_MAIL);
    }

    return HYBBX_OK;
}

void hybbx_session_mail_compose_cancel(hybbx_session_t *session)
{
    hybbx_session_core_t *core;

    if (session == NULL) {
        return;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return;
    }

    core->mail_composing = 0;
    core->mail_compose_body[0] = '\0';
    core->mail_compose_body_len = 0;
    core->mail_compose_to[0] = '\0';
    core->mail_compose_subject[0] = '\0';
}

const char *hybbx_session_mail_compose_body(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return "";
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL || !core->mail_composing) {
        return "";
    }

    return core->mail_compose_body;
}

const char *hybbx_session_mail_compose_to(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return "";
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL || !core->mail_composing) {
        return "";
    }

    return core->mail_compose_to;
}

const char *hybbx_session_mail_compose_subject(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return "";
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL || !core->mail_composing) {
        return "";
    }

    return core->mail_compose_subject;
}
