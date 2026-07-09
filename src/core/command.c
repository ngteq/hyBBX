#include "hybbx/hybbx.h"
#include "hybbx/command.h"
#include "hybbx/service.h"
#include "hybbx/session.h"
#include "hybbx/texts.h"
#include "hybbx/storage.h"
#include "hybbx/auth.h"
#include "hybbx/chat.h"
#include "hybbx/conference.h"
#include "hybbx/mail.h"
#include "hybbx/broadcast.h"
#include "hybbx/security.h"
#include "hybbx/service.h"
#include "hybbx/password.h"
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

static void cmd_deny_privilege(hybbx_session_t *session);

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
               str_ieq(verb, "rules") || str_ieq(verb, "legal") ||
               str_ieq(verb, "login") || str_ieq(verb, "register") ||
               str_ieq(verb, "clear") || str_ieq(verb, "cls") ||
               str_ieq(verb, "reset") || str_ieq(verb, "echo") ||
               str_ieq(verb, "exit") || str_ieq(verb, "logout") ||
               str_ieq(verb, "bye") || str_ieq(verb, "quit");
    }

    if (str_ieq(verb, "news") || str_ieq(verb, "motd") ||
        str_ieq(verb, "rules") || str_ieq(verb, "legal") ||
        str_ieq(verb, "who") || str_ieq(verb, "online") || str_ieq(verb, "users") ||
        str_ieq(verb, "session") || str_ieq(verb, "info") ||
        str_ieq(verb, "version") || str_ieq(verb, "ver") ||
        str_ieq(verb, "leave") || str_ieq(verb, "back") ||
        str_ieq(verb, "main") || str_ieq(verb, "menu") ||
        str_ieq(verb, "clear") || str_ieq(verb, "cls") ||
        str_ieq(verb, "reset") || str_ieq(verb, "echo") ||
        str_ieq(verb, "exit") || str_ieq(verb, "logout") ||
        str_ieq(verb, "bye") || str_ieq(verb, "quit") ||
        str_ieq(verb, "login") ||
        str_ieq(verb, "deleteme")) {
        return 1;
    }

    if (str_ieq(verb, "register")) {
        return hybbx_auth_may_register(level);
    }

    if (str_ieq(verb, "usercreate") || str_ieq(verb, "createuser")) {
        return hybbx_auth_may_create_user(level);
    }

    if (str_ieq(verb, "changeme")) {
        return hybbx_auth_may_changeme(level);
    }

    if (str_ieq(verb, "changeuser") || str_ieq(verb, "userchange")) {
        return hybbx_user_level_is_sysop_or_admin(level);
    }

    if (str_ieq(verb, "deleteuser") || str_ieq(verb, "userdelete")) {
        return hybbx_user_level_is_sysop(level);
    }

    if (str_ieq(verb, "chat")) {
        return !hybbx_user_level_is_guest(level);
    }

    if (str_ieq(verb, "conference") || str_ieq(verb, "meeting")) {
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

    if (str_ieq(verb, "shutdown") || str_ieq(verb, "restart")) {
        return hybbx_user_level_is_sysop(level);
    }

    if (str_ieq(verb, "broadcast")) {
        return hybbx_user_level_is_sysop(level);
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
    if (hybbx_session_is_guest(session) ||
        (hybbx_session_login_prompt(session) &&
         !hybbx_session_logged_in(session))) {
        return "Unknown command. Try /help.";
    }

    return "Unknown command. /help for list.";
}

static int cmd_verb_allowed_login_prompt(const char *verb)
{
    if (verb == NULL || verb[0] == '\0') {
        return 1;
    }

    if (cmd_verb_allowed(HYBBX_LEVEL_GUEST, verb)) {
        return 1;
    }

    return str_ieq(verb, "exit") || str_ieq(verb, "logout") ||
           str_ieq(verb, "bye") || str_ieq(verb, "quit");
}

static hybbx_result_t cmd_check_access(hybbx_session_t *session,
                                       const char *verb)
{
    hybbx_user_level_t level = hybbx_session_user_level(session);

    if (!hybbx_session_logged_in(session) &&
        hybbx_session_login_prompt(session)) {
        if (cmd_verb_allowed_login_prompt(verb)) {
            return HYBBX_OK;
        }
        hybbx_session_write_line(session,
            "Log in with /login or use /register.");
        return HYBBX_ERR_DENIED;
    }

    if (cmd_verb_allowed(level, verb)) {
        return HYBBX_OK;
    }

    if (level == HYBBX_LEVEL_GUEST) {
        hybbx_session_write_line(session,
            "Not available to guests. Try /help.");
    } else if (str_ieq(verb, "register")) {
        hybbx_session_write_line(session,
            "Only guests may self-register with /register.");
    } else if (str_ieq(verb, "changeme")) {
        hybbx_session_write_line(session,
            "Only registered users may use /changeme.");
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

static void cmd_help_continuation(hybbx_session_t *session, const char *cmds)
{
    char buf[HYBBX_LINE_MAX];

    snprintf(buf, sizeof(buf), "             %s", cmds);
    hybbx_session_write_line(session, buf);
}

static void cmd_help_staff(hybbx_session_t *session, hybbx_user_level_t level)
{
    if (hybbx_auth_may_create_user(level)) {
        cmd_help_group(session, "Staff",
                       "/usercreate  /activate  /changeuser");
        if (level == HYBBX_LEVEL_SYSOP) {
            cmd_help_continuation(session,
                "/promote  /demote  /delete  /deleteuser");
            cmd_help_continuation(session, "/shutdown  /restart  /broadcast");
        } else {
            cmd_help_continuation(session, "/promote  /demote  /delete");
        }
    } else if (hybbx_auth_may_activate(level)) {
        cmd_help_group(session, "Staff", "/activate");
    }
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

    hybbx_session_write_line(session,
        "HyBBX commands (/ required)  —  /help <cmd> for details");

    if (level == HYBBX_LEVEL_GUEST ||
        (hybbx_session_login_prompt(session) &&
         !hybbx_session_logged_in(session))) {
        cmd_help_pair(session, "/help", "list or explain",
                      "/motd", "daily message");
        cmd_help_pair(session, "/news", "system news",
                      "/rules", "legal notice");
        cmd_help_pair(session, "/login", "<user> <pass>",
                      "/register", "new account");
        cmd_help_pair(session, "/clear", "clear screen",
                      "/echo", "input echo");
        cmd_help_line(session, "/exit", "close connection");
        return;
    }

    cmd_help_group(session, "General",
                   "/help  /news  /motd  /rules  /who  /users  /session  /version");
    cmd_help_group(session, "Screen", "/clear  /echo");
    cmd_help_group(session, "Areas",
                   "/leave  /main  /chat  /conference  /mail  /exit");
    if (cmd_help_shows_deleteme(level)) {
        cmd_help_group(session, "Account",
                       "/login  /changeme  /deleteme");
    } else {
        cmd_help_group(session, "Account", "/login  /changeme");
    }

    cmd_help_staff(session, level);
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
    } else if (str_ieq(topic, "online")) {
        canonical = "who";
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
    } else if (str_ieq(topic, "legal")) {
        canonical = "rules";
    } else if (str_ieq(topic, "meeting")) {
        canonical = "conference";
    } else if (str_ieq(topic, "createuser")) {
        canonical = "usercreate";
    } else if (str_ieq(topic, "userchange")) {
        canonical = "changeuser";
    } else if (str_ieq(topic, "userdelete")) {
        canonical = "deleteuser";
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
        cmd_help_topic_title(session, "/motd", "daily message");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "rules")) {
        cmd_help_topic_title(session, "/rules",
                             "legal notice and acceptable use (rules.txt)");
        cmd_help_topic_detail(session, "  alias: /legal");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "who")) {
        cmd_help_topic_title(session, "/who",
                             "online users and connection type");
        cmd_help_topic_detail(session, "  alias: /online");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "users")) {
        cmd_help_topic_title(session, "/users",
                             "registered user levels as percentages");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "session")) {
        cmd_help_topic_title(session, "/session",
                             "username, session id, transport");
        cmd_help_topic_detail(session, "  alias: /info");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "version")) {
        cmd_help_topic_title(session, "/version",
                             "HyBBX version and host OS name");
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
            "  /chat show         users in current channel");
        cmd_help_topic_detail(session,
            "  /chat showall      all users@ChannelN (80 columns)");
        cmd_help_topic_detail(session,
            "  registered users only — /leave or /main to exit");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "conference")) {
        cmd_help_topic_title(session, "/conference",
                             "private two-user channel");
        cmd_help_topic_detail(session, "  alias: /meeting");
        cmd_help_topic_detail(session,
            "  /conference <topic> <user>  (invite; partner accepts y/n)");
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
        cmd_help_topic_title(session, "/login", "log in (registered accounts)");
        cmd_help_topic_detail(session,
            "  /login <username> <password>");
        cmd_help_topic_detail(session,
            "  guests use auto-login on connect; not available via /login");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "changeme")) {
        cmd_help_topic_title(session, "/changeme", "update own profile and password");
        cmd_help_topic_detail(session,
            "  /changeme <oldpass> <newpass> <name> <country> <location> <email>");
        cmd_help_topic_detail(session,
            "  newpass: 8-24 characters");
        cmd_help_topic_detail(session,
            "  username cannot be changed");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "register")) {
        cmd_help_topic_title(session, "/register", "self-registration (guests)");
        cmd_help_topic_detail(session,
            "  /register <user> <name> <country> <location> <email> <password>");
        cmd_help_topic_detail(session,
            "  password: 8-24 characters");
        cmd_help_topic_detail(session,
            "  guests only — pending until Sysop or Admin /activate");
        cmd_help_topic_detail(session,
            "  Sysop and Admin receive mail with all /register fields");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "usercreate")) {
        if (!hybbx_auth_may_create_user(level)) {
            cmd_deny_privilege(session);
            return HYBBX_OK;
        }
        cmd_help_topic_title(session, "/usercreate",
                             "create user account (Sysop, Admin)");
        cmd_help_topic_detail(session,
            "  /usercreate <user> <name> <country> <location> <email>");
        cmd_help_topic_detail(session,
            "  alias: /createuser");
        cmd_help_topic_detail(session,
            "  account is inactive until /activate");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "activate")) {
        if (!hybbx_auth_may_activate(level)) {
            cmd_deny_privilege(session);
            return HYBBX_OK;
        }
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
        cmd_help_topic_detail(session, "  /deleteme yes|no");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "delete")) {
        cmd_help_topic_title(session, "/delete",
                             "permanently remove account (staff)");
        cmd_help_topic_detail(session, "  /delete <username>");
        cmd_help_topic_detail(session, "  sysop account cannot be deleted");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "changeuser")) {
        cmd_help_topic_title(session, "/changeuser",
                             "overwrite user profile and password (staff)");
        cmd_help_topic_detail(session,
            "  /changeuser <user> <newpass> <name> <country> <location> <email>");
        cmd_help_topic_detail(session, "  alias: /userchange");
        cmd_help_topic_detail(session,
            "  newpass: 8-24 characters");
        if (level == HYBBX_LEVEL_ADMIN) {
            cmd_help_topic_detail(session,
                "  Admin: Mod and User only");
        } else {
            cmd_help_topic_detail(session,
                "  Sysop: Admin, Mod, and User");
        }
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "deleteuser")) {
        cmd_help_topic_title(session, "/deleteuser",
                             "delete any account except Sysop (Sysop only)");
        cmd_help_topic_detail(session, "  /deleteuser <username>");
        cmd_help_topic_detail(session, "  alias: /userdelete");
        cmd_help_topic_detail(session,
            "  cannot delete your own account or the Sysop account");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "shutdown")) {
        if (!hybbx_user_level_is_sysop(level)) {
            cmd_deny_privilege(session);
            return HYBBX_OK;
        }
        cmd_help_topic_title(session, "/shutdown", "stop the HyBBX daemon (Sysop)");
        cmd_help_topic_detail(session, "  /shutdown");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "restart")) {
        if (!hybbx_user_level_is_sysop(level)) {
            cmd_deny_privilege(session);
            return HYBBX_OK;
        }
        cmd_help_topic_title(session, "/restart",
                             "restart the HyBBX daemon (Sysop)");
        cmd_help_topic_detail(session, "  /restart");
        return HYBBX_OK;
    }

    if (str_ieq(canonical, "broadcast")) {
        if (!hybbx_user_level_is_sysop(level)) {
            cmd_deny_privilege(session);
            return HYBBX_OK;
        }
        cmd_help_topic_title(session, "/broadcast",
                             "AX.25 RF or TCP broadcast (Main only; Sysop)");
        cmd_help_topic_detail(session,
            "  /broadcast ax25 list");
        cmd_help_topic_detail(session,
            "  /broadcast ax25 <MHz|all> <message>  (low+half-duplex QoS; "
            "max 48 chars; 5 min minimum between AX.25 sends)");
        cmd_help_topic_detail(session,
            "  /broadcast tcp <message>   (stub — logged only)");
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

    if (hybbx_texts_send_motd(texts, session) != HYBBX_OK) {
        hybbx_session_write_line(session, "(no motd available)");
    }

    return HYBBX_OK;
}

static hybbx_result_t cmd_rules(hybbx_service_t *service, hybbx_session_t *session)
{
    const hybbx_texts_config_t *texts = hybbx_service_get_texts(service);

    if (texts == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (hybbx_texts_send_file(texts, session, HYBBX_TEXT_RULES) != HYBBX_OK) {
        hybbx_session_write_line(session, "(no rules available)");
    }

    return HYBBX_OK;
}

static const char *who_transport_label(const char *transport)
{
    if (transport == NULL || transport[0] == '\0') {
        return "unknown";
    }

    if (str_ieq(transport, "packet_radio")) {
        return "ax25";
    }
    if (str_ieq(transport, "telnet")) {
        return "telnet";
    }
    if (str_ieq(transport, "circuit")) {
        return "circuit";
    }
    if (str_ieq(transport, "ssh")) {
        return "ssh";
    }
    if (str_ieq(transport, "websocket")) {
        return "websocket";
    }

    return transport;
}

typedef struct who_ctx {
    hybbx_session_t *requester;
    unsigned count;
} who_ctx_t;

static void who_list_visitor(hybbx_session_t *session, void *userdata)
{
    who_ctx_t *ctx = (who_ctx_t *)userdata;
    const hybbx_session_record_t *rec;
    const char *name;
    const char *transport;
    char line[96];

    if (ctx == NULL || session == NULL || !hybbx_session_logged_in(session)) {
        return;
    }

    name = hybbx_session_display_name(session);
    rec = hybbx_session_record(session);
    transport = who_transport_label(rec != NULL ? rec->transport : NULL);
    if (rec == NULL || rec->transport[0] == '\0') {
        transport = who_transport_label(session->transport != NULL ?
                                        session->transport->name : NULL);
    }

    snprintf(line, sizeof(line), "  %-16s  %s", name, transport);
    hybbx_session_write_line(ctx->requester, line);
    ctx->count++;
}

static hybbx_result_t cmd_who(hybbx_service_t *service, hybbx_session_t *session)
{
    who_ctx_t ctx;
    char total[32];

    ctx.requester = session;
    ctx.count = 0;

    hybbx_session_write_line(session, "Online users:");
    hybbx_service_visit_sessions(service, who_list_visitor, &ctx);

    if (ctx.count == 0) {
        hybbx_session_write_line(session, "  (none)");
    }

    snprintf(total, sizeof(total), "Total: %u", ctx.count);
    hybbx_session_write_line(session, total);
    return HYBBX_OK;
}

typedef struct users_stats_ctx {
    size_t by_level[6];
    size_t total;
} users_stats_ctx_t;

static hybbx_result_t users_stats_cb(const hybbx_user_record_t *user, void *ctx)
{
    users_stats_ctx_t *stats = (users_stats_ctx_t *)ctx;

    if (user == NULL || stats == NULL) {
        return HYBBX_OK;
    }

    if (user->level < HYBBX_LEVEL_SYSOP || user->level > HYBBX_LEVEL_USER) {
        return HYBBX_OK;
    }

    stats->by_level[user->level]++;
    stats->total++;
    return HYBBX_OK;
}

static void users_compute_percents(const size_t *counts, size_t total,
                                   unsigned *percents)
{
    hybbx_user_level_t level;
    hybbx_user_level_t adjust_level = HYBBX_LEVEL_SYSOP;
    unsigned sum = 0;
    unsigned max_remainder = 0;

    if (total == 0) {
        return;
    }

    for (level = HYBBX_LEVEL_SYSOP; level <= HYBBX_LEVEL_USER; level++) {
        unsigned scaled = (unsigned)((counts[level] * 1000u) / total);
        unsigned rem = scaled % 10u;

        percents[level] = scaled / 10u;
        sum += percents[level];
        if (rem > max_remainder) {
            max_remainder = rem;
            adjust_level = level;
        }
    }

    if (sum < 100u) {
        percents[adjust_level] += 100u - sum;
    }
}

static hybbx_result_t cmd_users(hybbx_service_t *service, hybbx_session_t *session)
{
    hybbx_storage_t *storage;
    users_stats_ctx_t stats;
    unsigned percents[6];
    hybbx_user_level_t level;
    char line[48];
    char total[40];
    hybbx_result_t rc;

    storage = hybbx_service_get_storage(service);
    if (storage == NULL) {
        hybbx_session_write_line(session, "User storage unavailable.");
        return HYBBX_ERR_INVALID;
    }

    memset(&stats, 0, sizeof(stats));
    memset(percents, 0, sizeof(percents));

    rc = hybbx_storage_foreach_user(storage, users_stats_cb, &stats);
    if (rc != HYBBX_OK) {
        hybbx_session_write_line(session, "Could not read user database.");
        return rc;
    }

    hybbx_session_write_line(session, "Registered users:");

    if (stats.total == 0) {
        hybbx_session_write_line(session, "  (none)");
    } else {
        users_compute_percents(stats.by_level, stats.total, percents);
        for (level = HYBBX_LEVEL_SYSOP; level <= HYBBX_LEVEL_USER; level++) {
            if (stats.by_level[level] == 0) {
                continue;
            }

            snprintf(line, sizeof(line), "  %-8s %3u%%",
                     hybbx_user_level_name(level), percents[level]);
            hybbx_session_write_line(session, line);
        }
    }

    snprintf(total, sizeof(total), "Total users: %zu", stats.total);
    hybbx_session_write_line(session, total);
    return HYBBX_OK;
}

static hybbx_result_t cmd_session(hybbx_session_t *session)
{
    char buf[128];
    const hybbx_session_record_t *rec = hybbx_session_record(session);

    if (rec == NULL) {
        return HYBBX_ERR_INVALID;
    }

    snprintf(buf, sizeof(buf), "User: %s", hybbx_session_display_name(session));
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

static hybbx_result_t parse_profile_fields(const hybbx_parsed_command_t *cmd,
                                           unsigned name_start,
                                           unsigned tail_extra,
                                           hybbx_user_registration_t *reg)
{
    size_t i;
    size_t pos = 0;
    unsigned suffix;

    if (cmd == NULL || reg == NULL) {
        return HYBBX_ERR_INVALID;
    }

    suffix = 3u + tail_extra;
    if (cmd->argc < name_start + suffix) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_strlcpy(reg->email, cmd->argv[cmd->argc - 1u - tail_extra],
                  sizeof(reg->email));
    hybbx_strlcpy(reg->location, cmd->argv[cmd->argc - 2u - tail_extra],
                  sizeof(reg->location));
    hybbx_strlcpy(reg->country, cmd->argv[cmd->argc - 3u - tail_extra],
                  sizeof(reg->country));

    reg->full_name[0] = '\0';
    for (i = name_start; i + suffix < cmd->argc; i++) {
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

static hybbx_result_t parse_registration(const hybbx_parsed_command_t *cmd,
                                         hybbx_user_registration_t *reg,
                                         int with_password)
{
    hybbx_result_t rc;
    unsigned min_args = with_password ? 6u : 5u;
    unsigned tail_extra = with_password ? 1u : 0u;

    if (cmd == NULL || reg == NULL || cmd->argc < min_args) {
        return HYBBX_ERR_INVALID;
    }

    memset(reg, 0, sizeof(*reg));
    hybbx_strlcpy(reg->nickname, cmd->argv[0], sizeof(reg->nickname));
    hybbx_strlcpy(reg->username, cmd->argv[0], sizeof(reg->username));
    hybbx_username_normalize(reg->username);

    if (with_password) {
        hybbx_strlcpy(reg->password, cmd->argv[cmd->argc - 1],
                      sizeof(reg->password));
    }

    rc = parse_profile_fields(cmd, 1, tail_extra, reg);
    if (rc != HYBBX_OK) {
        return rc;
    }

    return HYBBX_OK;
}

static hybbx_result_t parse_changeme(const hybbx_parsed_command_t *cmd,
                                     const char *username,
                                     hybbx_user_registration_t *reg,
                                     const char **old_password,
                                     const char **new_password)
{
    hybbx_result_t rc;

    if (cmd == NULL || username == NULL || reg == NULL ||
        old_password == NULL || new_password == NULL || cmd->argc < 6) {
        return HYBBX_ERR_INVALID;
    }

    memset(reg, 0, sizeof(*reg));
    hybbx_strlcpy(reg->username, username, sizeof(reg->username));
    hybbx_username_normalize(reg->username);
    *old_password = cmd->argv[0];
    *new_password = cmd->argv[1];

    rc = parse_profile_fields(cmd, 2, 0, reg);
    if (rc != HYBBX_OK) {
        return rc;
    }

    return HYBBX_OK;
}

static hybbx_result_t parse_userchange(const hybbx_parsed_command_t *cmd,
                                       hybbx_user_registration_t *reg,
                                       const char **new_password)
{
    hybbx_result_t rc;

    if (cmd == NULL || reg == NULL || new_password == NULL || cmd->argc < 7) {
        return HYBBX_ERR_INVALID;
    }

    memset(reg, 0, sizeof(*reg));
    hybbx_strlcpy(reg->username, cmd->argv[0], sizeof(reg->username));
    hybbx_username_normalize(reg->username);
    *new_password = cmd->argv[1];

    rc = parse_profile_fields(cmd, 2, 0, reg);
    if (rc != HYBBX_OK) {
        return rc;
    }

    return HYBBX_OK;
}

static hybbx_result_t cmd_lookup_user(hybbx_service_t *service,
                                      const char *username,
                                      hybbx_user_record_t *out)
{
    hybbx_storage_t *storage;
    hybbx_result_t rc;

    if (service == NULL || username == NULL || out == NULL ||
        username[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    storage = hybbx_service_get_storage(service);
    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_storage_resolve_user(storage, username, out);
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

    snprintf(buf, sizeof(buf), "Activated '%s'.",
             hybbx_user_display_name(&target));
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
                 hybbx_user_display_name(&target),
                 hybbx_user_level_name(new_level));
        hybbx_session_write_line(session, buf);
        return HYBBX_OK;
    }

    target.level = new_level;
    rc = hybbx_storage_update_user(hybbx_service_get_storage(service), &target);
    if (rc != HYBBX_OK) {
        return rc;
    }

    snprintf(buf, sizeof(buf), "Promoted '%s' to %s.",
             hybbx_user_display_name(&target),
             hybbx_user_level_name(new_level));
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
             hybbx_user_display_name(&target),
             hybbx_user_level_name(old_level));
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

    snprintf(buf, sizeof(buf), "Deleted account '%s'.",
             hybbx_user_display_name(&target));
    hybbx_session_write_line(session, buf);
    return HYBBX_OK;
}

static hybbx_result_t cmd_deleteme(hybbx_service_t *service,
                                   hybbx_session_t *session,
                                   const hybbx_parsed_command_t *cmd)
{
    const hybbx_session_record_t *rec;
    hybbx_user_level_t level;
    const char *arg;
    hybbx_result_t rc;

    (void)service;

    level = hybbx_session_user_level(session);
    if (hybbx_user_level_is_sysop(level)) {
        hybbx_session_write_line(session,
            "The Sysop account is permanent and cannot be deleted.");
        return HYBBX_OK;
    }

    arg = cmd->argc > 0 ? cmd->argv[0] : NULL;
    if (arg != NULL && hybbx_bool_is_false(arg)) {
        hybbx_session_write_line(session, "Account deletion cancelled.");
        return HYBBX_OK;
    }

    if (!cmd_deleteme_confirmed(arg)) {
        hybbx_session_write_line(session,
            "Usage: /deleteme yes|no");
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

static hybbx_result_t cmd_register_user(hybbx_service_t *service,
                                        hybbx_session_t *session,
                                        const hybbx_parsed_command_t *cmd,
                                        int staff_created)
{
    hybbx_storage_t *storage;
    hybbx_user_registration_t reg;
    hybbx_user_record_t user;
    char buf[HYBBX_USER_FULL_NAME_MAX + 64];
    hybbx_result_t rc;

    if (staff_created) {
        if (cmd->argc < 5) {
            hybbx_session_write_line(session,
                "Usage: /usercreate <username> <full-name> <country> <location> <email>");
            return HYBBX_OK;
        }
    } else if (cmd->argc < 6) {
        hybbx_session_write_line(session,
            "Usage: /register <username> <full-name> <country> <location> <email> <password>");
        return HYBBX_OK;
    }

    rc = parse_registration(cmd, &reg, staff_created ? 0 : 1);
    if (rc != HYBBX_OK) {
        hybbx_session_write_line(session, "Registration fields too long.");
        return HYBBX_OK;
    }

    if (!staff_created && !hybbx_password_plain_valid(reg.password)) {
        hybbx_session_write_line(session,
            "Password must be 8-24 characters (- not allowed).");
        return HYBBX_OK;
    }

    storage = hybbx_service_get_storage(service);
    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_storage_register_user(storage, &reg, &user);
    if (rc == HYBBX_ERR_BUSY) {
        hybbx_session_write_line(session, "Username or nickname already taken.");
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

    if (staff_created) {
        snprintf(buf, sizeof(buf), "User '%s' created.",
                 hybbx_user_display_name(&user));
    } else {
        snprintf(buf, sizeof(buf), "Registration received for '%s'.",
                 hybbx_user_display_name(&user));
    }
    hybbx_session_write_line(session, buf);
    snprintf(buf, sizeof(buf), "Name: %s", user.full_name);
    hybbx_session_write_line(session, buf);
    if (staff_created) {
        hybbx_session_write_line(session,
            "Account is inactive. Use /activate before the user can log in.");
    } else {
        const hybbx_mail_config_t *mail_cfg = hybbx_service_get_mail(service);

        (void)hybbx_mail_notify_staff_registration(service, &reg, &user);
        hybbx_session_write_line(session,
            "A Sysop or Admin must activate your account before you can log in.");
        if (mail_cfg != NULL && mail_cfg->enabled) {
            hybbx_session_write_line(session,
                "Sysop and Admin were notified by mail.");
        }
    }
    return HYBBX_OK;
}

static hybbx_result_t cmd_register(hybbx_service_t *service,
                                   hybbx_session_t *session,
                                   const hybbx_parsed_command_t *cmd)
{
    if (!hybbx_auth_may_register(hybbx_session_user_level(session)) &&
        !(hybbx_session_login_prompt(session) &&
          !hybbx_session_logged_in(session))) {
        hybbx_session_write_line(session,
            "Only guests or login-prompt sessions may self-register with /register.");
        return HYBBX_ERR_DENIED;
    }

    return cmd_register_user(service, session, cmd, 0);
}

static hybbx_result_t cmd_createuser(hybbx_service_t *service,
                                     hybbx_session_t *session,
                                     const hybbx_parsed_command_t *cmd)
{
    return cmd_register_user(service, session, cmd, 1);
}

static hybbx_result_t cmd_changeme(hybbx_service_t *service,
                                   hybbx_session_t *session,
                                   const hybbx_parsed_command_t *cmd)
{
    hybbx_storage_t *storage;
    hybbx_user_registration_t reg;
    hybbx_user_record_t user;
    const char *old_password;
    const char *new_password;
    const char *username;
    hybbx_result_t rc;

    if (hybbx_session_is_guest(session)) {
        hybbx_session_write_line(session,
            "Only registered users may use /changeme.");
        return HYBBX_ERR_DENIED;
    }

    if (cmd->argc < 6) {
        hybbx_session_write_line(session,
            "Usage: /changeme <oldpass> <newpass> <full-name> <country> <location> <email>");
        return HYBBX_OK;
    }

    username = hybbx_session_username(session);
    if (username == NULL || username[0] == '\0') {
        return HYBBX_ERR_INVALID;
    }

    rc = parse_changeme(cmd, username, &reg, &old_password, &new_password);
    if (rc != HYBBX_OK) {
        hybbx_session_write_line(session, "Profile fields too long.");
        return HYBBX_OK;
    }

    if (!hybbx_password_plain_valid(new_password)) {
        hybbx_session_write_line(session,
            "New password must be 8-24 characters (- not allowed).");
        return HYBBX_OK;
    }

    storage = hybbx_service_get_storage(service);
    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_storage_find_user(storage, reg.username, &user);
    if (rc == HYBBX_ERR_NOT_FOUND) {
        hybbx_session_write_line(session, "Account not found.");
        return HYBBX_OK;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    {
        const hybbx_session_record_t *rec = hybbx_session_record(session);

        if (rec == NULL || user.id != rec->user_id) {
            hybbx_session_write_line(session,
                "You can only change your own account.");
            return HYBBX_OK;
        }

        if (!hybbx_user_profile_valid(&reg)) {
            hybbx_session_write_line(session,
                "Invalid profile (check name, country, location, email).");
            return HYBBX_OK;
        }
    }

    if (user.password[0] == '\0') {
        hybbx_session_write_line(session,
            "Account has no password. Ask Sysop or Admin to set one.");
        return HYBBX_OK;
    }

    if (!hybbx_password_match(user.password, old_password)) {
        hybbx_session_write_line(session, "Old password incorrect.");
        return HYBBX_OK;
    }

    hybbx_strlcpy(user.full_name, reg.full_name, sizeof(user.full_name));
    hybbx_strlcpy(user.country, reg.country, sizeof(user.country));
    hybbx_strlcpy(user.location, reg.location, sizeof(user.location));
    hybbx_strlcpy(user.email, reg.email, sizeof(user.email));

    rc = hybbx_password_hash(new_password, user.password, sizeof(user.password));
    if (rc != HYBBX_OK) {
        hybbx_session_write_line(session, "Could not store new password.");
        return rc;
    }

    rc = hybbx_storage_update_user(storage, &user);
    if (rc != HYBBX_OK) {
        return rc;
    }

    hybbx_session_write_line(session, "Account updated.");
    return HYBBX_OK;
}

static hybbx_result_t cmd_userchange(hybbx_service_t *service,
                                       hybbx_session_t *session,
                                       const hybbx_parsed_command_t *cmd)
{
    hybbx_storage_t *storage;
    hybbx_user_registration_t reg;
    hybbx_user_record_t user;
    const char *new_password;
    hybbx_user_level_t actor_level;
    hybbx_result_t rc;
    char buf[160];

    actor_level = hybbx_session_user_level(session);
    if (!hybbx_user_level_is_sysop_or_admin(actor_level)) {
        cmd_deny_privilege(session);
        return HYBBX_ERR_DENIED;
    }

    if (cmd->argc < 7) {
        hybbx_session_write_line(session,
            "Usage: /changeuser <user> <newpass> <full-name> <country> <location> <email>");
        return HYBBX_OK;
    }

    rc = parse_userchange(cmd, &reg, &new_password);
    if (rc != HYBBX_OK) {
        hybbx_session_write_line(session, "Profile fields too long.");
        return HYBBX_OK;
    }

    if (!hybbx_password_plain_valid(new_password)) {
        hybbx_session_write_line(session,
            "New password must be 8-24 characters (- not allowed).");
        return HYBBX_OK;
    }

    if (!hybbx_user_profile_valid(&reg)) {
        hybbx_session_write_line(session,
            "Invalid profile (check name, country, location, email).");
        return HYBBX_OK;
    }

    storage = hybbx_service_get_storage(service);
    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_storage_find_user(storage, reg.username, &user);
    if (rc == HYBBX_ERR_NOT_FOUND) {
        hybbx_session_write_line(session, "Unknown user.");
        return HYBBX_OK;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (str_ieq(user.username, hybbx_session_username(session))) {
        hybbx_session_write_line(session,
            "Use /changeme to update your own account.");
        return HYBBX_OK;
    }

    if (hybbx_user_level_is_sysop(user.level)) {
        hybbx_session_write_line(session,
            "The Sysop account cannot be changed with /changeuser.");
        return HYBBX_OK;
    }

    if (!hybbx_auth_may_userchange(actor_level, user.level)) {
        if (actor_level == HYBBX_LEVEL_ADMIN &&
            user.level == HYBBX_LEVEL_ADMIN) {
            hybbx_session_write_line(session,
                "Admins cannot change other Admins. Sysop only.");
        } else {
            cmd_deny_privilege(session);
        }
        return HYBBX_ERR_DENIED;
    }

    hybbx_strlcpy(user.full_name, reg.full_name, sizeof(user.full_name));
    hybbx_strlcpy(user.country, reg.country, sizeof(user.country));
    hybbx_strlcpy(user.location, reg.location, sizeof(user.location));
    hybbx_strlcpy(user.email, reg.email, sizeof(user.email));

    rc = hybbx_password_hash(new_password, user.password, sizeof(user.password));
    if (rc != HYBBX_OK) {
        hybbx_session_write_line(session, "Could not store new password.");
        return rc;
    }

    rc = hybbx_storage_update_user(storage, &user);
    if (rc != HYBBX_OK) {
        return rc;
    }

    snprintf(buf, sizeof(buf), "Updated account '%s'.",
             hybbx_user_display_name(&user));
    hybbx_session_write_line(session, buf);
    return HYBBX_OK;
}

static hybbx_result_t cmd_userdelete(hybbx_service_t *service,
                                       hybbx_session_t *session,
                                       const hybbx_parsed_command_t *cmd)
{
    hybbx_user_record_t target;
    hybbx_user_level_t actor_level;
    char buf[128];
    hybbx_result_t rc;

    actor_level = hybbx_session_user_level(session);
    if (!hybbx_user_level_is_sysop(actor_level)) {
        cmd_deny_privilege(session);
        return HYBBX_ERR_DENIED;
    }

    if (cmd->argc < 1 || cmd->argv[0] == NULL || cmd->argv[0][0] == '\0') {
        hybbx_session_write_line(session, "Usage: /deleteuser <username>");
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

    if (str_ieq(target.username, hybbx_session_username(session))) {
        hybbx_session_write_line(session, "You cannot delete your own account.");
        return HYBBX_OK;
    }

    if (!hybbx_auth_may_userdelete(actor_level, target.level)) {
        hybbx_session_write_line(session,
            "The Sysop account is permanent and cannot be deleted.");
        return HYBBX_OK;
    }

    rc = hybbx_storage_delete_user(hybbx_service_get_storage(service), target.id);
    if (rc != HYBBX_OK) {
        return rc;
    }

    snprintf(buf, sizeof(buf), "Deleted account '%s'.",
             hybbx_user_display_name(&target));
    hybbx_session_write_line(session, buf);
    return HYBBX_OK;
}

static hybbx_result_t cmd_login(hybbx_service_t *service,
                                hybbx_session_t *session,
                                const hybbx_parsed_command_t *cmd)
{
    hybbx_storage_t *storage;
    hybbx_user_record_t user;
    const hybbx_auth_config_t *auth;
    const char *guest_prefix;
    char username[HYBBX_USER_NAME_MAX];
    unsigned guest_slot;
    hybbx_result_t rc;

    if (cmd->argc < 1 || cmd->argv[0] == NULL || cmd->argv[0][0] == '\0') {
        hybbx_session_write_line(session,
            "Usage: /login <username> <password>");
        return HYBBX_OK;
    }

    auth = hybbx_service_get_auth(service);
    guest_prefix = auth != NULL ? auth->guest_prefix : HYBBX_AUTH_DEFAULT_GUEST_PREFIX;

    hybbx_strlcpy(username, cmd->argv[0], sizeof(username));
    hybbx_username_normalize(username);

    if (hybbx_guest_slot_from_username(guest_prefix, username, &guest_slot)) {
        if (auth != NULL && auth->auto_login) {
            hybbx_session_write_line(session,
                "Guest access is automatic on connect (auto_login). "
                "/login is for registered accounts only.");
        } else {
            hybbx_session_write_line(session,
                "/login is for registered accounts only.");
        }
        return HYBBX_OK;
    }

    if (cmd->argc < 2 || cmd->argv[1] == NULL || cmd->argv[1][0] == '\0') {
        hybbx_session_write_line(session,
            "Usage: /login <username> <password>");
        return HYBBX_OK;
    }

    storage = hybbx_service_get_storage(service);
    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_storage_find_user(storage, username, &user);

    if (rc == HYBBX_ERR_NOT_FOUND) {
        hybbx_session_write_line(session, "Unknown user.");
        return HYBBX_OK;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (hybbx_user_level_is_guest(user.level)) {
        hybbx_session_write_line(session,
            "Guests are ephemeral and use auto-login on connect.");
        return HYBBX_OK;
    }

    if (!user.active) {
        hybbx_session_write_line(session,
            "Account pending activation by Sysop or Admin.");
        return HYBBX_OK;
    }

    if (user.password[0] == '\0') {
        hybbx_session_write_line(session,
            "Account has no password. Contact Sysop or Admin.");
        return HYBBX_OK;
    }

    if (!hybbx_password_match(user.password, cmd->argv[1])) {
        const hybbx_session_record_t *rec = hybbx_session_record(session);

        hybbx_security_log_write("login_fail ip=%s user=%s transport=%s",
                                 rec != NULL && rec->remote[0] != '\0' ?
                                 rec->remote : "?",
                                 username,
                                 rec != NULL && rec->transport[0] != '\0' ?
                                 rec->transport : "?");
        hybbx_session_write_line(session, "Invalid password.");
        return HYBBX_OK;
    }

    if (hybbx_service_find_registered_session(service, user.id, session) !=
        NULL) {
        hybbx_session_write_line(session,
            "That account is already logged in elsewhere.");
        return HYBBX_OK;
    }

    rc = hybbx_session_switch_user(session, &user);
    if (rc != HYBBX_OK) {
        hybbx_session_write_line(session, "Login failed.");
        return rc;
    }

    {
        time_t since_login = user.last_login_at;
        const hybbx_texts_config_t *texts;

        texts = hybbx_service_get_texts(service);
        if (texts != NULL) {
            (void)hybbx_texts_send_motd(texts, session);
        }

        hybbx_mail_announce_since_last_login(service, session, since_login);

        user.last_login_at = time(NULL);
        (void)hybbx_storage_update_user(storage, &user);
    }
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

    if (str_ieq(cmd->argv[0], "show")) {
        if (cmd->argc != 1) {
            hybbx_session_write_line(session, "Usage: /chat show");
            return HYBBX_OK;
        }
        hybbx_chat_show_channel(service, session);
        return HYBBX_OK;
    }

    if (str_ieq(cmd->argv[0], "showall")) {
        if (cmd->argc != 1) {
            hybbx_session_write_line(session, "Usage: /chat showall");
            return HYBBX_OK;
        }
        hybbx_chat_show_all(service, session);
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

static hybbx_result_t cmd_conference(hybbx_service_t *service,
                                     hybbx_session_t *session,
                                     const hybbx_parsed_command_t *cmd)
{
    char topic[HYBBX_CONFERENCE_TOPIC_MAX];
    const char *user;
    size_t i;
    size_t pos = 0;

    if (hybbx_session_is_guest(session)) {
        hybbx_session_write_line(session, "Guests cannot use conference.");
        return HYBBX_ERR_DENIED;
    }

    if (cmd->argc < 2) {
        hybbx_session_write_line(session,
            "Usage: /conference <topic> <user>");
        return HYBBX_OK;
    }

    user = cmd->argv[cmd->argc - 1];
    topic[0] = '\0';

    for (i = 0; i + 1 < cmd->argc; i++) {
        size_t part_len = strlen(cmd->argv[i]);

        if (i > 0) {
            if (pos + 1 >= sizeof(topic)) {
                hybbx_session_write_line(session, "Topic too long.");
                return HYBBX_OK;
            }
            topic[pos++] = ' ';
            topic[pos] = '\0';
        }

        if (pos + part_len >= sizeof(topic)) {
            hybbx_session_write_line(session, "Topic too long.");
            return HYBBX_OK;
        }

        memcpy(topic + pos, cmd->argv[i], part_len);
        pos += part_len;
        topic[pos] = '\0';
    }

    return hybbx_conference_start(service, session, topic, user);
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

        rc = hybbx_mail_deliver(service, hybbx_session_display_name(session),
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

static hybbx_result_t cmd_broadcast(hybbx_service_t *service,
                                    hybbx_session_t *session,
                                    const hybbx_parsed_command_t *cmd)
{
    const hybbx_broadcast_config_t *cfg;
    char message[HYBBX_BROADCAST_AX25_MESSAGE_MAX + 1];
    double frequency_mhz = 0.0;
    int msg_start;
    int i;
    size_t off = 0;
    hybbx_result_t rc;

    cfg = hybbx_service_get_broadcast(service);
    if (cfg == NULL || !cfg->enabled) {
        hybbx_session_write_line(session, "Broadcast is disabled.");
        return HYBBX_OK;
    }

    if (cmd->argc < 2) {
        hybbx_session_write_line(session,
            "Usage: /broadcast ax25 list | ax25 <MHz|all> <text> | tcp <text>");
        return HYBBX_OK;
    }

    if (str_ieq(cmd->argv[1], "ax25")) {
        if (!cfg->ax25_enabled) {
            hybbx_session_write_line(session, "AX.25 broadcast is disabled.");
            return HYBBX_OK;
        }
        if (cmd->argc < 3) {
            hybbx_session_write_line(session,
                "Usage: /broadcast ax25 list | ax25 <MHz|all> <text>");
            return HYBBX_OK;
        }
        if (str_ieq(cmd->argv[2], "list")) {
            hybbx_broadcast_list_ax25_frequencies(session, cfg);
            return HYBBX_OK;
        }
        if (str_ieq(cmd->argv[2], "all")) {
            frequency_mhz = 0.0;
            msg_start = 3;
        } else {
            char *end = NULL;
            double parsed = strtod(cmd->argv[2], &end);

            if (end == cmd->argv[2] || parsed <= 0.0) {
                hybbx_session_write_line(session,
                    "Use frequency in MHz (e.g. 27.205) or all.");
                return HYBBX_ERR_INVALID;
            }
            frequency_mhz = parsed;
            msg_start = 3;
        }
        if ((size_t)cmd->argc <= (size_t)msg_start) {
            hybbx_session_write_line(session, "Missing broadcast message.");
            return HYBBX_ERR_INVALID;
        }
        message[0] = '\0';
        for (i = msg_start; i < (int)cmd->argc; i++) {
            size_t part_len = strlen(cmd->argv[i]);

            if (off > 0) {
                if (off >= sizeof(message) - 1) {
                    break;
                }
                message[off++] = ' ';
            }
            if (off + part_len >= sizeof(message)) {
                part_len = sizeof(message) - off - 1;
            }
            memcpy(message + off, cmd->argv[i], part_len);
            off += part_len;
        }
        message[off] = '\0';
        rc = hybbx_broadcast_ax25(service, frequency_mhz, message);
        if (rc == HYBBX_ERR_INVALID) {
            hybbx_session_write_line(session,
                "AX.25 message too long (max 48 characters for 1200 baud).");
        } else if (rc == HYBBX_ERR_BUSY) {
            hybbx_session_write_line(session,
                "AX.25 broadcast not sent (5+ min rate limit or no "
                "low+half-duplex link).");
        } else if (rc == HYBBX_ERR_DENIED) {
            hybbx_session_write_line(session,
                "AX.25 broadcast requires low-bandwidth half-duplex link (QoS).");
        } else if (rc == HYBBX_ERR_NOT_FOUND) {
            hybbx_session_write_line(session,
                "No secondary on that frequency (MHz).");
        } else if (rc != HYBBX_OK) {
            hybbx_session_write_line(session, "AX.25 broadcast failed.");
        } else {
            hybbx_session_write_line(session, "AX.25 broadcast sent.");
        }
        return rc == HYBBX_OK ? HYBBX_OK : HYBBX_ERR_IO;
    }

    if (str_ieq(cmd->argv[1], "tcp")) {
        if (!cfg->tcp_enabled) {
            hybbx_session_write_line(session, "TCP broadcast is disabled.");
            return HYBBX_OK;
        }
        if (cmd->argc < 3) {
            hybbx_session_write_line(session, "Usage: /broadcast tcp <text>");
            return HYBBX_OK;
        }
        message[0] = '\0';
        off = 0;
        for (i = 2; i < (int)cmd->argc; i++) {
            size_t part_len = strlen(cmd->argv[i]);

            if (off > 0 && off < sizeof(message) - 1) {
                message[off++] = ' ';
            }
            if (off + part_len >= sizeof(message)) {
                part_len = sizeof(message) - off - 1;
            }
            memcpy(message + off, cmd->argv[i], part_len);
            off += part_len;
        }
        message[off] = '\0';
        rc = hybbx_broadcast_tcp_stub(service, message);
        if (rc != HYBBX_OK) {
            hybbx_session_write_line(session, "TCP broadcast failed.");
            return HYBBX_ERR_IO;
        }
        hybbx_session_write_line(session, "TCP broadcast logged (stub).");
        return HYBBX_OK;
    }

    hybbx_session_write_line(session,
        "Usage: /broadcast ax25 list | ax25 <MHz|all> <text> | tcp <text>");
    return HYBBX_ERR_INVALID;
}

static hybbx_result_t cmd_shutdown(hybbx_service_t *service,
                                   hybbx_session_t *session)
{
    if (!hybbx_user_level_is_sysop(hybbx_session_user_level(session))) {
        cmd_deny_privilege(session);
        return HYBBX_ERR_DENIED;
    }

    hybbx_security_log_write("shutdown ip=%s user=%s transport=%s",
                             hybbx_session_record(session) != NULL &&
                             hybbx_session_record(session)->remote[0] != '\0' ?
                             hybbx_session_record(session)->remote : "?",
                             hybbx_session_display_name(session),
                             hybbx_session_record(session) != NULL ?
                             hybbx_session_record(session)->transport : "?");
    hybbx_session_write_line(session, "Shutting down HyBBX.");
    hybbx_service_request_shutdown(service, 0);
    return HYBBX_OK;
}

static hybbx_result_t cmd_restart(hybbx_service_t *service,
                                  hybbx_session_t *session)
{
    if (!hybbx_user_level_is_sysop(hybbx_session_user_level(session))) {
        cmd_deny_privilege(session);
        return HYBBX_ERR_DENIED;
    }

    hybbx_security_log_write("restart ip=%s user=%s transport=%s",
                             hybbx_session_record(session) != NULL &&
                             hybbx_session_record(session)->remote[0] != '\0' ?
                             hybbx_session_record(session)->remote : "?",
                             hybbx_session_display_name(session),
                             hybbx_session_record(session) != NULL ?
                             hybbx_session_record(session)->transport : "?");
    hybbx_session_write_line(session, "Restarting HyBBX.");
    hybbx_service_request_shutdown(service, 1);
    return HYBBX_OK;
}

static hybbx_result_t cmd_version(hybbx_session_t *session)
{
    char buf[128];
    char os_name[64];

    if (hybbx_platform_os_name(os_name, sizeof(os_name)) != HYBBX_OK) {
        hybbx_strlcpy(os_name, "unknown", sizeof(os_name));
    }

    snprintf(buf, sizeof(buf), "HyBBX %s", HYBBX_VERSION_STRING);
    hybbx_session_write_line(session, buf);
    snprintf(buf, sizeof(buf), "Operating system: %s", os_name);
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

    if (str_ieq(cmd->verb, "rules") || str_ieq(cmd->verb, "legal")) {
        return cmd_rules(service, session);
    }

    if (str_ieq(cmd->verb, "who") || str_ieq(cmd->verb, "online")) {
        return cmd_who(service, session);
    }

    if (str_ieq(cmd->verb, "users")) {
        return cmd_users(service, session);
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

    if (str_ieq(cmd->verb, "conference") || str_ieq(cmd->verb, "meeting")) {
        return cmd_conference(service, session, cmd);
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

    if (str_ieq(cmd->verb, "changeme")) {
        return cmd_changeme(service, session, cmd);
    }

    if (str_ieq(cmd->verb, "changeuser") || str_ieq(cmd->verb, "userchange")) {
        return cmd_userchange(service, session, cmd);
    }

    if (str_ieq(cmd->verb, "deleteuser") || str_ieq(cmd->verb, "userdelete")) {
        return cmd_userdelete(service, session, cmd);
    }

    if (str_ieq(cmd->verb, "usercreate") || str_ieq(cmd->verb, "createuser")) {
        return cmd_createuser(service, session, cmd);
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

    if (str_ieq(cmd->verb, "broadcast")) {
        return cmd_broadcast(service, session, cmd);
    }

    if (str_ieq(cmd->verb, "shutdown")) {
        return cmd_shutdown(service, session);
    }

    if (str_ieq(cmd->verb, "restart")) {
        return cmd_restart(service, session);
    }

    if (str_ieq(cmd->verb, "exit") || str_ieq(cmd->verb, "logout") ||
        str_ieq(cmd->verb, "bye") || str_ieq(cmd->verb, "quit")) {
        return cmd_exit(session);
    }

    hybbx_session_write_line(session, cmd_help_unknown(session));
    return HYBBX_ERR_NOT_FOUND;
}
