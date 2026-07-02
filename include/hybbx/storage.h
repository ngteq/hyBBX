#ifndef HYBBX_STORAGE_H
#define HYBBX_STORAGE_H

#include "hybbx/auth.h"
#include "hybbx/types.h"

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum hybbx_storage_backend_kind {
    HYBBX_STORAGE_FLATFILE = 1,
    HYBBX_STORAGE_SQLITE = 2,
    HYBBX_STORAGE_MYSQL = 3,
    HYBBX_STORAGE_MARIADB = 4
} hybbx_storage_backend_kind_t;

#define HYBBX_USER_NAME_MAX 64
#define HYBBX_USER_FULL_NAME_MAX 128
#define HYBBX_USER_COUNTRY_MAX 64
#define HYBBX_USER_LOCATION_MAX 128
#define HYBBX_USER_EMAIL_MAX 128
#define HYBBX_USER_PASSWORD_MAX HYBBX_PASSWORD_STORED_MAX
#define HYBBX_TRANSPORT_NAME_MAX 32

/** Maximum registered user records per INI user-file shard. */
#define HYBBX_USERS_PER_FILE 50u

/** INI section prefix for one user: [user.<id>]. */
#define HYBBX_USER_INI_SECTION_PREFIX "user."

typedef struct hybbx_user_registration {
    char username[HYBBX_USER_NAME_MAX];
    char full_name[HYBBX_USER_FULL_NAME_MAX];
    char country[HYBBX_USER_COUNTRY_MAX];
    char location[HYBBX_USER_LOCATION_MAX];
    char email[HYBBX_USER_EMAIL_MAX];
} hybbx_user_registration_t;

/** Validate profile fields only (full name, country, location, email). */
int hybbx_user_profile_valid(const hybbx_user_registration_t *reg);

/** Validate registration profile fields (username + full name, country, location, email). */
int hybbx_registration_valid(const hybbx_user_registration_t *reg,
                             const char *guest_prefix);

typedef struct hybbx_user_record {
    uint64_t id;
    char username[HYBBX_USER_NAME_MAX];
    hybbx_user_level_t level;
    int active;
    time_t created_at;
    char full_name[HYBBX_USER_FULL_NAME_MAX];
    char country[HYBBX_USER_COUNTRY_MAX];
    char location[HYBBX_USER_LOCATION_MAX];
    char email[HYBBX_USER_EMAIL_MAX];
    char password[HYBBX_USER_PASSWORD_MAX];
} hybbx_user_record_t;

typedef struct hybbx_session_record {
    uint64_t session_id;
    uint64_t user_id;
    char username[HYBBX_USER_NAME_MAX];
    char transport[HYBBX_TRANSPORT_NAME_MAX];
    time_t connected_at;
    time_t disconnected_at;
    int active;
} hybbx_session_record_t;

typedef struct hybbx_storage hybbx_storage_t;

typedef struct hybbx_storage_options {
    hybbx_storage_backend_kind_t backend;
    const char *path;
    const char *guest_prefix;
} hybbx_storage_options_t;

hybbx_storage_t *hybbx_storage_open(const hybbx_storage_options_t *options);
void hybbx_storage_close(hybbx_storage_t *storage);

hybbx_storage_backend_kind_t hybbx_storage_backend(const hybbx_storage_t *storage);

/**
 * Deprecated: guests are ephemeral (@ref hybbx_service_guest_assign).
 * Always returns @ref HYBBX_ERR_INVALID.
 */
hybbx_result_t hybbx_storage_create_guest(hybbx_storage_t *storage,
                                          hybbx_user_record_t *out);

/**
 * Register a new user account (level user, inactive until Sysop/Admin activates).
 */
hybbx_result_t hybbx_storage_register_user(hybbx_storage_t *storage,
                                           const hybbx_user_registration_t *reg,
                                           hybbx_user_record_t *out);

/** Look up a user by name (case-insensitive). */
hybbx_result_t hybbx_storage_find_user(hybbx_storage_t *storage,
                                       const char *username,
                                       hybbx_user_record_t *out);

/** Count existing accounts at a given level (e.g. enforce one Sysop). */
hybbx_result_t hybbx_storage_count_level(hybbx_storage_t *storage,
                                         hybbx_user_level_t level,
                                         size_t *count);

/**
 * Iterate all user records. @p fn returns HYBBX_OK to continue; any other
 * result stops iteration and is returned from this function.
 */
typedef hybbx_result_t (*hybbx_storage_user_fn)(const hybbx_user_record_t *user,
                                                void *ctx);

hybbx_result_t hybbx_storage_foreach_user(hybbx_storage_t *storage,
                                          hybbx_storage_user_fn fn,
                                          void *ctx);

hybbx_result_t hybbx_storage_session_begin(hybbx_storage_t *storage,
                                           const hybbx_user_record_t *user,
                                           const char *transport,
                                           hybbx_session_record_t *out);

hybbx_result_t hybbx_storage_session_end(hybbx_storage_t *storage,
                                         uint64_t session_id);

/**
 * Replace an existing user record (matched by @p user->id).
 */
hybbx_result_t hybbx_storage_update_user(hybbx_storage_t *storage,
                                         const hybbx_user_record_t *user);

/** Remove a user record from storage (matched by @p user_id). */
hybbx_result_t hybbx_storage_delete_user(hybbx_storage_t *storage,
                                         uint64_t user_id);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_STORAGE_H */
