#include "hybbx/auth.h"
#include "hybbx/storage.h"
#include "hybbx/util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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

void hybbx_auth_config_defaults(hybbx_auth_config_t *auth)
{
    if (auth == NULL) {
        return;
    }

    auth->auto_login = 1;
    hybbx_strlcpy(auth->guest_prefix, HYBBX_AUTH_DEFAULT_GUEST_PREFIX,
                  sizeof(auth->guest_prefix));
}

const char *hybbx_user_level_name(hybbx_user_level_t level)
{
    switch (level) {
    case HYBBX_LEVEL_SYSOP:
        return "sysop";
    case HYBBX_LEVEL_ADMIN:
        return "admin";
    case HYBBX_LEVEL_MOD:
        return "mod";
    case HYBBX_LEVEL_USER:
        return "user";
    case HYBBX_LEVEL_GUEST:
        return "guest";
    default:
        return "user";
    }
}

hybbx_user_level_t hybbx_user_level_parse(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return HYBBX_LEVEL_USER;
    }

    if (str_ieq(name, "sysop")) {
        return HYBBX_LEVEL_SYSOP;
    }
    if (str_ieq(name, "admin")) {
        return HYBBX_LEVEL_ADMIN;
    }
    if (str_ieq(name, "mod") || str_ieq(name, "moderator")) {
        return HYBBX_LEVEL_MOD;
    }
    if (str_ieq(name, "guest")) {
        return HYBBX_LEVEL_GUEST;
    }

    return HYBBX_LEVEL_USER;
}

int hybbx_user_level_is_guest(hybbx_user_level_t level)
{
    return level == HYBBX_LEVEL_GUEST;
}

int hybbx_user_level_is_sysop_or_admin(hybbx_user_level_t level)
{
    return level == HYBBX_LEVEL_SYSOP || level == HYBBX_LEVEL_ADMIN;
}

int hybbx_user_level_is_sysop(hybbx_user_level_t level)
{
    return level == HYBBX_LEVEL_SYSOP;
}

static int name_has_prefix(const char *username, const char *prefix)
{
    size_t plen;
    size_t i;

    if (username == NULL || prefix == NULL) {
        return 0;
    }

    plen = strlen(prefix);
    if (plen == 0) {
        return 0;
    }

    for (i = 0; i < plen; i++) {
        char cu = username[i];
        char cp = prefix[i];

        if (cu == '\0') {
            return 0;
        }

        if (cu >= 'A' && cu <= 'Z') {
            cu = (char)(cu + 32);
        }
        if (cp >= 'A' && cp <= 'Z') {
            cp = (char)(cp + 32);
        }

        if (cu != cp) {
            return 0;
        }
    }

    return 1;
}

static int username_char_ok(char ch)
{
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') ||
           ch == '_' || ch == '-';
}

void hybbx_username_normalize(char *username)
{
    size_t i;

    if (username == NULL) {
        return;
    }

    for (i = 0; username[i] != '\0'; i++) {
        if (username[i] >= 'A' && username[i] <= 'Z') {
            username[i] = (char)(username[i] + 32);
        }
    }
}

const char *hybbx_username_display(const char *username,
                                   hybbx_user_level_t level)
{
    hybbx_user_record_t user;

    if (username == NULL || username[0] == '\0') {
        return "";
    }

    memset(&user, 0, sizeof(user));
    hybbx_strlcpy(user.username, username, sizeof(user.username));
    hybbx_username_normalize(user.username);
    user.level = level;
    hybbx_nickname_infer(username, user.nickname, sizeof(user.nickname));

    return hybbx_user_display_name(&user);
}

void hybbx_nickname_infer(const char *stored_username,
                          char *nickname,
                          size_t nickname_len)
{
    if (stored_username == NULL || nickname == NULL || nickname_len == 0) {
        return;
    }

    if (str_ieq(stored_username, HYBBX_DEFAULT_SYSOP_USERNAME) ||
        str_ieq(stored_username, "sysop")) {
        hybbx_strlcpy(nickname, HYBBX_DEFAULT_SYSOP_USERNAME, nickname_len);
        return;
    }

    if (stored_username[0] >= 'a' && stored_username[0] <= 'z') {
        nickname[0] = (char)(stored_username[0] - 32);
        hybbx_strlcpy(nickname + 1, stored_username + 1, nickname_len - 1);
        return;
    }

    hybbx_strlcpy(nickname, stored_username, nickname_len);
}

const char *hybbx_user_display_name(const hybbx_user_record_t *user)
{
    if (user == NULL) {
        return "";
    }

    if (user->nickname[0] != '\0') {
        return user->nickname;
    }

    if (user->username[0] == '\0') {
        return "";
    }

    return user->username;
}

int hybbx_guest_slot_from_username(const char *guest_prefix,
                                   const char *username,
                                   unsigned *slot_out)
{
    const char *prefix;
    size_t plen;
    size_t i;
    unsigned long slot;
    char *end;

    if (username == NULL || slot_out == NULL) {
        return 0;
    }

    prefix = guest_prefix != NULL && guest_prefix[0] != '\0' ?
             guest_prefix : HYBBX_AUTH_DEFAULT_GUEST_PREFIX;
    plen = strlen(prefix);
    if (plen == 0 || strlen(username) <= plen) {
        return 0;
    }

    for (i = 0; i < plen; i++) {
        char a = (char)(prefix[i] >= 'A' && prefix[i] <= 'Z' ?
                        prefix[i] + 32 : prefix[i]);
        char b = (char)(username[i] >= 'A' && username[i] <= 'Z' ?
                        username[i] + 32 : username[i]);

        if (a != b) {
            return 0;
        }
    }

    slot = strtoul(username + plen, &end, 10);
    if (end == username + plen || *end != '\0' || slot < 1 ||
        slot > HYBBX_GUEST_NUMBER_MAX) {
        return 0;
    }

    *slot_out = (unsigned)slot;
    return 1;
}

void hybbx_guest_fill_record(const char *guest_prefix,
                             unsigned slot,
                             hybbx_user_record_t *out)
{
    const char *prefix;
    time_t now;

    if (out == NULL || slot < 1 || slot > HYBBX_GUEST_NUMBER_MAX) {
        return;
    }

    prefix = guest_prefix != NULL && guest_prefix[0] != '\0' ?
             guest_prefix : HYBBX_AUTH_DEFAULT_GUEST_PREFIX;
    now = time(NULL);

    memset(out, 0, sizeof(*out));
    out->id = HYBBX_GUEST_USER_ID(slot);
    snprintf(out->username, sizeof(out->username), "%s%u", prefix, slot);
    hybbx_strlcpy(out->nickname, out->username, sizeof(out->nickname));
    out->level = HYBBX_LEVEL_GUEST;
    out->active = 1;
    out->created_at = now;
}

int hybbx_username_valid(const char *username, const char *guest_prefix)
{
    size_t len;
    size_t i;
    size_t digit_count = 0;
    size_t underscore_count = 0;
    size_t hyphen_count = 0;
    const char *prefix;

    if (username == NULL) {
        return 0;
    }

    len = strlen(username);
    if (len < HYBBX_USERNAME_MIN_LEN || len > HYBBX_USERNAME_MAX_LEN) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        if (!username_char_ok(username[i])) {
            return 0;
        }

        if (username[i] >= '0' && username[i] <= '9') {
            digit_count++;
        } else if (username[i] == '_') {
            underscore_count++;
        } else if (username[i] == '-') {
            hyphen_count++;
        }
    }

    if (digit_count > HYBBX_USERNAME_MAX_DIGITS) {
        return 0;
    }

    if (underscore_count > 1 || hyphen_count > 1 ||
        (underscore_count > 0 && hyphen_count > 0)) {
        return 0;
    }

    prefix = guest_prefix != NULL && guest_prefix[0] != '\0' ?
             guest_prefix : HYBBX_AUTH_DEFAULT_GUEST_PREFIX;

    if (name_has_prefix(username, prefix)) {
        return 0;
    }

    if (str_ieq(username, "sysop") || str_ieq(username, "admin") ||
        str_ieq(username, "mod") || str_ieq(username, "guest")) {
        return 0;
    }

    return 1;
}

int hybbx_password_plain_valid(const char *password)
{
    size_t len;

    if (password == NULL || password[0] == '\0') {
        return 0;
    }

    if (str_ieq(password, "-")) {
        return 0;
    }

    len = strlen(password);
    if (len < HYBBX_PASSWORD_MIN_LEN || len > HYBBX_PASSWORD_MAX_LEN) {
        return 0;
    }

    return 1;
}

static int profile_text_char_ok(char ch)
{
    if (ch == '|') {
        return 0;
    }

    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') ||
           ch == ' ' || ch == '-' || ch == '\'' || ch == '.' || ch == ',';
}

static int profile_text_valid(const char *text, size_t min_len, size_t max_len)
{
    size_t len;
    size_t i;

    if (text == NULL) {
        return 0;
    }

    len = strlen(text);
    if (len < min_len || len >= max_len) {
        return 0;
    }

    for (i = 0; i < len; i++) {
        if (!profile_text_char_ok(text[i])) {
            return 0;
        }
    }

    return 1;
}

static int email_valid(const char *email)
{
    const char *at;
    const char *dot;
    size_t len;
    size_t i;

    if (email == NULL) {
        return 0;
    }

    len = strlen(email);
    if (len < 5 || len >= HYBBX_USER_EMAIL_MAX) {
        return 0;
    }

    at = strchr(email, '@');
    if (at == NULL || at == email || at[1] == '\0') {
        return 0;
    }

    dot = strchr(at + 1, '.');
    if (dot == NULL || dot[1] == '\0') {
        return 0;
    }

    for (i = 0; i < len; i++) {
        char ch = email[i];

        if (ch == '|' || ch == ' ') {
            return 0;
        }

        if (!((ch >= 'a' && ch <= 'z') ||
              (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') ||
              ch == '@' || ch == '.' || ch == '-' || ch == '_' || ch == '+')) {
            return 0;
        }
    }

    return 1;
}

int hybbx_user_profile_valid(const hybbx_user_registration_t *reg)
{
    if (reg == NULL) {
        return 0;
    }

    if (!profile_text_valid(reg->full_name, 2, HYBBX_USER_FULL_NAME_MAX)) {
        return 0;
    }

    if (!profile_text_valid(reg->country, 2, HYBBX_USER_COUNTRY_MAX)) {
        return 0;
    }

    if (!profile_text_valid(reg->location, 2, HYBBX_USER_LOCATION_MAX)) {
        return 0;
    }

    if (!email_valid(reg->email)) {
        return 0;
    }

    return 1;
}

int hybbx_registration_valid(const hybbx_user_registration_t *reg,
                             const char *guest_prefix)
{
    if (reg == NULL) {
        return 0;
    }

    if (!hybbx_username_valid(reg->username, guest_prefix)) {
        return 0;
    }

    if (!hybbx_username_valid(reg->nickname, guest_prefix)) {
        return 0;
    }

    return hybbx_user_profile_valid(reg);
}
