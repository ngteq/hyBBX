#include "hybbx/hybbx.h"
#include "hybbx/command.h"
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "hybbx/texts.h"
#include "hybbx/storage.h"
#include "hybbx/auth.h"
#include "hybbx/chat.h"
#include "hybbx/mail.h"
#include "hybbx/traffic.h"
#include "hybbx/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

static int cmd_verb_allowed(hybbx_user_level_t level, const char *verb)
{
    if (verb == NULL || verb[0] == '\0') {
        return 1;
    }

    if (str_ieq(verb, "help") || str_ieq(verb, "?") || str_ieq(verb, "command")) {
        return 1;
    }

    if (level == HYBBX_LEVEL_GUEST) {
        return str_ieq(verb, "motd") || str_ieq(verb, "news") ||
               str_ieq(verb, "login") || str_ieq(verb, "register") ||
               str_ieq(verb, "clear") || str_ieq(verb, "cls") ||
               str_ieq(verb, "reset") || str_ieq(verb, "echo");
    }

    if (str_ieq(verb, "news") || str_ieq(verb, "motd") ||
        str_ieq(verb, "who") || str_ieq(verb, "session") || str_ieq(verb, "info") ||
        str_ieq(verb, "version") || str_ieq(verb, "ver") ||
        str_ieq(verb, "leave") || str_ieq(verb, "back") ||
        str_ieq(verb, "main") || str_ieq(verb, "menu") ||
        str_ieq(verb, "clear") || str_ieq(verb, "cls") ||
        str_ieq(verb, "reset") || str_ieq(verb, "echo") ||
        str_ieq(verb, "exit") || str_ieq(verb, "logout") ||
        str_ieq(verb, "bye") || str_ieq(verb, "quit") ||
        str_ieq(verb, "login") || str_ieq(verb, "register") ||
        str_ieq(verb, "deleteme")) {
        return 1;
    }

    if (str_ieq(verb, "chat")) {
        return !hybbx_user_level_is_guest(level);
    }

    if (str_ieq(verb, "mail")) {
        return !hybbx_user_level_is_guest(level);
    }

    if (str_ieq(verb, "activate")) {
        return hybbx_auth_may_activate(level);
    }

    if (str_ieq(verb, "promote") || str_ieq(verb, "demote") ||
        str_ieq(verb, "delete") || str_ieq(verb, "del")) {
        return level == HYBBX_LEVEL_SYSOP || level == HYBBX_LEVEL_ADMIN;
    }

    return 0;
}

static int cmd_deleteme_confirmed(const char *arg)
{
    return arg != NULL && hybbx_bool_is_true(arg);
}

static int cmd_help_shows_deleteme(hybbx_user_level_t level)
{
    return !hybbx_user_level_is_sysop(level) &&
           !hybbx_user_level_is_guest(level);
}

static const char *cmd_help_unknown(hybbx_session_t *session)
{
    if (hybbx_session_user_level(session) == HYBBX_LEVEL_GUEST) {
        return "Unknown command. Try /help.";
    }

    return "Unknown command. /help for list.";
}

static hybbx_result_t cmd_check_access(hybbx_session_t *session,
                                       const char *verb)
{
    hybbx_user_level_t level = hybbx_session_user_level(session);

    if (cmd_verb_allowed(level, verb)) {
        return HYBBX_OK;
    }

    if (level == HYBBX_LEVEL_GUEST) {
        hybbx_session_write_line(session,
            "Not available to guests. Try /help.");
    } else {
        hybbx_session_write_line(session, "Insufficient privileges.");
    }

    return HYBBX_ERR_DENIED;
}

static void cmd_help_line(hybbx_session_t *session,
                          const char *cmd, const char *desc)
{
    char buf[HYBBX_LINE_MAX];

    snprintf(buf, sizeof(buf), "  %-18s %s", cmd, desc);
    hybbx_session_write_line(session, buf);
}

static void cmd_help_pair(hybbx_session_t *session,
                          const char *c1, const char *d1,
                          const char *c2, const char *d2)
{
    char buf[HYBBX_LINE_MAX];

    if (c2 != NULL && d2 != NULL) {
        snprintf(buf, sizeof(buf), "  %-18s %-22s  %-18s %s",
                 c1, d1, c2, d2);
    } else {
        snprintf(buf, sizeof(buf), "  %-18s %s", c1, d1);
    }
    hybbx_session_write_line(session, buf);
}

static void cmd_help_group(hybbx_session_t *session,
                           const char *label, const char *cmds)
{
    char buf[HYBBX_LINE_MAX];

    snprintf(buf, sizeof(buf), "  %-10s %s", label, cmds);
    hybbx_session_write_line(session, buf);
}

static void cmd_help_topic_title(hybbx_session_t *session,
                                 const char *cmd, const char *summary)
{
    char buf[HYBBX_LINE_MAX];

    snprintf(buf, sizeof(buf), "%s — %s", cmd, summary);
    hybbx_session_write_line(session, buf);
}

static void cmd_help_topic_detail(hybbx_session_t *session, const char *line)
{
    hybbx_session_write_line(session, line);
}

static void cmd_help_list_for_level(hybbx_session_t *session)
{
    hybbx_user_level_t level = hybbx_session_user_level(session);
    char staff[HYBBX_LINE_MAX];

    hybbx_session_write_line(session,
        "HyBBX commands (/ required)  —  /help <cmd> for details");

    if (level == HYBBX_LEVEL_GUEST) {
        cmd_help_pair(session, "/help", "list or explain",
                      "/motd", "message of the day");
        cmd_help_pair(session, "/news", "system news",
                      "/login", "<user> <pass>");
        cmd_help_pair(session, "/register", "new account",
                      "/clear", "clear screen");
        cmd_help_line(session, "/echo", "input echo on/off");
        return;
    }

    cmd_help_group(session, "General",
                   "/help  /news  /motd  /who  /session  /version");
    cmd_help_group(session, "Screen", "/clear  /echo");
    cmd_help_group(session, "Areas",
                   "/leave  /main  /chat  /mail  /exit");
    cmd_help_group(session, "Account", "/login  /register");

    if (cmd_help_shows_deleteme(level)) {
        hybbx_session_write_line(session, "             /deleteme yes");
    }

    staff[0] = '\0';
    if (hybbx_auth_may_activate(level)) {
        hybbx_strlcpy(staff, "/activate", sizeof(staff));
    }
    if (level == HYBBX_LEVEL_SYSOP || level == HYBBX_LEVEL_ADMIN) {
        snprintf(staff + strlen(staff), sizeof(staff) - strlen(staff),
                 "%s/promote  /demote  /delete",
                 staff[0] != '\0' ? "  " : "");
    }

    if (staff[0] != '\0') {
        cmd_help_group(session, "Staff", staff);
    }
}

static hybbx_result_t cmd_help_topic(hybbx_session_t *session, const char *topic)
{
    hybbx_user_level_t level = hybbx_session_user_level(session);
    const char *canonical = topic;

    if (topic == NULL || topic[0] == '\0') {
        cmd_help_list_for_level(session);
        return HYBBX_OK;
    }

    if (str_ieq(topic, "info")) {
        canonical = "session";
    } else if (str_ieq(topic, "ver")) {
        canonical = "version";
    } else if (str_ieq(topic, "logout") || str_ieq(topic, "bye") ||
               str_ieq(topic, "quit")) {
        canonical = "exit";
    } else if (str_ieq(topic, "?")) {
        canonical = "help";
    } else if (str_ieq(topic, "command")) {
        canonical = "help";
    } else if (str_ieq(topic, "del")) {
        canonical = "delete";
    } else if (str_ieq(topic, "back")) {
        canonical = "leave";
    } else if (str_ieq(topic, "menu")) {
        canonical = "main";
    } else if (str_ieq(topic, "cls") || str_ieq(topic, "reset")) {
        canonical = "clear";
    }

    if (!cmd_verb_allowed(level, canonical) ||
        (str_ieq(canonical, "deleteme") && !cmd_help_shows_deleteme(level))) {
        hybbx_session_write_line(session, cmd_help_unknown(session));
        return HYBBX_ERR_NOT_FOUND;
    }

    if (str_ieq(canonical, "help")) {
        cmd_help_topic_title(session, "/help",
                             "list commands or /help <cmd>");
        if (level == HYBBX_LEVEL_GUEST) {
            cmd_help_topic_detail(session, "  aliases: /help  ?");
        } else {
            cmd_help_topic_detail(session,
                "  aliases: /  /command  ?     /command <verb> = /<verb>");
        }
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "news")) {
        cmd_help_topic_title(session, "/news", "show system news (news.txt)");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "motd")) {
        cmd_help_topic_title(session, "/motd", "show message of the day");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "who")) {
        cmd_help_topic_title(session, "/who", "users online on this node");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "session")) {
        cmd_help_topic_title(session, "/session",
                             "username, session id, transport");
        cmd_help_topic_detail(session, "  alias: /info");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "version")) {
        cmd_help_topic_title(session, "/version", "running HyBBX version");
        cmd_help_topic_detail(session, "  alias: /ver");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "leave")) {
        cmd_help_topic_title(session, "/leave", "up one area level");
        cmd_help_topic_detail(session, "  alias: /back");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "main")) {
        cmd_help_topic_title(session, "/main", "return to main area");
        cmd_help_topic_detail(session, "  alias: /menu");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "clear")) {
        cmd_help_topic_title(session, "/clear",
                             "clear screen and discard input line");
        cmd_help_topic_detail(session, "  aliases: /cls  /reset");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "echo")) {
        cmd_help_topic_title(session, "/echo",
                             "show or set typed-character echo");
        cmd_help_topic_detail(session, "  /echo          status");
        cmd_help_topic_detail(session, "  /echo yes|no   on or off");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "chat")) {
        cmd_help_topic_title(session, "/chat", "chat channels");
        cmd_help_topic_detail(session,
            "  /chat              list    /chat <n|name>  join");
        cmd_help_topic_detail(session,
            "  registered users only — /leave or /main to exit");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "mail")) {
        cmd_help_topic_title(session, "/mail", "personal mailbox");
        cmd_help_topic_detail(session,
            "  list [<from>-<to>]   subjects, e.g. list 1-15  list 5-20");
        cmd_help_topic_detail(session,
            "  read <n>  |  delete <n|from-to>  |  recycle");
        cmd_help_topic_detail(session,
            "  send <user> <subj>  —  body lines, then /mail done");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "exit")) {
        cmd_help_topic_title(session, "/exit", "close connection");
        cmd_help_topic_detail(session, "  aliases: /quit  /logout  /bye");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "login")) {
        cmd_help_topic_title(session, "/login", "log in as registered user");
        cmd_help_topic_detail(session,
            "  /login <username> <password>   account must be activated");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "register")) {
        cmd_help_topic_title(session, "/register", "create user account");
        cmd_help_topic_detail(session,
            "  /register <user> <name> <country> <location> <email>");
        cmd_help_topic_detail(session,
            "  username 4-12 chars (a-z 0-9) — pending until /activate");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "activate")) {
        cmd_help_topic_title(session, "/activate",
                             "enable pending account (staff)");
        cmd_help_topic_detail(session, "  /activate <username>");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "promote")) {
        if (level == HYBBX_LEVEL_SYSOP) {
            cmd_help_topic_title(session, "/promote",
                                 "grant admin or mod role (sysop)");
            cmd_help_topic_detail(session, "  /promote <username> admin|mod");
        } else {
            cmd_help_topic_title(session, "/promote",
                                 "grant mod role (admin)");
            cmd_help_topic_detail(session, "  /promote <username> mod");
        }
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "demote")) {
        cmd_help_topic_title(session, "/demote",
                             "remove admin or mod role (staff)");
        cmd_help_topic_detail(session, "  /demote <username>");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "deleteme")) {
        cmd_help_topic_title(session, "/deleteme",
                             "permanently delete your account");
        cmd_help_topic_detail(session, "  /deleteme yes   (required)");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "delete")) {
        cmd_help_topic_title(session, "/delete",
                             "permanently remove account (staff)");
        cmd_help_topic_detail(session, "  /delete <username>");
        cmd_help_topic_detail(session, "  sysop account cannot be deleted");
        return HYBBX_OK;
    }

    hybbx_session_write_line(session, cmd_help_unknown(session));
    return HYBBX_ERR_NOT_FOUND;
}

static hybbx_result_t cmd_help(hybbx_session_t *session,
                               const hybbx_parsed_command_t *cmd)
{
    if (cmd->argc == 0) {
        cmd_help_list_for_level(session);
        return HYBBX_OK;
    }

    return cmd_help_topic(session, cmd->argv[0]);
}

static hybbx_result_t cmd_news(hybbx_service_t *service, hybbx_session_t *session)
{
    const hybbx_texts_config_t *texts = hybbx_service_get_texts(service);

    if (texts == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (hybbx_texts_send_file(texts, session, HYBBX_TEXT_NEWS) != HYBBX_OK) {
        hybbx_session_write_line(session, "(no news available)");
    }

    return HYBBX_OK;
}

static hybbx_result_t cmd_motd(hybbx_service_t *service, hybbx_session_t *session)
{
    const hybbx_texts_config_t *texts = hybbx_service_get_texts(service);

    if (texts == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (hybbx_texts_send_file(texts, session, HYBBX_TEXT_MOTD) != HYBBX_OK) {
        hybbx_session_write_line(session, "(no motd available)");
    }

    return HYBBX_OK;
}

static hybbx_result_t cmd_who(hybbx_session_t *session)
{
    hybbx_session_write_line(session, "Online users:");
    hybbx_session_write_line(session, hybbx_session_username(session));
    return HYBBX_OK;
}

static hybbx_result_t cmd_session(hybbx_session_t *session)
{
    char buf[128];
    const hybbx_session_record_t *rec = hybbx_session_record(session);

    if (rec == NULL) {
        return HYBBX_ERR_INVALID;
    }

    snprintf(buf, sizeof(buf), "User: %s", rec->username);
    hybbx_session_write_line(session, buf);
    snprintf(buf, sizeof(buf), "Level: %s",
             hybbx_user_level_name(hybbx_session_user_level(session)));
    hybbx_session_write_line(session, buf);
    snprintf(buf, sizeof(buf), "Session: %llu",
             (unsigned long long)rec->session_id);
    hybbx_session_write_line(session, buf);
    snprintf(buf, sizeof(buf), "Transport: %s", rec->transport);
    hybbx_session_write_line(session, buf);
    return HYBBX_OK;
}

static hybbx_result_t parse_registration(const hybbx_parsed_command_t *cmd,
                                         hybbx_user_registration_t *reg)
{
    size_t i;
    size_t pos = 0;

    if (cmd == NULL || reg == NULL || cmd->argc < 5) {
        return HYBBX_ERR_INVALID;
    }

    memset(reg, 0, sizeof(*reg));
    hybbx_strlcpy(reg->username, cmd->argv[0], sizeof(reg->username));
    hybbx_username_normalize(reg->username);
    hybbx_strlcpy(reg->email, cmd->argv[cmd->argc - 1], sizeof(reg->email));
    hybbx_strlcpy(reg->location, cmd->argv[cmd->argc - 2],
                  sizeof(reg->location));
    hybbx_strlcpy(reg->country, cmd->argv[cmd->argc - 3], sizeof(reg->country));

    for (i = 1; i + 3 < cmd->argc; i++) {
        size_t part_len;
        const char *part = cmd->argv[i];

        if (part == NULL) {
            continue;
        }

        part_len = strlen(part);
        if (pos > 0) {
            if (pos + 1 >= sizeof(reg->full_name)) {
                return HYBBX_ERR_INVALID;
            }
            reg->full_name[pos++] = ' ';
        }

        if (pos + part_len >= sizeof(reg->full_name)) {
            return HYBBX_ERR_INVALID;
        }

        memcpy(reg->full_name + pos, part, part_len);
        pos += part_len;
    }

    reg->full_name[pos] = '\0';
    return HYBBX_OK;
}

static hybbx_result_t cmd_lookup_user(hybbx_service_t *service,
                                      const char *username,
                                      hybbx_user_record_t *out)
{
    hybbx_storage_t *storage;
    char normalized[HYBBX_USER_NAME_MAX];
    hybbx_result_t rc;

    if (service == NULL || username == NULL || out == NULL ||
        username[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    storage = hybbx_service_get_storage(service);
    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_strlcpy(normalized, username, sizeof(normalized));
    hybbx_username_normalize(normalized);

    rc = hybbx_storage_find_user(storage, normalized, out);
    if (rc == HYBBX_ERR_NOT_FOUND) {
        return HYBBX_ERR_NOT_FOUND;
    }

    return rc;
}

static void cmd_deny_privilege(hybbx_session_t *session)
{
    hybbx_session_write_line(session, "Insufficient privileges.");
}

static hybbx_result_t cmd_activate(hybbx_service_t *service,
                                   hybbx_session_t *session,
                                   const hybbx_parsed_command_t *cmd)
{
    hybbx_user_record_t target;
    hybbx_user_level_t actor_level;
    char buf[128];
    hybbx_result_t rc;

    if (cmd->argc < 1 || cmd->argv[0] == NULL || cmd->argv[0][0] == '\0') {
        hybbx_session_write_line(session, "Usage: /activate <username>");
        return HYBBX_OK;
    }

    actor_level = hybbx_session_user_level(session);
    if (!hybbx_auth_may_activate(actor_level)) {
        cmd_deny_privilege(session);
        return HYBBX_ERR_DENIED;
    }

    rc = cmd_lookup_user(service, cmd->argv[0], &target);
    if (rc == HYBBX_ERR_NOT_FOUND) {
        hybbx_session_write_line(session, "Unknown user.");
        return HYBBX_OK;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (target.level != HYBBX_LEVEL_USER) {
        hybbx_session_write_line(session,
            "Only pending registered user accounts can be activated.");
        return HYBBX_OK;
    }

    if (target.active) {
        hybbx_session_write_line(session, "Account is already active.");
        return HYBBX_OK;
    }

    target.active = 1;
    rc = hybbx_storage_update_user(hybbx_service_get_storage(service), &target);
    if (rc != HYBBX_OK) {
        return rc;
    }

    snprintf(buf, sizeof(buf), "Activated '%s'.", target.username);
    hybbx_session_write_line(session, buf);
    return HYBBX_OK;
}

static hybbx_result_t cmd_promote(hybbx_service_t *service,
                                  hybbx_session_t *session,
                                  const hybbx_parsed_command_t *cmd)
{
    hybbx_user_record_t target;
    hybbx_user_level_t actor_level;
    hybbx_user_level_t new_level;
    char buf[128];
    hybbx_result_t rc;

    if (cmd->argc < 2 || cmd->argv[0] == NULL || cmd->argv[0][0] == '\0' ||
        cmd->argv[1] == NULL || cmd->argv[1][0] == '\0') {
        hybbx_session_write_line(session,
            "Usage: /promote <username> admin|mod");
        return HYBBX_OK;
    }

    actor_level = hybbx_session_user_level(session);
    new_level = hybbx_user_level_parse(cmd->argv[1]);
    if (new_level != HYBBX_LEVEL_ADMIN && new_level != HYBBX_LEVEL_MOD) {
        hybbx_session_write_line(session,
            "Level must be admin or mod.");
        return HYBBX_OK;
    }

    rc = cmd_lookup_user(service, cmd->argv[0], &target);
    if (rc == HYBBX_ERR_NOT_FOUND) {
        hybbx_session_write_line(session, "Unknown user.");
        return HYBBX_OK;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (!hybbx_auth_may_promote(actor_level, target.level, target.active,
                                new_level)) {
        if (actor_level == HYBBX_LEVEL_MOD || actor_level == HYBBX_LEVEL_USER) {
            cmd_deny_privilege(session);
        } else if (new_level == HYBBX_LEVEL_ADMIN) {
            hybbx_session_write_line(session,
                "Only the Sysop may add Admins.");
        } else {
            hybbx_session_write_line(session,
                "Only Sysop or Admin may add Mods.");
        }
        return HYBBX_ERR_DENIED;
    }

    if (target.level == new_level) {
        snprintf(buf, sizeof(buf), "'%s' is already %s.",
                 target.username, hybbx_user_level_name(new_level));
        hybbx_session_write_line(session, buf);
        return HYBBX_OK;
    }

    target.level = new_level;
    rc = hybbx_storage_update_user(hybbx_service_get_storage(service), &target);
    if (rc != HYBBX_OK) {
        return rc;
    }

    snprintf(buf, sizeof(buf), "Promoted '%s' to %s.",
             target.username, hybbx_user_level_name(new_level));
    hybbx_session_write_line(session, buf);
    return HYBBX_OK;
}

static hybbx_result_t cmd_demote(hybbx_service_t *service,
                                 hybbx_session_t *session,
                                 const hybbx_parsed_command_t *cmd)
{
    hybbx_user_record_t target;
    hybbx_user_level_t actor_level;
    hybbx_user_level_t old_level;
    char buf[128];
    hybbx_result_t rc;

    if (cmd->argc < 1 || cmd->argv[0] == NULL || cmd->argv[0][0] == '\0') {
        hybbx_session_write_line(session, "Usage: /demote <username>");
        return HYBBX_OK;
    }

    actor_level = hybbx_session_user_level(session);

    rc = cmd_lookup_user(service, cmd->argv[0], &target);
    if (rc == HYBBX_ERR_NOT_FOUND) {
        hybbx_session_write_line(session, "Unknown user.");
        return HYBBX_OK;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (str_ieq(target.username, hybbx_session_username(session))) {
        hybbx_session_write_line(session, "You cannot demote your own account.");
        return HYBBX_OK;
    }

    old_level = target.level;
    if (!hybbx_auth_may_demote(actor_level, target.level)) {
        if (actor_level == HYBBX_LEVEL_MOD || actor_level == HYBBX_LEVEL_USER) {
            cmd_deny_privilege(session);
        } else if (target.level == HYBBX_LEVEL_ADMIN) {
            hybbx_session_write_line(session,
                "Only the Sysop may remove Admins.");
        } else if (target.level == HYBBX_LEVEL_MOD) {
            hybbx_session_write_line(session,
                "Only Sysop or Admin may remove Mods.");
        } else {
            hybbx_session_write_line(session,
                "That account is not an Admin or Mod.");
        }
        return HYBBX_ERR_DENIED;
    }

    target.level = HYBBX_LEVEL_USER;
    rc = hybbx_storage_update_user(hybbx_service_get_storage(service), &target);
    if (rc != HYBBX_OK) {
        return rc;
    }

    snprintf(buf, sizeof(buf), "Demoted '%s' from %s to user.",
             target.username, hybbx_user_level_name(old_level));
    hybbx_session_write_line(session, buf);
    return HYBBX_OK;
}

static hybbx_result_t cmd_delete(hybbx_service_t *service,
                                 hybbx_session_t *session,
                                 const hybbx_parsed_command_t *cmd)
{
    hybbx_user_record_t target;
    hybbx_user_level_t actor_level;
    char buf[128];
    hybbx_result_t rc;

    if (cmd->argc < 1 || cmd->argv[0] == NULL || cmd->argv[0][0] == '\0') {
        hybbx_session_write_line(session, "Usage: /delete <username>");
        return HYBBX_OK;
    }

    actor_level = hybbx_session_user_level(session);

    rc = cmd_lookup_user(service, cmd->argv[0], &target);
    if (rc == HYBBX_ERR_NOT_FOUND) {
        hybbx_session_write_line(session, "Unknown user.");
        return HYBBX_OK;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (str_ieq(target.username, hybbx_session_username(session))) {
        hybbx_session_write_line(session, "You cannot delete your own account.");
        return HYBBX_OK;
    }

    if (hybbx_user_level_is_sysop(target.level)) {
        hybbx_session_write_line(session,
            "The Sysop account is permanent and cannot be deleted.");
        return HYBBX_OK;
    }

    if (!hybbx_auth_may_delete(actor_level, target.level)) {
        if (actor_level == HYBBX_LEVEL_MOD || actor_level == HYBBX_LEVEL_USER) {
            cmd_deny_privilege(session);
        } else if (target.level == HYBBX_LEVEL_ADMIN) {
            hybbx_session_write_line(session,
                "Admins cannot delete other Admins.");
        } else {
            cmd_deny_privilege(session);
        }
        return HYBBX_ERR_DENIED;
    }

    rc = hybbx_storage_delete_user(hybbx_service_get_storage(service), target.id);
    if (rc != HYBBX_OK) {
        return rc;
    }

    snprintf(buf, sizeof(buf), "Deleted account '%s'.", target.username);
    hybbx_session_write_line(session, buf);
    return HYBBX_OK;
}

static hybbx_result_t cmd_deleteme(hybbx_service_t *service,
                                   hybbx_session_t *session,
                                   const hybbx_parsed_command_t *cmd)
{
    const hybbx_session_record_t *rec;
    hybbx_user_level_t level;
    hybbx_result_t rc;

    (void)service;

    level = hybbx_session_user_level(session);
    if (hybbx_user_level_is_sysop(level)) {
        hybbx_session_write_line(session,
            "The Sysop account is permanent and cannot be deleted.");
        return HYBBX_OK;
    }

    if (!cmd_deleteme_confirmed(cmd->argc > 0 ? cmd->argv[0] : NULL)) {
        hybbx_session_write_line(session,
            "Confirmation required: use /deleteme yes or /deleteme true.");
        return HYBBX_OK;
    }

    rec = hybbx_session_record(session);
    if (rec == NULL || rec->user_id == 0) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_storage_delete_user(hybbx_service_get_storage(service),
                                   rec->user_id);
    if (rc == HYBBX_ERR_NOT_FOUND) {
        hybbx_session_write_line(session, "Account not found.");
        return HYBBX_OK;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    hybbx_session_write_line(session, "Account deleted. Goodbye.");
    return HYBBX_SESSION_END;
}

static hybbx_result_t cmd_register(hybbx_service_t *service,
                                   hybbx_session_t *session,
                                   const hybbx_parsed_command_t *cmd)
{
    hybbx_storage_t *storage;
    hybbx_user_registration_t reg;
    hybbx_user_record_t user;
    char buf[128];
    hybbx_result_t rc;

    if (cmd->argc < 5) {
        hybbx_session_write_line(session,
            "Usage: /register <username> <full-name> <country> <location> <email>");
        return HYBBX_OK;
    }

    rc = parse_registration(cmd, &reg);
    if (rc != HYBBX_OK) {
        hybbx_session_write_line(session, "Registration fields too long.");
        return HYBBX_OK;
    }

    storage = hybbx_service_get_storage(service);
    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_storage_register_user(storage, &reg, &user);
    if (rc == HYBBX_ERR_BUSY) {
        hybbx_session_write_line(session, "Username already taken.");
        return HYBBX_OK;
    }
    if (rc == HYBBX_ERR_INVALID) {
        hybbx_session_write_line(session,
            "Invalid username (4-12 chars, one _ or one -, max 4 digits) or bad profile.");
        return HYBBX_OK;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    snprintf(buf, sizeof(buf), "Registration received for '%s'.",
             user.username);
    hybbx_session_write_line(session, buf);
    snprintf(buf, sizeof(buf), "Name: %s", user.full_name);
    hybbx_session_write_line(session, buf);
    hybbx_session_write_line(session,
        "A Sysop or Admin must activate your account before you can log in.");
    return HYBBX_OK;
}

static hybbx_result_t cmd_login(hybbx_service_t *service,
                                hybbx_session_t *session,
                                const hybbx_parsed_command_t *cmd)
{
    hybbx_storage_t *storage;
    hybbx_user_record_t user;
    hybbx_result_t rc;

    if (cmd->argc < 2 || cmd->argv[0] == NULL || cmd->argv[0][0] == '\0' ||
        cmd->argv[1] == NULL || cmd->argv[1][0] == '\0') {
        hybbx_session_write_line(session, "Usage: /login <username> <password>");
        return HYBBX_OK;
    }

    storage = hybbx_service_get_storage(service);
    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    {
        char username[HYBBX_USER_NAME_MAX];

        hybbx_strlcpy(username, cmd->argv[0], sizeof(username));
        hybbx_username_normalize(username);

        rc = hybbx_storage_find_user(storage, username, &user);
    }

    if (rc == HYBBX_ERR_NOT_FOUND) {
        hybbx_session_write_line(session, "Unknown user.");
        return HYBBX_OK;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (hybbx_user_level_is_guest(user.level)) {
        hybbx_session_write_line(session, "Use your guest session as-is.");
        return HYBBX_OK;
    }

    if (!user.active) {
        hybbx_session_write_line(session,
            "Account pending activation by Sysop or Admin.");
        return HYBBX_OK;
    }

    if (!hybbx_password_match(user.password, cmd->argv[1])) {
        hybbx_session_write_line(session, "Invalid password.");
        return HYBBX_OK;
    }

    rc = hybbx_session_switch_user(session, &user);
    if (rc != HYBBX_OK) {
        hybbx_session_write_line(session, "Login failed.");
        return rc;
    }

    hybbx_session_write_line(session, "Login successful.");
    hybbx_session_show_prompt(session);
    return HYBBX_OK;
}

static hybbx_result_t cmd_chat(hybbx_service_t *service,
                               hybbx_session_t *session,
                               const hybbx_parsed_command_t *cmd)
{
    const hybbx_chat_config_t *chat;
    unsigned channel_index;
    unsigned current;
    const char *name;
    char line[HYBBX_CHAT_CHANNEL_NAME_MAX + 48];
    hybbx_result_t rc;

    if (hybbx_session_is_guest(session)) {
        hybbx_session_write_line(session, "Guests cannot use chat.");
        return HYBBX_ERR_DENIED;
    }

    chat = hybbx_service_get_chat(service);
    if (chat == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (cmd->argc == 0) {
        hybbx_chat_list_channels(session, chat);
        return HYBBX_OK;
    }

    rc = hybbx_chat_resolve_channel(chat, cmd->argv[0], &channel_index);
    if (rc == HYBBX_ERR_NOT_FOUND) {
        hybbx_session_write_line(session, "Unknown chat channel.");
        return HYBBX_OK;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    current = hybbx_session_chat_channel(session);
    if (current == channel_index) {
        snprintf(line, sizeof(line), "Already in channel %u %s.",
                 channel_index,
                 hybbx_chat_channel_name(chat, channel_index));
        hybbx_session_write_line(session, line);
        return HYBBX_OK;
    }

    rc = hybbx_session_join_chat_channel(session, channel_index);
    if (rc != HYBBX_OK) {
        return rc;
    }

    name = hybbx_chat_channel_name(chat, channel_index);
    snprintf(line, sizeof(line), "Channel %u: %s", channel_index, name);
    hybbx_session_write_line(session, line);
    hybbx_session_write_line(session,
        "Each line is a message; /leave or /main to exit.");
    return HYBBX_OK;
}

static void mail_join_subject(const hybbx_parsed_command_t *cmd,
                              char *out, size_t out_len)
{
    size_t i;
    size_t pos = 0;

    if (out == NULL || out_len == 0) {
        return;
    }

    out[0] = '\0';
    if (cmd == NULL || cmd->argc < 3) {
        return;
    }

    for (i = 2; i < cmd->argc; i++) {
        size_t part_len;

        if (cmd->argv[i] == NULL) {
            continue;
        }

        if (pos > 0) {
            if (pos + 1 >= out_len) {
                break;
            }
            out[pos++] = ' ';
            out[pos] = '\0';
        }

        part_len = strlen(cmd->argv[i]);
        if (pos + part_len >= out_len) {
            part_len = out_len - pos - 1;
        }
        memcpy(out + pos, cmd->argv[i], part_len);
        pos += part_len;
        out[pos] = '\0';
    }
}

static hybbx_result_t cmd_mail(hybbx_service_t *service,
                               hybbx_session_t *session,
                               const hybbx_parsed_command_t *cmd)
{
    char subject[HYBBX_MAIL_SUBJECT_MAX + 1];
    hybbx_result_t rc;

    if (hybbx_session_is_guest(session)) {
        hybbx_session_write_line(session, "Guests cannot use mail.");
        return HYBBX_ERR_DENIED;
    }

    if (cmd->argc == 0) {
        hybbx_mail_list_inbox(service, session);
        return HYBBX_OK;
    }

    if (str_ieq(cmd->argv[0], "list")) {
        unsigned from = 1;
        unsigned to = 0;

        if (cmd->argc >= 2) {
            if (!hybbx_mail_parse_list_range(cmd->argv[1], &from, &to)) {
                hybbx_session_write_line(session,
                    "Usage: /mail list [<from>-<to>]   e.g. list 1-15  list 5-20");
                return HYBBX_OK;
            }
        }

        hybbx_mail_list_inbox_range(service, session, from, to);
        return HYBBX_OK;
    }

    if (str_ieq(cmd->argv[0], "delete") || str_ieq(cmd->argv[0], "del")) {
        unsigned from = 1;
        unsigned to = 0;

        if (cmd->argc < 2 || cmd->argv[1] == NULL) {
            hybbx_session_write_line(session,
                "Usage: /mail delete <n|from-to>   e.g. delete 3  delete 5-20");
            return HYBBX_OK;
        }

        if (!hybbx_mail_parse_list_range(cmd->argv[1], &from, &to)) {
            hybbx_session_write_line(session,
                "Usage: /mail delete <n|from-to>   e.g. delete 3  delete 5-20");
            return HYBBX_OK;
        }

        return hybbx_mail_delete_range(service, session, from, to);
    }

    if (str_ieq(cmd->argv[0], "recycle")) {
        return hybbx_mail_recycle_empty(service, session);
    }

    if (str_ieq(cmd->argv[0], "read")) {
        unsigned index;

        if (cmd->argc < 2 || cmd->argv[1] == NULL) {
            hybbx_session_write_line(session, "Usage: /mail read <n>");
            return HYBBX_OK;
        }

        index = (unsigned)strtoul(cmd->argv[1], NULL, 10);
        if (index == 0) {
            hybbx_session_write_line(session, "Usage: /mail read <n>");
            return HYBBX_OK;
        }

        return hybbx_mail_read(service, session, index);
    }

    if (str_ieq(cmd->argv[0], "send")) {
        if (cmd->argc < 3) {
            hybbx_session_write_line(session,
                "Usage: /mail send <user> <subject>");
            return HYBBX_OK;
        }

        mail_join_subject(cmd, subject, sizeof(subject));
        if (subject[0] == '\0') {
            hybbx_session_write_line(session,
                "Usage: /mail send <user> <subject>");
            return HYBBX_OK;
        }

        rc = hybbx_session_mail_compose_start(session, cmd->argv[1], subject);
        if (rc == HYBBX_ERR_INVALID) {
            hybbx_session_write_line(session, "Subject too long.");
            return HYBBX_OK;
        }
        if (rc != HYBBX_OK) {
            return rc;
        }

        hybbx_session_write_line(session, "Compose body. /mail done to send.");
        return HYBBX_OK;
    }

    if (str_ieq(cmd->argv[0], "done")) {
        const char *body;
        const char *to_user;
        const char *mail_subject;

        if (!hybbx_session_mail_composing(session)) {
            hybbx_session_write_line(session, "Not composing mail.");
            return HYBBX_OK;
        }

        to_user = hybbx_session_mail_compose_to(session);
        mail_subject = hybbx_session_mail_compose_subject(session);
        body = hybbx_session_mail_compose_body(session);

        rc = hybbx_mail_deliver(service, hybbx_session_username(session),
                                to_user, mail_subject, body);

        if (rc == HYBBX_ERR_NOT_FOUND) {
            hybbx_session_write_line(session, "Unknown recipient.");
            return HYBBX_OK;
        }
        if (rc == HYBBX_ERR_DENIED) {
            hybbx_session_write_line(session, "Cannot mail that user.");
            return HYBBX_OK;
        }
        if (rc != HYBBX_OK) {
            hybbx_session_write_line(session, "Send failed.");
            return rc;
        }

        hybbx_session_write_line(session, "Message sent.");
        hybbx_session_leave_area(session);
        hybbx_session_show_prompt(session);
        return HYBBX_OK;
    }

    if (str_ieq(cmd->argv[0], "cancel")) {
        if (hybbx_session_mail_composing(session)) {
            hybbx_session_leave_area(session);
            hybbx_session_write_line(session, "Compose cancelled.");
        } else {
            hybbx_session_write_line(session, "Not composing mail.");
        }
        return HYBBX_OK;
    }

    hybbx_session_write_line(session, "Unknown /mail subcommand. Try /help mail.");
    return HYBBX_OK;
}

static void cmd_emit_current_area(hybbx_session_t *session)
{
    const char *name = hybbx_session_area_name(hybbx_session_area(session));
    char line[64];

    snprintf(line, sizeof(line), "%c%s.",
             (char)toupper((unsigned char)name[0]), name + 1);
    hybbx_session_write_line(session, line);
    hybbx_session_show_prompt(session);
}

static hybbx_result_t cmd_leave(hybbx_session_t *session)
{
    hybbx_session_area_t before = hybbx_session_area(session);

    hybbx_session_leave_area(session);
    if (hybbx_session_area(session) != before) {
        cmd_emit_current_area(session);
    }

    return HYBBX_OK;
}

static hybbx_result_t cmd_main(hybbx_session_t *session)
{
    hybbx_session_area_t before = hybbx_session_area(session);

    hybbx_session_go_main(session);
    if (before != HYBBX_AREA_MAIN) {
        cmd_emit_current_area(session);
    }

    return HYBBX_OK;
}

static hybbx_result_t cmd_exit(hybbx_session_t *session)
{
    hybbx_session_write_line(session, "Goodbye.");
    return HYBBX_SESSION_END;
}

static hybbx_result_t cmd_version(hybbx_session_t *session)
{
    char buf[64];

    snprintf(buf, sizeof(buf), "HyBBX %s", HYBBX_VERSION_STRING);
    hybbx_session_write_line(session, buf);
    return HYBBX_OK;
}

static hybbx_result_t cmd_clear(hybbx_session_t *session)
{
    return hybbx_session_clear_terminal(session);
}

static hybbx_result_t cmd_echo(hybbx_session_t *session,
                             const hybbx_parsed_command_t *cmd)
{
    char buf[48];

    if (cmd->argc < 2) {
        snprintf(buf, sizeof(buf), "Input echo is %s.",
                 hybbx_bool_to_string(hybbx_session_input_echo(session)));
        hybbx_session_write_line(session, buf);
        return HYBBX_OK;
    }

    if (hybbx_bool_is_true(cmd->argv[1])) {
        return hybbx_session_set_input_echo(session, 1);
    }

    if (hybbx_bool_is_false(cmd->argv[1])) {
        return hybbx_session_set_input_echo(session, 0);
    }

    hybbx_session_write_line(session, "Usage: /echo [yes|no]");
    return HYBBX_ERR_INVALID;
}

hybbx_result_t hybbx_command_dispatch(hybbx_service_t *service,
                                        hybbx_session_t *session,
                                        const hybbx_parsed_command_t *cmd)
{
    if (cmd == NULL || service == NULL || session == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (cmd->scope != HYBBX_CMD_SCOPE_HYBBX) {
        return HYBBX_ERR_INVALID;
    }

    if (cmd->verb == NULL || cmd->verb[0] == '\0') {
        return cmd_help(session, cmd);
    }

    if (str_ieq(cmd->verb, "help") || str_ieq(cmd->verb, "?")) {
        return cmd_help(session, cmd);
    }

    {
        hybbx_result_t access = cmd_check_access(session, cmd->verb);
        if (access != HYBBX_OK) {
            return access;
        }
    }

    if (str_ieq(cmd->verb, "news")) {
        return cmd_news(service, session);
    }

    if (str_ieq(cmd->verb, "motd")) {
        return cmd_motd(service, session);
    }

    if (str_ieq(cmd->verb, "who")) {
        return cmd_who(session);
    }

    if (str_ieq(cmd->verb, "session") || str_ieq(cmd->verb, "info")) {
        return cmd_session(session);
    }

    if (str_ieq(cmd->verb, "version") || str_ieq(cmd->verb, "ver")) {
        return cmd_version(session);
    }

    if (str_ieq(cmd->verb, "leave") || str_ieq(cmd->verb, "back")) {
        return cmd_leave(session);
    }

    if (str_ieq(cmd->verb, "main") || str_ieq(cmd->verb, "menu")) {
        return cmd_main(session);
    }

    if (str_ieq(cmd->verb, "clear") || str_ieq(cmd->verb, "cls") ||
        str_ieq(cmd->verb, "reset")) {
        return cmd_clear(session);
    }

    if (str_ieq(cmd->verb, "echo")) {
        return cmd_echo(session, cmd);
    }

    if (str_ieq(cmd->verb, "chat")) {
        return cmd_chat(service, session, cmd);
    }

    if (str_ieq(cmd->verb, "mail")) {
        return cmd_mail(service, session, cmd);
    }

    if (str_ieq(cmd->verb, "login")) {
        return cmd_login(service, session, cmd);
    }

    if (str_ieq(cmd->verb, "register")) {
        return cmd_register(service, session, cmd);
    }

    if (str_ieq(cmd->verb, "activate")) {
        return cmd_activate(service, session, cmd);
    }

    if (str_ieq(cmd->verb, "promote")) {
        return cmd_promote(service, session, cmd);
    }

    if (str_ieq(cmd->verb, "demote")) {
        return cmd_demote(service, session, cmd);
    }

    if (str_ieq(cmd->verb, "delete") || str_ieq(cmd->verb, "del")) {
        return cmd_delete(service, session, cmd);
    }

    if (str_ieq(cmd->verb, "deleteme")) {
        return cmd_deleteme(service, session, cmd);
    }

    if (str_ieq(cmd->verb, "exit") || str_ieq(cmd->verb, "logout") ||
        str_ieq(cmd->verb, "bye") || str_ieq(cmd->verb, "quit")) {
        return cmd_exit(session);
    }

    hybbx_session_write_line(session, cmd_help_unknown(session));
    return HYBBX_ERR_NOT_FOUND;
}
