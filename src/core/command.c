#include "hybbx/hybbx.h"
#include "hybbx/command.h"
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "hybbx/texts.h"
#include "hybbx/storage.h"
#include "hybbx/auth.h"
#include "hybbx/chat.h"
#include "hybbx/util.h"

#include <stdio.h>
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
               str_ieq(verb, "login") || str_ieq(verb, "register");
    }

    if (str_ieq(verb, "news") || str_ieq(verb, "motd") ||
        str_ieq(verb, "who") || str_ieq(verb, "session") || str_ieq(verb, "info") ||
        str_ieq(verb, "version") || str_ieq(verb, "ver") ||
        str_ieq(verb, "leave") ||
        str_ieq(verb, "exit") || str_ieq(verb, "logout") ||
        str_ieq(verb, "bye") || str_ieq(verb, "quit") ||
        str_ieq(verb, "login") || str_ieq(verb, "register") ||
        str_ieq(verb, "deleteme")) {
        return 1;
    }

    if (str_ieq(verb, "chat")) {
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
    return arg != NULL && (str_ieq(arg, "yes") || str_ieq(arg, "true"));
}

static int cmd_help_shows_deleteme(hybbx_user_level_t level)
{
    return !hybbx_user_level_is_sysop(level) &&
           !hybbx_user_level_is_guest(level);
}

static const char *cmd_help_unknown(hybbx_session_t *session)
{
    if (hybbx_session_user_level(session) == HYBBX_LEVEL_GUEST) {
        return "Unknown command. Guests: /help, /motd, /news, /login, /register.";
    }

    return "Unknown command. Type /help for a list.";
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
            "Guests may only use /help, /motd, /news, /login, /register.");
    } else {
        hybbx_session_write_line(session, "Insufficient privileges.");
    }

    return HYBBX_ERR_DENIED;
}

static void cmd_help_list_for_level(hybbx_session_t *session)
{
    hybbx_user_level_t level = hybbx_session_user_level(session);

    hybbx_session_write_line(session, "HyBBX system commands (leading / required):");

    if (level == HYBBX_LEVEL_GUEST) {
        hybbx_session_write_line(session, "  /help  or / help");
        hybbx_session_write_line(session, "  /motd  or / motd");
        hybbx_session_write_line(session, "  /news  or / news");
        hybbx_session_write_line(session, "  /login or / login");
        hybbx_session_write_line(session, "  /register or / register");
        hybbx_session_write_line(session, "");
        hybbx_session_write_line(session, "Type /help <verb> for details on one command.");
        return;
    }

    hybbx_session_write_line(session, "  / or /command       Help (same as /help)");
    hybbx_session_write_line(session, "  /help  or / help");
    hybbx_session_write_line(session, "  /news  or / news");
    hybbx_session_write_line(session, "  /motd  or / motd");
    hybbx_session_write_line(session, "  /who   or / who");
    hybbx_session_write_line(session, "  /session or / session");
    hybbx_session_write_line(session, "  /version or / version");
    hybbx_session_write_line(session, "  /leave or / leave");
    hybbx_session_write_line(session, "  /chat  or / chat");
    hybbx_session_write_line(session, "  /exit  or / exit");
    hybbx_session_write_line(session, "  /login or / login");
    hybbx_session_write_line(session, "  /register or / register");

    if (cmd_help_shows_deleteme(level)) {
        hybbx_session_write_line(session, "  /deleteme yes");
    }

    if (hybbx_auth_may_activate(level)) {
        hybbx_session_write_line(session, "  /activate <user>");
    }

    if (level == HYBBX_LEVEL_SYSOP) {
        hybbx_session_write_line(session, "  /promote <user> admin|mod");
    } else if (level == HYBBX_LEVEL_ADMIN) {
        hybbx_session_write_line(session, "  /promote <user> mod");
    }

    if (level == HYBBX_LEVEL_SYSOP || level == HYBBX_LEVEL_ADMIN) {
        hybbx_session_write_line(session, "  /demote <user>");
        hybbx_session_write_line(session, "  /delete <user>");
    }

    hybbx_session_write_line(session, "");
    hybbx_session_write_line(session, "  /command <verb> is an alias for /<verb>");
    hybbx_session_write_line(session, "Type /help <verb> for details on one command.");
    hybbx_session_write_line(session,
        "Comments (; #) and other lines are ignored.");
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
    }

    if (!cmd_verb_allowed(level, canonical) ||
        (str_ieq(canonical, "deleteme") && !cmd_help_shows_deleteme(level))) {
        hybbx_session_write_line(session, cmd_help_unknown(session));
        return HYBBX_ERR_NOT_FOUND;
    }

    if (str_ieq(canonical, "help")) {
        hybbx_session_write_line(session, "/help - List or explain HyBBX commands");
        if (level == HYBBX_LEVEL_GUEST) {
            hybbx_session_write_line(session, "  /help             List guest commands");
            hybbx_session_write_line(session, "  / help            Same as /help");
            hybbx_session_write_line(session, "  /help <verb>      Explain one command");
        } else {
            hybbx_session_write_line(session, "  / or /command     Same as /help");
            hybbx_session_write_line(session, "  /help             List commands for your level");
            hybbx_session_write_line(session, "  / help            Same as /help");
            hybbx_session_write_line(session, "  /help <verb>      Explain one command");
            hybbx_session_write_line(session, "  /command <verb>   Alias for /<verb>");
        }
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "news")) {
        hybbx_session_write_line(session, "/news - Show system news");
        hybbx_session_write_line(session, "Displays news.txt from the configured texts directory.");
        hybbx_session_write_line(session, "Use this command anytime to read current HyBBX news.");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "motd")) {
        hybbx_session_write_line(session, "/motd - Show message of the day");
        hybbx_session_write_line(session, "Displays motd.txt again (also shown after guest login).");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "who")) {
        hybbx_session_write_line(session, "/who - Who is online");
        hybbx_session_write_line(session, "Lists users currently connected to this HyBBX node.");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "session")) {
        hybbx_session_write_line(session, "/session - Session information");
        hybbx_session_write_line(session, "Shows your username, session ID, and transport.");
        hybbx_session_write_line(session, "Alias: /info");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "version")) {
        hybbx_session_write_line(session, "/version - HyBBX version");
        hybbx_session_write_line(session, "Prints the running HyBBX software version.");
        hybbx_session_write_line(session, "Alias: /ver");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "leave")) {
        char line[128];

        hybbx_session_write_line(session, "/leave - Leave current area");
        snprintf(line, sizeof(line),
            "Returns from mail, chat, or other areas to %s.",
            hybbx_session_area_name(HYBBX_AREA_MAIN));
        hybbx_session_write_line(session, line);
        hybbx_session_write_line(session, "Does not close the connection.");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "chat")) {
        hybbx_session_write_line(session, "/chat - Enter a chat channel");
        hybbx_session_write_line(session, "  /chat              List channels");
        hybbx_session_write_line(session, "  /chat <number>     Join by channel number");
        hybbx_session_write_line(session, "  /chat <name>       Join by channel name");
        hybbx_session_write_line(session,
            "Channel name is the topic. Message length: see [chat] message_max in INI.");
        hybbx_session_write_line(session,
            "You see your lines as ME: …; others as <username>: …");
        hybbx_session_write_line(session, "Guests cannot use chat. /leave to exit.");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "exit")) {
        hybbx_session_write_line(session, "/exit - Close connection (logout)");
        hybbx_session_write_line(session, "Ends your session and disconnects.");
        hybbx_session_write_line(session, "Aliases: /quit, /logout, /bye");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "login")) {
        hybbx_session_write_line(session, "/login - Log in as a registered user");
        hybbx_session_write_line(session, "  /login <username> <password>");
        hybbx_session_write_line(session,
            "Account must be activated by Sysop or Admin.");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "register")) {
        hybbx_session_write_line(session, "/register - Create a user account");
        hybbx_session_write_line(session,
            "  /register <username> <full-name> <country> <location> <email>");
        hybbx_session_write_line(session,
            "Username: 4-12 chars (a-z, 0-9), one _ or one - only, max 4 digits.");
        hybbx_session_write_line(session,
            "Multi-word full names are supported (e.g. /register alice John Doe Germany Berlin a@b.c).");
        hybbx_session_write_line(session,
            "Creates a pending account; Sysop or Admin must activate it.");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "activate")) {
        hybbx_session_write_line(session,
            "/activate - Activate a pending registered account");
        hybbx_session_write_line(session, "  /activate <username>");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "promote")) {
        hybbx_session_write_line(session,
            "/promote - Promote a user to Mod or Admin");
        if (level == HYBBX_LEVEL_SYSOP) {
            hybbx_session_write_line(session, "  /promote <username> admin|mod");
            hybbx_session_write_line(session,
                "Sysop may add Admins and Mods.");
        } else {
            hybbx_session_write_line(session, "  /promote <username> mod");
            hybbx_session_write_line(session,
                "Admin may add Mods.");
        }
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "demote")) {
        hybbx_session_write_line(session,
            "/demote - Remove Admin or Mod privileges");
        hybbx_session_write_line(session, "  /demote <username>");
        if (level == HYBBX_LEVEL_SYSOP) {
            hybbx_session_write_line(session,
                "Sysop may remove Admins and Mods.");
        } else {
            hybbx_session_write_line(session,
                "Admin may remove Mods.");
        }
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "deleteme")) {
        hybbx_session_write_line(session,
            "/deleteme - Permanently delete your own account");
        hybbx_session_write_line(session, "  /deleteme yes");
        hybbx_session_write_line(session, "  /deleteme true     (alias for yes)");
        hybbx_session_write_line(session,
            "Without yes or true, deletion is not performed.");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "delete")) {
        hybbx_session_write_line(session,
            "/delete - Permanently remove a user account");
        hybbx_session_write_line(session, "  /delete <username>");
        hybbx_session_write_line(session,
            "The Sysop account is permanent and cannot be deleted.");
        if (level == HYBBX_LEVEL_SYSOP) {
            hybbx_session_write_line(session,
                "Sysop may delete Admins, Mods, Users, and Guests (not self).");
        } else {
            hybbx_session_write_line(session,
                "Admin may delete Mods, Users, and Guests (not Admins or Sysop).");
        }
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
    snprintf(line, sizeof(line),
        "Each line is a message; /leave to return to %s.",
        hybbx_session_area_name(HYBBX_AREA_MAIN));
    hybbx_session_write_line(session, line);
    return HYBBX_OK;
}

static hybbx_result_t cmd_leave(hybbx_session_t *session)
{
    hybbx_session_area_t area = hybbx_session_area(session);
    char line[64];

    hybbx_session_leave_area(session);
    if (area != HYBBX_AREA_MAIN) {
        const char *name = hybbx_session_area_name(HYBBX_AREA_MAIN);

        snprintf(line, sizeof(line), "%c%s.",
                 (char)toupper((unsigned char)name[0]), name + 1);
        hybbx_session_write_line(session, line);
        hybbx_session_show_prompt(session);
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

    if (str_ieq(cmd->verb, "leave")) {
        return cmd_leave(session);
    }

    if (str_ieq(cmd->verb, "chat")) {
        return cmd_chat(service, session, cmd);
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
