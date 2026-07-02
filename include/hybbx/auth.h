#ifndef HYBBX_AUTH_H
#define HYBBX_AUTH_H

#include "hybbx/password.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_AUTH_GUEST_PREFIX_MAX 32

/** Default guest name prefix: Guest1, Guest2, … */
#define HYBBX_AUTH_DEFAULT_GUEST_PREFIX "Guest"

/** Highest guest number issued (Guest1 … Guest25); max simultaneous guests. */
#define HYBBX_GUEST_NUMBER_MAX 25u

/**
 * Synthetic user IDs for ephemeral guest sessions (not stored in user files).
 * Slot @p n is 1 … @ref HYBBX_GUEST_NUMBER_MAX.
 */
#define HYBBX_GUEST_USER_ID(n) (0xffffffffffffff00ULL + (uint64_t)(n))

#define HYBBX_USERNAME_MIN_LEN 4u
#define HYBBX_USERNAME_MAX_LEN 12u
#define HYBBX_USERNAME_MAX_DIGITS 4u

/** Default Sysop account (auto-created when no Sysop exists in the user database). */
#define HYBBX_DEFAULT_SYSOP_USERNAME "Sysop"
#define HYBBX_DEFAULT_SYSOP_PASSWORD "SysopPassword"

/** Plain-text password length policy for new passwords (/changeme, future set-password). */
#define HYBBX_PASSWORD_MIN_LEN 8u
#define HYBBX_PASSWORD_MAX_LEN 24u

/**
 * User privilege levels (highest to lowest).
 * Sysop: exactly one account on the system.
 */
typedef enum hybbx_user_level {
    HYBBX_LEVEL_SYSOP = 1,
    HYBBX_LEVEL_ADMIN = 2,
    HYBBX_LEVEL_MOD = 3,
    HYBBX_LEVEL_USER = 4,
    HYBBX_LEVEL_GUEST = 5
} hybbx_user_level_t;

typedef struct hybbx_auth_config {
    int auto_login;
    char guest_prefix[HYBBX_AUTH_GUEST_PREFIX_MAX];
} hybbx_auth_config_t;

void hybbx_auth_config_defaults(hybbx_auth_config_t *auth);

const char *hybbx_user_level_name(hybbx_user_level_t level);
hybbx_user_level_t hybbx_user_level_parse(const char *name);
int hybbx_user_level_is_guest(hybbx_user_level_t level);

/** Non-zero when @p level is Sysop or Admin. */
int hybbx_user_level_is_sysop_or_admin(hybbx_user_level_t level);

/** Non-zero when @p actor may activate pending registered accounts. */
int hybbx_auth_may_activate(hybbx_user_level_t actor);

/** Non-zero when @p actor may self-register via `/register` (guests only). */
int hybbx_auth_may_register(hybbx_user_level_t actor);

/** Non-zero when @p actor may create user accounts via `/createuser` (Sysop, Admin). */
int hybbx_auth_may_create_user(hybbx_user_level_t actor);

/** Non-zero when @p actor may update own profile via `/changeme` (not guests). */
int hybbx_auth_may_changeme(hybbx_user_level_t actor);

/**
 * Non-zero when @p actor may overwrite another account via `/userchange`.
 * Sysop: Admin, Mod, User. Admin: Mod, User only. Not Sysop or guests.
 */
int hybbx_auth_may_userchange(hybbx_user_level_t actor,
                              hybbx_user_level_t target_level);

/**
 * Non-zero when @p actor may delete another account via `/userdelete` (Sysop only;
 * any non-Sysop target).
 */
int hybbx_auth_may_userdelete(hybbx_user_level_t actor,
                              hybbx_user_level_t target_level);

/**
 * Non-zero when @p actor may promote a user at @p target_level to @p new_level.
 * Target must be active and not a guest. Sysop: admin; Sysop or Admin: mod.
 */
int hybbx_auth_may_promote(hybbx_user_level_t actor,
                           hybbx_user_level_t target_level,
                           int target_active,
                           hybbx_user_level_t new_level);

/** Non-zero when @p actor may demote a user at @p target_level to user. */
int hybbx_auth_may_demote(hybbx_user_level_t actor,
                          hybbx_user_level_t target_level);

/**
 * Non-zero when @p actor may delete an account at @p target_level.
 * Sysop account is never deletable. Admins cannot delete Admins.
 */
int hybbx_auth_may_delete(hybbx_user_level_t actor,
                          hybbx_user_level_t target_level);

/** Non-zero when @p level is the protected Sysop account tier. */
int hybbx_user_level_is_sysop(hybbx_user_level_t level);

/** Non-zero when @p password meets the plain-text password policy (8–24 characters). */
int hybbx_password_plain_valid(const char *password);

/**
 * Return non-zero when @p username is valid for registration:
 * 4–12 characters, letters (a–z), digits (max 4), and at most one `_` or one `-`
 * (not both); case-insensitive.
 */
int hybbx_username_valid(const char *username, const char *guest_prefix);

/** Fold @p username to lowercase in place (for case-insensitive storage). */
void hybbx_username_normalize(char *username);

/**
 * Parse @p username as @c <guest_prefix><1-25> (case-insensitive prefix).
 * Writes slot to @p slot_out. Returns 1 on match, else 0.
 */
int hybbx_guest_slot_from_username(const char *guest_prefix,
                                   const char *username,
                                   unsigned *slot_out);

struct hybbx_user_record;

/** Fill @p out with an ephemeral guest record for @p slot (1 … 25). */
void hybbx_guest_fill_record(const char *guest_prefix,
                             unsigned slot,
                             struct hybbx_user_record *out);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_AUTH_H */
