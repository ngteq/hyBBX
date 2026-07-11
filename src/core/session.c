#include "hybbx/session.h"
#include "hybbx/service.h"
#include "hybbx/command.h"
#include "hybbx/storage.h"
#include "hybbx/texts.h"
#include "hybbx/auth.h"
#include "hybbx/chat.h"
#include "hybbx/conference.h"
#include "hybbx/mail.h"
#include "hybbx/proxymail.h"
#include "hybbx/proxychat.h"
#include "hybbx/terminal.h"
#include "hybbx/traffic.h"
#include "hybbx/log.h"
#include "hybbx/hybbx.h"
#include "hybbx/util.h"
#include "hybbx/bandwidth_policy.h"

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

static int session_console_show_guest_events(void)
{
    const hybbx_log_config_t *cfg = hybbx_log_config_get();

    if (cfg == NULL) {
        return 1;
    }

    return cfg->level <= HYBBX_LOG_STATS;
}

typedef struct hybbx_session_core {
    hybbx_session_t pub;
    hybbx_session_record_t record;
    hybbx_user_record_t user;
    hybbx_service_t *service;
    char line_buf[HYBBX_LINE_MAX];
    size_t line_len;
    size_t line_cursor;
    char history[HYBBX_HISTORY_MAX][HYBBX_LINE_MAX];
    unsigned history_count;
    unsigned history_next;
    int history_view;
    char history_saved_line[HYBBX_LINE_MAX];
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
    char conference_topic[HYBBX_CONFERENCE_TOPIC_MAX];
    char conference_partner[HYBBX_USER_NAME_MAX];
    int conference_invite_pending;
    char conference_invite_from[HYBBX_USER_NAME_MAX];
    char conference_invite_topic[HYBBX_CONFERENCE_TOPIC_MAX];
    struct {
        char target[HYBBX_USER_NAME_MAX];
        time_t sent_at[HYBBX_CONFERENCE_INVITE_MAX_PER_TARGET];
    } conference_invite_rates[8];
    int mail_composing;
    char mail_compose_to[HYBBX_USER_NAME_MAX];
    char mail_compose_subject[HYBBX_MAIL_SUBJECT_MAX + 1];
    char mail_compose_body[HYBBX_MAIL_BODY_MAX + 1];
    size_t mail_compose_body_len;
    int proxymail_composing;
    char proxymail_compose_to[HYBBX_PROXYMAIL_ADDRESS_MAX];
    char proxymail_compose_subject[HYBBX_MAIL_SUBJECT_MAX + 1];
    char proxymail_compose_body[HYBBX_MAIL_BODY_MAX + 1];
    size_t proxymail_compose_body_len;
    unsigned out_col;
    int input_echo;
    int bandwidth_paused;
    int bandwidth_disconnect;
    unsigned char esc_state;
    char csi_buf[24];
    size_t csi_len;
} hybbx_session_core_t;

#define SESSION_ESC_NONE 0u
#define SESSION_ESC_ESC  1u
#define SESSION_ESC_CSI  2u
#define SESSION_ESC_SS3 3u

#define SESSION_CSI_BUF_MAX 24u

static hybbx_result_t session_handle_user_byte(hybbx_session_core_t *core,
                                               unsigned char byte);

static size_t session_line_max_len(const hybbx_session_core_t *core);
static unsigned session_chat_message_max(const hybbx_session_core_t *core);
static hybbx_result_t session_write_transport(hybbx_session_core_t *core,
                                              const char *data, size_t len);
static hybbx_result_t session_line_refresh_suffix(hybbx_session_core_t *core);
static hybbx_result_t session_line_cursor_left(hybbx_session_core_t *core);
static hybbx_result_t session_line_cursor_right(hybbx_session_core_t *core);
static hybbx_result_t session_line_cursor_home(hybbx_session_core_t *core);
static hybbx_result_t session_line_cursor_end(hybbx_session_core_t *core);
static hybbx_result_t session_line_backspace(hybbx_session_core_t *core);
static hybbx_result_t session_line_delete_forward(hybbx_session_core_t *core);
static hybbx_result_t session_line_insert_char(hybbx_session_core_t *core,
                                               char ch);
static hybbx_result_t session_history_up(hybbx_session_core_t *core);
static hybbx_result_t session_history_down(hybbx_session_core_t *core);

static void session_escape_reset(hybbx_session_core_t *core)
{
    if (core == NULL) {
        return;
    }

    core->esc_state = SESSION_ESC_NONE;
    core->csi_len = 0;
}

static int session_csi_is_final(unsigned char ch)
{
    return ch >= 0x40u && ch <= 0x7eu;
}

static size_t session_line_max_len(const hybbx_session_core_t *core)
{
    size_t line_max = sizeof(core->line_buf) - 1;

    if (core == NULL) {
        return HYBBX_LINE_MAX - 1;
    }

    if (core->area == HYBBX_AREA_CHAT || core->area == HYBBX_AREA_CONFERENCE ||
        core->area == HYBBX_AREA_PROXYCHAT) {
        line_max = session_chat_message_max(core);
    } else if (core->area == HYBBX_AREA_MAIL && core->mail_composing) {
        line_max = HYBBX_LINE_MAX - 1;
    } else if (core->area == HYBBX_AREA_PROXYMAIL && core->proxymail_composing) {
        line_max = HYBBX_LINE_MAX - 1;
    }

    return line_max;
}

static int session_char_echoable(char ch)
{
    unsigned char byte = (unsigned char)ch;

    return byte >= 0x20 && byte != 0x7f;
}

static hybbx_result_t session_write_transport(hybbx_session_core_t *core,
                                              const char *data, size_t len)
{
    if (core == NULL || data == NULL || len == 0) {
        return HYBBX_OK;
    }

    if (core->pub.transport != NULL && core->pub.transport->write != NULL) {
        return core->pub.transport->write(&core->pub, data, len);
    }

    return HYBBX_OK;
}

static hybbx_result_t session_line_move_back(hybbx_session_core_t *core,
                                             size_t count)
{
    char buf[64];
    size_t chunk;
    size_t sent = 0;

    if (core == NULL || count == 0) {
        return HYBBX_OK;
    }

    memset(buf, '\b', sizeof(buf));

    while (sent < count) {
        hybbx_result_t rc;

        chunk = count - sent;
        if (chunk > sizeof(buf)) {
            chunk = sizeof(buf);
        }

        rc = session_write_transport(core, buf, chunk);
        if (rc != HYBBX_OK) {
            return rc;
        }
        sent += chunk;
    }

    return HYBBX_OK;
}

static hybbx_result_t session_line_refresh_suffix(hybbx_session_core_t *core)
{
    size_t suffix_len;
    hybbx_result_t rc;

    if (core == NULL || !core->input_echo) {
        return HYBBX_OK;
    }

    suffix_len = core->line_len - core->line_cursor;
    if (suffix_len > 0) {
        rc = session_write_transport(core, core->line_buf + core->line_cursor,
                                   suffix_len);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    rc = session_write_transport(core, "\x1b[K", 3);
    if (rc != HYBBX_OK) {
        return rc;
    }

    return session_line_move_back(core, suffix_len);
}

static hybbx_result_t session_line_cursor_left(hybbx_session_core_t *core)
{
    if (core == NULL || core->line_cursor == 0) {
        return HYBBX_OK;
    }

    core->line_cursor--;
    if (!core->input_echo) {
        return HYBBX_OK;
    }

    return session_write_transport(core, "\b", 1);
}

static hybbx_result_t session_line_cursor_right(hybbx_session_core_t *core)
{
    hybbx_result_t rc;

    if (core == NULL || core->line_cursor >= core->line_len) {
        return HYBBX_OK;
    }

    if (core->input_echo) {
        char out[2];

        out[0] = core->line_buf[core->line_cursor];
        out[1] = '\0';
        rc = session_write_transport(core, out, 1);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    core->line_cursor++;
    return HYBBX_OK;
}

static hybbx_result_t session_line_cursor_home(hybbx_session_core_t *core)
{
    hybbx_result_t rc = HYBBX_OK;

    if (core == NULL) {
        return HYBBX_ERR_INVALID;
    }

    while (core->line_cursor > 0) {
        rc = session_line_cursor_left(core);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    return HYBBX_OK;
}

static hybbx_result_t session_line_cursor_end(hybbx_session_core_t *core)
{
    hybbx_result_t rc = HYBBX_OK;

    if (core == NULL) {
        return HYBBX_ERR_INVALID;
    }

    while (core->line_cursor < core->line_len) {
        rc = session_line_cursor_right(core);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    return HYBBX_OK;
}

static hybbx_result_t session_line_backspace(hybbx_session_core_t *core)
{
    hybbx_result_t rc;

    if (core == NULL || core->line_cursor == 0) {
        return HYBBX_OK;
    }

    core->line_cursor--;
    memmove(core->line_buf + core->line_cursor,
            core->line_buf + core->line_cursor + 1,
            core->line_len - core->line_cursor - 1);
    core->line_len--;
    core->line_buf[core->line_len] = '\0';

    if (!core->input_echo) {
        return HYBBX_OK;
    }

    rc = session_write_transport(core, "\b", 1);
    if (rc != HYBBX_OK) {
        return rc;
    }

    return session_line_refresh_suffix(core);
}

static hybbx_result_t session_line_delete_forward(hybbx_session_core_t *core)
{
    if (core == NULL || core->line_cursor >= core->line_len) {
        return HYBBX_OK;
    }

    memmove(core->line_buf + core->line_cursor,
            core->line_buf + core->line_cursor + 1,
            core->line_len - core->line_cursor - 1);
    core->line_len--;
    core->line_buf[core->line_len] = '\0';

    if (!core->input_echo) {
        return HYBBX_OK;
    }

    return session_line_refresh_suffix(core);
}

static hybbx_result_t session_line_insert_char(hybbx_session_core_t *core,
                                               char ch)
{
    size_t line_max;
    hybbx_result_t rc;
    char out[2];

    if (core == NULL || !session_char_echoable(ch)) {
        return HYBBX_OK;
    }

    line_max = session_line_max_len(core);
    if (core->line_len >= line_max) {
        return HYBBX_OK;
    }

    if (core->line_cursor < core->line_len) {
        memmove(core->line_buf + core->line_cursor + 1,
                core->line_buf + core->line_cursor,
                core->line_len - core->line_cursor);
    }

    core->line_buf[core->line_cursor] = ch;
    core->line_len++;
    core->line_cursor++;
    core->line_buf[core->line_len] = '\0';

    if (!core->input_echo) {
        return HYBBX_OK;
    }

    out[0] = ch;
    out[1] = '\0';
    rc = session_write_transport(core, out, 1);
    if (rc != HYBBX_OK) {
        core->line_cursor--;
        core->line_len--;
        core->line_buf[core->line_len] = '\0';
        return rc;
    }

    return session_line_refresh_suffix(core);
}

static int session_history_should_store(const char *line)
{
    if (line == NULL || line[0] == '\0') {
        return 0;
    }

    return strncmp(line, "/login ", 7) != 0 &&
           strncmp(line, "/register ", 10) != 0 &&
           strncmp(line, "/changeme ", 10) != 0 &&
           strncmp(line, "/userchange ", 12) != 0;
}

static void session_history_add(hybbx_session_core_t *core, const char *line)
{
    if (core == NULL || !session_history_should_store(line)) {
        return;
    }

    hybbx_strlcpy(core->history[core->history_next], line,
                  sizeof(core->history[0]));
    core->history_next = (core->history_next + 1u) % HYBBX_HISTORY_MAX;
    if (core->history_count < HYBBX_HISTORY_MAX) {
        core->history_count++;
    }
    core->history_view = -1;
    core->history_saved_line[0] = '\0';
}

static hybbx_result_t session_line_replace(hybbx_session_core_t *core,
                                           const char *line)
{
    size_t len;
    hybbx_result_t rc;

    if (core == NULL || line == NULL) {
        return HYBBX_ERR_INVALID;
    }

    len = strlen(line);
    if (len >= sizeof(core->line_buf)) {
        len = sizeof(core->line_buf) - 1;
    }

    if (core->input_echo) {
        rc = session_line_move_back(core, core->line_cursor);
        if (rc != HYBBX_OK) {
            return rc;
        }
        if (len > 0) {
            rc = session_write_transport(core, line, len);
            if (rc != HYBBX_OK) {
                return rc;
            }
        }
        rc = session_write_transport(core, "\x1b[K", 3);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    memcpy(core->line_buf, line, len);
    core->line_buf[len] = '\0';
    core->line_len = len;
    core->line_cursor = len;
    return HYBBX_OK;
}

static hybbx_result_t session_history_up(hybbx_session_core_t *core)
{
    unsigned slot;

    if (core == NULL || core->history_count == 0) {
        return HYBBX_OK;
    }

    if (core->history_view < 0) {
        hybbx_strlcpy(core->history_saved_line, core->line_buf,
                      sizeof(core->history_saved_line));
        core->history_view = 0;
    } else if ((unsigned)(core->history_view + 1) < core->history_count) {
        core->history_view++;
    } else {
        return HYBBX_OK;
    }

    slot = (core->history_next + HYBBX_HISTORY_MAX - 1u -
            (unsigned)core->history_view) % HYBBX_HISTORY_MAX;
    return session_line_replace(core, core->history[slot]);
}

static hybbx_result_t session_history_down(hybbx_session_core_t *core)
{
    unsigned slot;

    if (core == NULL || core->history_view < 0) {
        return HYBBX_OK;
    }

    if (core->history_view == 0) {
        core->history_view = -1;
        return session_line_replace(core, core->history_saved_line);
    }

    core->history_view--;
    slot = (core->history_next + HYBBX_HISTORY_MAX - 1u -
            (unsigned)core->history_view) % HYBBX_HISTORY_MAX;
    return session_line_replace(core, core->history[slot]);
}

static hybbx_result_t session_apply_csi(hybbx_session_core_t *core,
                                        const char *params, char final_ch)
{
    if (core == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (final_ch == 'A') {
        return session_history_up(core);
    }

    if (final_ch == 'B') {
        return session_history_down(core);
    }

    if (final_ch == 'C') {
        return session_line_cursor_right(core);
    }

    if (final_ch == 'D') {
        return session_line_cursor_left(core);
    }

    if (final_ch == 'H') {
        return session_line_cursor_home(core);
    }

    if (final_ch == 'F') {
        return session_line_cursor_end(core);
    }

    if (final_ch == '~') {
        if (strcmp(params, "1") == 0 || strcmp(params, "7") == 0) {
            return session_line_cursor_home(core);
        }
        if (strcmp(params, "3") == 0) {
            return session_line_delete_forward(core);
        }
        if (strcmp(params, "4") == 0 || strcmp(params, "8") == 0) {
            return session_line_cursor_end(core);
        }
        return HYBBX_OK;
    }

    if (final_ch == 'K' || final_ch == 'P') {
        return HYBBX_OK;
    }

    return HYBBX_OK;
}

static int session_filter_escape_byte(hybbx_session_core_t *core, unsigned char byte)
{
    if (core == NULL) {
        return 0;
    }

    if (core->esc_state == SESSION_ESC_NONE && byte == 0x9bu) {
        core->esc_state = SESSION_ESC_CSI;
        core->csi_len = 0;
        return 1;
    }

    if (core->esc_state == SESSION_ESC_NONE && byte == 0x1bu) {
        core->esc_state = SESSION_ESC_ESC;
        return 1;
    }

    if (core->esc_state == SESSION_ESC_ESC) {
        if (byte == '[') {
            core->esc_state = SESSION_ESC_CSI;
            core->csi_len = 0;
            return 1;
        }
        if (byte == 'O') {
            core->esc_state = SESSION_ESC_SS3;
            return 1;
        }

        session_escape_reset(core);
        return 0;
    }

    if (core->esc_state == SESSION_ESC_SS3) {
        hybbx_result_t rc = HYBBX_OK;

        if (byte == 'C') {
            rc = session_line_cursor_right(core);
        } else if (byte == 'D') {
            rc = session_line_cursor_left(core);
        } else if (byte == 'H') {
            rc = session_line_cursor_home(core);
        } else if (byte == 'F') {
            rc = session_line_cursor_end(core);
        }

        session_escape_reset(core);
        (void)rc;
        return 1;
    }

    if (core->esc_state == SESSION_ESC_CSI) {
        if (core->csi_len + 1 >= SESSION_CSI_BUF_MAX) {
            session_escape_reset(core);
            return 1;
        }

        core->csi_buf[core->csi_len++] = (char)byte;

        if (session_csi_is_final(byte)) {
            char final_ch = (char)byte;
            char params[SESSION_CSI_BUF_MAX];

            if (core->csi_len > 1) {
                memcpy(params, core->csi_buf, core->csi_len - 1);
                params[core->csi_len - 1] = '\0';
            } else {
                params[0] = '\0';
            }

            session_escape_reset(core);
            (void)session_apply_csi(core, params, final_ch);
            return 1;
        }

        return 1;
    }

    return 0;
}

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

    if (area == HYBBX_AREA_CONFERENCE) {
        hybbx_conference_area_leaving(&core->pub);
    }

    if (area == HYBBX_AREA_MAIL) {
        core->mail_composing = 0;
        core->mail_compose_body[0] = '\0';
        core->mail_compose_body_len = 0;
        core->mail_compose_to[0] = '\0';
        core->mail_compose_subject[0] = '\0';
    }

    if (area == HYBBX_AREA_PROXYMAIL) {
        core->proxymail_composing = 0;
        core->proxymail_compose_body[0] = '\0';
        core->proxymail_compose_body_len = 0;
        core->proxymail_compose_to[0] = '\0';
        core->proxymail_compose_subject[0] = '\0';
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
    if (core != NULL && core->bandwidth_paused) {
        return HYBBX_OK;
    }

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

hybbx_result_t hybbx_session_command_gap(hybbx_session_t *session)
{
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
    core->line_cursor = 0;
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
    session_history_add(core, core->line_buf);
    session_process_line(core, core->line_buf);
    core->line_len = 0;
    core->line_cursor = 0;
    core->history_view = -1;
    if ((core->logged_in || core->login_prompt) &&
        core->area != HYBBX_AREA_CHAT &&
        core->area != HYBBX_AREA_CONFERENCE &&
        core->area != HYBBX_AREA_PROXYCHAT &&
        !(core->area == HYBBX_AREA_MAIL && core->mail_composing) &&
        !(core->area == HYBBX_AREA_PROXYMAIL && core->proxymail_composing)) {
        hybbx_session_show_prompt(&core->pub);
    }
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
        return session_line_backspace(core);
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

    return session_line_insert_char(core, ch);
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

    if (hybbx_conference_invite_pending(&core->pub) &&
        scope == HYBBX_CMD_SCOPE_LOCAL) {
        if (hybbx_conference_reply_invite(core->service, &core->pub, line) ==
            HYBBX_OK) {
            return;
        }
        hybbx_session_write_line(&core->pub, "Reply y/n or yes/no.");
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

    if (core->area == HYBBX_AREA_PROXYMAIL && core->proxymail_composing &&
        scope == HYBBX_CMD_SCOPE_LOCAL) {
        const hybbx_mail_config_t *mail;
        size_t line_len;
        size_t body_max;

        mail = hybbx_service_get_mail(core->service);
        body_max = mail != NULL ? mail->body_max : HYBBX_MAIL_BODY_MAX;
        line_len = strlen(line);

        if (core->proxymail_compose_body_len + line_len + 2 > body_max) {
            hybbx_session_write_line(&core->pub, "Message body too long.");
            return;
        }

        if (core->proxymail_compose_body_len > 0) {
            core->proxymail_compose_body[core->proxymail_compose_body_len++] =
                '\n';
        }
        memcpy(core->proxymail_compose_body + core->proxymail_compose_body_len,
               line, line_len + 1);
        core->proxymail_compose_body_len += line_len;
        return;
    }

    if (core->area == HYBBX_AREA_PROXYCHAT && scope == HYBBX_CMD_SCOPE_LOCAL) {
        if (hybbx_session_is_guest(&core->pub)) {
            hybbx_session_write_line(&core->pub, "Guests cannot use proxychat.");
            return;
        }

        if (strlen(line) > session_chat_message_max(core)) {
            hybbx_session_write_line(&core->pub, "Message too long.");
            return;
        }

        (void)hybbx_proxychat_post(hybbx_session_service(&core->pub), &core->pub,
                                   line);
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

    if (core->area == HYBBX_AREA_CONFERENCE && scope == HYBBX_CMD_SCOPE_LOCAL) {
        if (hybbx_session_is_guest(&core->pub)) {
            hybbx_session_write_line(&core->pub, "Guests cannot use conference.");
            return;
        }

        if (strlen(line) > session_chat_message_max(core)) {
            hybbx_session_write_line(&core->pub, "Message too long.");
            return;
        }

        (void)hybbx_conference_post(core->service, &core->pub, line);
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
    int suppress_startup_text;

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

    suppress_startup_text = (transport->kind == HYBBX_TRANSPORT_CIRCUIT);

    (void)hybbx_term_init_session(&core->pub);

    if (!suppress_startup_text) {
        session_send_banner(core);
    }

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

        if (!suppress_startup_text) {
            if (session_console_show_guest_events()) {
                printf("[session] auto-login %s on %s (session %llu)\n",
                       core->record.username, transport->name,
                       (unsigned long long)core->record.session_id);
            }
            hybbx_log_stats("auto-login %s on %s (session %llu)",
                            core->record.username, transport->name,
                            (unsigned long long)core->record.session_id);
        }

        if (!suppress_startup_text) {
            hybbx_session_show_prompt(&core->pub);
        }
    } else {
        core->login_prompt = 1;
        if (!suppress_startup_text) {
            hybbx_session_show_prompt(&core->pub);
        }
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

    if (hybbx_conference_invite_pending(session) ||
        hybbx_session_area(session) == HYBBX_AREA_CONFERENCE) {
        hybbx_conference_session_closed(session);
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

    if (core->bandwidth_disconnect) {
        return HYBBX_SESSION_END;
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

    if (core->bandwidth_paused) {
        return HYBBX_OK;
    }

    if (core->logged_in) {
        expiry_rc = session_check_guest_expiry(core);
        if (expiry_rc != HYBBX_OK) {
            return expiry_rc;
        }
    }

    for (i = 0; i < len; i++) {
        hybbx_result_t rc;

        if (session_filter_escape_byte(core, data[i])) {
            continue;
        }

        rc = session_handle_user_byte(core, data[i]);

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
    case HYBBX_AREA_CONFERENCE:
        return "conference";
    case HYBBX_AREA_PROXYMAIL:
        return "proxymail";
    case HYBBX_AREA_PROXYCHAT:
        return "proxychat";
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

    if (session_str_ieq(name, "conference")) {
        return HYBBX_AREA_CONFERENCE;
    }

    if (session_str_ieq(name, "proxymail")) {
        return HYBBX_AREA_PROXYMAIL;
    }

    if (session_str_ieq(name, "proxychat")) {
        return HYBBX_AREA_PROXYCHAT;
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

hybbx_service_t *hybbx_session_service(hybbx_session_t *session)
{
    hybbx_session_core_t *core;

    if (session == NULL) {
        return NULL;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return NULL;
    }

    return core->service;
}

static int session_str_ieq_local(const char *a, const char *b)
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

int hybbx_session_conference_may_invite(hybbx_session_t *session,
                                        const char *target)
{
    hybbx_session_core_t *core;
    size_t s;
    size_t k;
    time_t now;
    unsigned count;

    if (session == NULL || target == NULL || target[0] == '\0') {
        return 0;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return 0;
    }

    now = time(NULL);
    for (s = 0; s < sizeof(core->conference_invite_rates) /
                    sizeof(core->conference_invite_rates[0]); s++) {
        if (!session_str_ieq_local(core->conference_invite_rates[s].target,
                                   target)) {
            continue;
        }

        count = 0;
        for (k = 0; k < HYBBX_CONFERENCE_INVITE_MAX_PER_TARGET; k++) {
            time_t sent = core->conference_invite_rates[s].sent_at[k];

            if (sent != 0 &&
                (now - sent) < (time_t)HYBBX_CONFERENCE_INVITE_WINDOW_SEC) {
                count++;
            }
        }

        return count < HYBBX_CONFERENCE_INVITE_MAX_PER_TARGET;
    }

    return 1;
}

void hybbx_session_conference_invite_sent(hybbx_session_t *session,
                                          const char *target)
{
    hybbx_session_core_t *core;
    size_t s;
    size_t slot = (size_t)-1;
    size_t empty = (size_t)-1;
    size_t k;
    time_t now;

    if (session == NULL || target == NULL || target[0] == '\0') {
        return;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return;
    }

    now = time(NULL);

    for (s = 0; s < sizeof(core->conference_invite_rates) /
                    sizeof(core->conference_invite_rates[0]); s++) {
        if (core->conference_invite_rates[s].target[0] == '\0') {
            if (empty == (size_t)-1) {
                empty = s;
            }
            continue;
        }

        if (session_str_ieq_local(core->conference_invite_rates[s].target,
                                  target)) {
            slot = s;
            break;
        }
    }

    if (slot == (size_t)-1) {
        slot = empty;
    }

    if (slot == (size_t)-1) {
        slot = 0;
    }

    if (core->conference_invite_rates[slot].target[0] == '\0') {
        hybbx_strlcpy(core->conference_invite_rates[slot].target, target,
                      sizeof(core->conference_invite_rates[slot].target));
    }

    for (k = 0; k < HYBBX_CONFERENCE_INVITE_MAX_PER_TARGET - 1u; k++) {
        core->conference_invite_rates[slot].sent_at[k] =
            core->conference_invite_rates[slot].sent_at[k + 1];
    }

    core->conference_invite_rates[slot].sent_at[
        HYBBX_CONFERENCE_INVITE_MAX_PER_TARGET - 1u] = now;
}

void hybbx_session_set_conference_invite(hybbx_session_t *session,
                                         const char *from_username,
                                         const char *topic)
{
    hybbx_session_core_t *core;

    if (session == NULL || from_username == NULL || topic == NULL) {
        return;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return;
    }

    core->conference_invite_pending = 1;
    hybbx_strlcpy(core->conference_invite_from, from_username,
                  sizeof(core->conference_invite_from));
    hybbx_strlcpy(core->conference_invite_topic, topic,
                  sizeof(core->conference_invite_topic));
}

void hybbx_session_clear_conference_invite(hybbx_session_t *session)
{
    hybbx_session_core_t *core;

    if (session == NULL) {
        return;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return;
    }

    core->conference_invite_pending = 0;
    core->conference_invite_from[0] = '\0';
    core->conference_invite_topic[0] = '\0';
}

int hybbx_conference_invite_pending(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return 0;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return 0;
    }

    return core->conference_invite_pending != 0;
}

const char *hybbx_session_conference_invite_from(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return NULL;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL || !core->conference_invite_pending) {
        return NULL;
    }

    return core->conference_invite_from;
}

const char *hybbx_session_conference_invite_topic(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return NULL;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL || !core->conference_invite_pending) {
        return NULL;
    }

    return core->conference_invite_topic;
}

hybbx_result_t hybbx_session_join_conference(hybbx_session_t *session,
                                             const char *topic,
                                             const char *partner_username)
{
    hybbx_session_core_t *core;
    const hybbx_chat_config_t *chat;

    if (session == NULL || topic == NULL || partner_username == NULL ||
        topic[0] == '\0' || partner_username[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL || core->service == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (core->area != HYBBX_AREA_CONFERENCE) {
        hybbx_result_t push_rc = session_area_push(core, HYBBX_AREA_CONFERENCE);

        if (push_rc != HYBBX_OK) {
            return push_rc;
        }
    }

    hybbx_strlcpy(core->conference_topic, topic, sizeof(core->conference_topic));
    hybbx_strlcpy(core->conference_partner, partner_username,
                  sizeof(core->conference_partner));

    chat = hybbx_service_get_chat(core->service);
    if (chat != NULL && !core->chat_max_notice_shown) {
        char notice[96];

        snprintf(notice, sizeof(notice), "Max %u chars per message.",
                 chat->message_max);
        hybbx_session_write_line(session, notice);
        core->chat_max_notice_shown = 1;
    }

    return HYBBX_OK;
}

void hybbx_session_clear_conference(hybbx_session_t *session)
{
    hybbx_session_core_t *core;

    if (session == NULL) {
        return;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return;
    }

    core->conference_topic[0] = '\0';
    core->conference_partner[0] = '\0';
}

const char *hybbx_session_conference_partner(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return NULL;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL || core->conference_partner[0] == '\0') {
        return NULL;
    }

    return core->conference_partner;
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

hybbx_result_t hybbx_session_enter_mail(hybbx_session_t *session)
{
    return hybbx_session_enter_area(session, HYBBX_AREA_MAIL);
}

hybbx_result_t hybbx_session_enter_chat(hybbx_session_t *session)
{
    return hybbx_session_enter_area(session, HYBBX_AREA_CHAT);
}

hybbx_result_t hybbx_session_enter_proxymail(hybbx_session_t *session)
{
    hybbx_session_core_t *core;
    hybbx_result_t rc;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (core->area != HYBBX_AREA_MAIL &&
        core->area != HYBBX_AREA_PROXYMAIL) {
        rc = session_area_push(core, HYBBX_AREA_MAIL);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    return session_area_push(core, HYBBX_AREA_PROXYMAIL);
}

hybbx_result_t hybbx_session_enter_proxychat(hybbx_session_t *session)
{
    hybbx_session_core_t *core;
    hybbx_result_t rc;

    if (session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (core->area != HYBBX_AREA_CHAT &&
        core->area != HYBBX_AREA_PROXYCHAT) {
        rc = session_area_push(core, HYBBX_AREA_CHAT);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    return session_area_push(core, HYBBX_AREA_PROXYCHAT);
}

int hybbx_session_proxymail_composing(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return 0;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return 0;
    }

    return core->proxymail_composing;
}

hybbx_result_t hybbx_session_proxymail_compose_start(hybbx_session_t *session,
                                                     const char *to_address,
                                                     const char *subject)
{
    hybbx_session_core_t *core;
    const hybbx_mail_config_t *mail;
    size_t subject_len;

    if (session == NULL || to_address == NULL || subject == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (!hybbx_proxymail_parse_address(to_address, NULL, 0, NULL, 0)) {
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

    hybbx_strlcpy(core->proxymail_compose_to, to_address,
                  sizeof(core->proxymail_compose_to));
    hybbx_strlcpy(core->proxymail_compose_subject, subject,
                  sizeof(core->proxymail_compose_subject));
    core->proxymail_compose_body[0] = '\0';
    core->proxymail_compose_body_len = 0;
    core->proxymail_composing = 1;

    return hybbx_session_enter_proxymail(session);
}

void hybbx_session_proxymail_compose_cancel(hybbx_session_t *session)
{
    hybbx_session_core_t *core;

    if (session == NULL) {
        return;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return;
    }

    core->proxymail_composing = 0;
    core->proxymail_compose_body[0] = '\0';
    core->proxymail_compose_body_len = 0;
    core->proxymail_compose_to[0] = '\0';
    core->proxymail_compose_subject[0] = '\0';
}

const char *hybbx_session_proxymail_compose_body(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return "";
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL || !core->proxymail_composing) {
        return "";
    }

    return core->proxymail_compose_body;
}

const char *hybbx_session_proxymail_compose_to(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return "";
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL || !core->proxymail_composing) {
        return "";
    }

    return core->proxymail_compose_to;
}

const char *hybbx_session_proxymail_compose_subject(
    const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return "";
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL || !core->proxymail_composing) {
        return "";
    }

    return core->proxymail_compose_subject;
}

time_t hybbx_session_connected_at(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return 0;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return 0;
    }

    return core->record.connected_at;
}

int hybbx_session_bandwidth_paused(const hybbx_session_t *session)
{
    const hybbx_session_core_t *core;

    if (session == NULL) {
        return 0;
    }

    core = (const hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return 0;
    }

    return core->bandwidth_paused;
}

void hybbx_session_set_bandwidth_paused(hybbx_session_t *session, int paused)
{
    hybbx_session_core_t *core;

    if (session == NULL) {
        return;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL) {
        return;
    }

    core->bandwidth_paused = paused ? 1 : 0;
}

void hybbx_session_disconnect_bandwidth(hybbx_session_t *session)
{
    hybbx_session_core_t *core;

    if (session == NULL) {
        return;
    }

    core = (hybbx_session_core_t *)session->core_data;
    if (core == NULL || !core->logged_in) {
        return;
    }

    core->bandwidth_paused = 0;
    (void)hybbx_traffic_emit(session, &core->out_col,
                               HYBBX_BANDWIDTH_DISCONNECT_MSG,
                               strlen(HYBBX_BANDWIDTH_DISCONNECT_MSG));
    (void)hybbx_traffic_emit(session, &core->out_col, "\n", 1);
    core->bandwidth_disconnect = 1;
}
