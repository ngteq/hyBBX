#include "hybbx/storage.h"
#include "hybbx/auth.h"
#include "hybbx/password.h"
#include "hybbx/config.h"
#include "hybbx/util.h"
#include "hybbx/limits.h"
#include "storage_private.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

struct flatfile_state {
    char users_dir[HYBBX_PATH_MAX];
    char legacy_users_path[HYBBX_PATH_MAX];
    char sessions_path[HYBBX_PATH_MAX];
    char guest_next_path[HYBBX_PATH_MAX];
    char session_next_path[HYBBX_PATH_MAX];
    char user_next_path[HYBBX_PATH_MAX];
};

typedef struct user_shard {
    hybbx_user_record_t users[HYBBX_USERS_PER_FILE];
    size_t count;
} user_shard_t;

static int mkdir_p(const char *path)
{
    char buf[HYBBX_PATH_MAX];
    size_t len;
    size_t i;

    if (path == NULL || path[0] == '\0') {
        return -1;
    }

    len = strlen(path);
    if (len >= sizeof(buf)) {
        return -1;
    }

    memcpy(buf, path, len + 1);

    for (i = 1; i < len; i++) {
        if (buf[i] == '/') {
            buf[i] = '\0';
            if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
                return -1;
            }
            buf[i] = '/';
        }
    }

    if (mkdir(buf, 0755) != 0 && errno != EEXIST) {
        return -1;
    }

    return 0;
}

static hybbx_result_t read_counter(const char *path, uint64_t *value)
{
    FILE *fp;
    unsigned long long n = 0;

    fp = fopen(path, "r");
    if (fp == NULL) {
        *value = 0;
        return HYBBX_OK;
    }

    if (fscanf(fp, "%llu", &n) != 1) {
        fclose(fp);
        return HYBBX_ERR_IO;
    }

    fclose(fp);
    *value = (uint64_t)n;
    return HYBBX_OK;
}

static hybbx_result_t write_counter(const char *path, uint64_t value)
{
    FILE *fp;

    fp = fopen(path, "w");
    if (fp == NULL) {
        return HYBBX_ERR_IO;
    }

    fprintf(fp, "%llu\n", (unsigned long long)value);
    fclose(fp);
    return HYBBX_OK;
}

static hybbx_result_t bump_counter(const char *path, uint64_t *value)
{
    hybbx_result_t rc;

    rc = read_counter(path, value);
    if (rc != HYBBX_OK) {
        return rc;
    }

    (*value)++;
    return write_counter(path, *value);
}

static hybbx_result_t append_line(const char *path, const char *line)
{
    FILE *fp;

    fp = fopen(path, "a");
    if (fp == NULL) {
        return HYBBX_ERR_IO;
    }

    fprintf(fp, "%s\n", line);
    fclose(fp);
    return HYBBX_OK;
}

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

static int file_exists(const char *path)
{
    struct stat st;

    return path != NULL && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static hybbx_result_t user_shard_path(const struct flatfile_state *state,
                                      unsigned shard_index,
                                      char *out, size_t out_len)
{
    char name[32];
    int n;

    if (state == NULL || out == NULL || out_len == 0 || shard_index == 0) {
        return HYBBX_ERR_INVALID;
    }

    if (shard_index == 1) {
        n = snprintf(name, sizeof(name), "users.ini");
    } else {
        n = snprintf(name, sizeof(name), "users%u.ini", shard_index);
    }

    if (n < 0 || (size_t)n >= sizeof(name)) {
        return HYBBX_ERR_INVALID;
    }

    return hybbx_path_join(out, out_len, state->users_dir, name);
}

static unsigned last_user_shard_index(const struct flatfile_state *state)
{
    char path[HYBBX_PATH_MAX];
    unsigned index;

    for (index = 1; index < 10000u; index++) {
        if (user_shard_path(state, index, path, sizeof(path)) != HYBBX_OK) {
            break;
        }
        if (!file_exists(path)) {
            break;
        }
    }

    if (index == 1) {
        return 0;
    }

    return index - 1;
}

static int parse_legacy_user_line(const char *line, hybbx_user_record_t *out)
{
    unsigned long long id = 0;
    unsigned long long active = 0;
    long long created = 0;
    char username[HYBBX_USER_NAME_MAX];
    char level_name[32];
    char full_name[HYBBX_USER_FULL_NAME_MAX];
    char country[HYBBX_USER_COUNTRY_MAX];
    char location[HYBBX_USER_LOCATION_MAX];
    char email[HYBBX_USER_EMAIL_MAX];
    char password[HYBBX_USER_PASSWORD_MAX];
    int fields;

    if (line == NULL || out == NULL) {
        return 0;
    }

    full_name[0] = '\0';
    country[0] = '\0';
    location[0] = '\0';
    email[0] = '\0';
    password[0] = '\0';

    fields = sscanf(line,
                    "%llu|%63[^|]|%31[^|]|%llu|%lld|"
                    "%127[^|]|%63[^|]|%127[^|]|%127[^|]|%95[^|]",
                    &id, username, level_name, &active, &created,
                    full_name, country, location, email, password);
    if (fields < 5) {
        return 0;
    }

    memset(out, 0, sizeof(*out));
    out->id = (uint64_t)id;
    hybbx_strlcpy(out->username, username, sizeof(out->username));
    out->level = hybbx_user_level_parse(level_name);
    out->active = (int)active;
    out->created_at = (time_t)created;

    if (fields >= 9) {
        hybbx_strlcpy(out->full_name, full_name, sizeof(out->full_name));
        hybbx_strlcpy(out->country, country, sizeof(out->country));
        hybbx_strlcpy(out->location, location, sizeof(out->location));
        hybbx_strlcpy(out->email, email, sizeof(out->email));
    }

    if (fields >= 10) {
        hybbx_strlcpy(out->password, password, sizeof(out->password));
    }

    return 1;
}

static int parse_user_section(const hybbx_config_t *cfg, const char *section,
                              hybbx_user_record_t *out)
{
    const char *value;
    unsigned long long n;

    if (cfg == NULL || section == NULL || out == NULL) {
        return 0;
    }

    if (strncmp(section, HYBBX_USER_INI_SECTION_PREFIX,
                strlen(HYBBX_USER_INI_SECTION_PREFIX)) != 0) {
        return 0;
    }

    memset(out, 0, sizeof(*out));

    value = hybbx_config_get(cfg, section, "id", NULL);
    if (value == NULL || value[0] == '\0') {
        return 0;
    }
    n = strtoull(value, NULL, 10);
    if (n == 0) {
        return 0;
    }
    out->id = (uint64_t)n;

    value = hybbx_config_get(cfg, section, "username", NULL);
    if (value == NULL || value[0] == '\0') {
        return 0;
    }
    hybbx_strlcpy(out->username, value, sizeof(out->username));

    value = hybbx_config_get(cfg, section, "level", "user");
    out->level = hybbx_user_level_parse(value);
    out->active = hybbx_config_get_bool(cfg, section, "active", 0);

    value = hybbx_config_get(cfg, section, "created", NULL);
    if (value != NULL && value[0] != '\0') {
        out->created_at = (time_t)strtoll(value, NULL, 10);
    }

    value = hybbx_config_get(cfg, section, "fullname", NULL);
    if (value != NULL) {
        hybbx_strlcpy(out->full_name, value, sizeof(out->full_name));
    }

    value = hybbx_config_get(cfg, section, "country", NULL);
    if (value != NULL) {
        hybbx_strlcpy(out->country, value, sizeof(out->country));
    }

    value = hybbx_config_get(cfg, section, "location", NULL);
    if (value != NULL) {
        hybbx_strlcpy(out->location, value, sizeof(out->location));
    }

    value = hybbx_config_get(cfg, section, "email", NULL);
    if (value != NULL) {
        hybbx_strlcpy(out->email, value, sizeof(out->email));
    }

    value = hybbx_config_get(cfg, section, "password", NULL);
    if (value != NULL) {
        hybbx_strlcpy(out->password, value, sizeof(out->password));
    }

    return 1;
}

typedef struct section_list_ctx {
    char names[HYBBX_USERS_PER_FILE][HYBBX_CONFIG_SECTION_MAX];
    size_t count;
} section_list_ctx_t;

static void section_list_collect(const char *section, const char *key,
                                 const char *value, void *ctx)
{
    section_list_ctx_t *list = (section_list_ctx_t *)ctx;
    size_t i;

    (void)key;
    (void)value;

    if (section == NULL || list == NULL) {
        return;
    }

    if (strncmp(section, HYBBX_USER_INI_SECTION_PREFIX,
                strlen(HYBBX_USER_INI_SECTION_PREFIX)) != 0) {
        return;
    }

    for (i = 0; i < list->count; i++) {
        if (strcmp(list->names[i], section) == 0) {
            return;
        }
    }

    if (list->count >= HYBBX_USERS_PER_FILE) {
        return;
    }

    hybbx_strlcpy(list->names[list->count], section,
                  sizeof(list->names[list->count]));
    list->count++;
}

static hybbx_result_t load_user_shard(const char *path, user_shard_t *shard)
{
    hybbx_config_t cfg;
    section_list_ctx_t sections;
    size_t i;
    hybbx_result_t rc;

    if (path == NULL || shard == NULL) {
        return HYBBX_ERR_INVALID;
    }

    memset(shard, 0, sizeof(*shard));
    memset(&sections, 0, sizeof(sections));

    rc = hybbx_config_load(&cfg, path);
    if (rc != HYBBX_OK) {
        return rc;
    }

    hybbx_config_foreach(&cfg, section_list_collect, &sections);

    for (i = 0; i < sections.count; i++) {
        hybbx_user_record_t user;

        if (!parse_user_section(&cfg, sections.names[i], &user)) {
            continue;
        }

        if (shard->count >= HYBBX_USERS_PER_FILE) {
            hybbx_config_free(&cfg);
            return HYBBX_ERR_IO;
        }

        shard->users[shard->count++] = user;
    }

    hybbx_config_free(&cfg);
    return HYBBX_OK;
}

static hybbx_result_t save_user_shard(const struct flatfile_state *state,
                                      unsigned shard_index,
                                      const user_shard_t *shard)
{
    char path[HYBBX_PATH_MAX];
    FILE *fp;
    size_t i;
    hybbx_result_t rc;

    if (state == NULL || shard == NULL || shard->count > HYBBX_USERS_PER_FILE) {
        return HYBBX_ERR_INVALID;
    }

    rc = user_shard_path(state, shard_index, path, sizeof(path));
    if (rc != HYBBX_OK) {
        return rc;
    }

    fp = fopen(path, "w");
    if (fp == NULL) {
        return HYBBX_ERR_IO;
    }

    fprintf(fp, "; HyBBX user file (max %u users)\n",
            (unsigned)HYBBX_USERS_PER_FILE);
    fprintf(fp, "; shard %u\n\n", shard_index);

    for (i = 0; i < shard->count; i++) {
        const hybbx_user_record_t *user = &shard->users[i];

        fprintf(fp, "[user.%llu]\n", (unsigned long long)user->id);
        fprintf(fp, "id = %llu\n", (unsigned long long)user->id);
        fprintf(fp, "username = %s\n", user->username);
        fprintf(fp, "level = %s\n", hybbx_user_level_name(user->level));
        fprintf(fp, "active = %s\n", hybbx_bool_to_string(user->active));
        fprintf(fp, "created = %lld\n", (long long)user->created_at);
        fprintf(fp, "fullname = %s\n", user->full_name);
        fprintf(fp, "country = %s\n", user->country);
        fprintf(fp, "location = %s\n", user->location);
        fprintf(fp, "email = %s\n", user->email);
        fprintf(fp, "password = %s\n\n", user->password);
    }

    if (fclose(fp) != 0) {
        return HYBBX_ERR_IO;
    }

    return HYBBX_OK;
}

static hybbx_result_t find_user_shard(const struct flatfile_state *state,
                                      uint64_t user_id,
                                      unsigned *shard_index,
                                      size_t *slot_index)
{
    unsigned shard;
    unsigned last;
    char path[HYBBX_PATH_MAX];
    user_shard_t loaded;
    size_t i;
    hybbx_result_t rc;

    if (state == NULL || user_id == 0 || shard_index == NULL ||
        slot_index == NULL) {
        return HYBBX_ERR_INVALID;
    }

    last = last_user_shard_index(state);

    for (shard = 1; shard <= last; shard++) {
        if (user_shard_path(state, shard, path, sizeof(path)) != HYBBX_OK) {
            continue;
        }
        if (!file_exists(path)) {
            continue;
        }

        rc = load_user_shard(path, &loaded);
        if (rc != HYBBX_OK) {
            return rc;
        }

        for (i = 0; i < loaded.count; i++) {
            if (loaded.users[i].id == user_id) {
                *shard_index = shard;
                *slot_index = i;
                return HYBBX_OK;
            }
        }
    }

    return HYBBX_ERR_NOT_FOUND;
}

static hybbx_result_t append_user_record(struct flatfile_state *state,
                                         const hybbx_user_record_t *user)
{
    unsigned shard;
    unsigned last;
    char path[HYBBX_PATH_MAX];
    user_shard_t loaded;
    hybbx_result_t rc;

    if (state == NULL || user == NULL) {
        return HYBBX_ERR_INVALID;
    }

    last = last_user_shard_index(state);
    if (last == 0) {
        shard = 1;
        memset(&loaded, 0, sizeof(loaded));
    } else {
        rc = user_shard_path(state, last, path, sizeof(path));
        if (rc != HYBBX_OK) {
            return rc;
        }

        rc = load_user_shard(path, &loaded);
        if (rc != HYBBX_OK) {
            return rc;
        }

        if (loaded.count >= HYBBX_USERS_PER_FILE) {
            shard = last + 1;
            memset(&loaded, 0, sizeof(loaded));
        } else {
            shard = last;
        }
    }

    if (loaded.count >= HYBBX_USERS_PER_FILE) {
        return HYBBX_ERR_BUSY;
    }

    loaded.users[loaded.count++] = *user;
    return save_user_shard(state, shard, &loaded);
}

static hybbx_result_t ensure_default_sysop(hybbx_storage_t *storage)
{
    struct flatfile_state *state;
    size_t sysop_count = 0;
    hybbx_user_record_t sysop;
    uint64_t user_id;
    time_t now = time(NULL);
    hybbx_result_t rc;

    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    state = storage->backend_data;
    if (state == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_storage_flatfile_count_level(storage, HYBBX_LEVEL_SYSOP,
                                            &sysop_count);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (sysop_count > 0) {
        return HYBBX_OK;
    }

    rc = bump_counter(state->user_next_path, &user_id);
    if (rc != HYBBX_OK) {
        return rc;
    }

    memset(&sysop, 0, sizeof(sysop));
    sysop.id = user_id;
    hybbx_strlcpy(sysop.username, HYBBX_DEFAULT_SYSOP_USERNAME,
                  sizeof(sysop.username));
    hybbx_username_normalize(sysop.username);
    sysop.level = HYBBX_LEVEL_SYSOP;
    sysop.active = 1;
    sysop.created_at = now;
    hybbx_strlcpy(sysop.full_name, "System Operator", sizeof(sysop.full_name));
    if (hybbx_password_hash(HYBBX_DEFAULT_SYSOP_PASSWORD,
                            sysop.password, sizeof(sysop.password)) != HYBBX_OK) {
        return HYBBX_ERR_IO;
    }

    rc = append_user_record(state, &sysop);
    if (rc != HYBBX_OK) {
        return rc;
    }

    printf("[storage] created default Sysop account (username %s)\n",
           HYBBX_DEFAULT_SYSOP_USERNAME);
    return HYBBX_OK;
}

static hybbx_result_t foreach_user(struct flatfile_state *state,
                                   hybbx_result_t (*fn)(const hybbx_user_record_t *user,
                                                        void *ctx),
                                   void *ctx)
{
    unsigned shard;
    unsigned last;
    char path[HYBBX_PATH_MAX];
    user_shard_t loaded;
    size_t i;
    hybbx_result_t rc;

    if (state == NULL || fn == NULL) {
        return HYBBX_ERR_INVALID;
    }

    last = last_user_shard_index(state);
    for (shard = 1; shard <= last; shard++) {
        rc = user_shard_path(state, shard, path, sizeof(path));
        if (rc != HYBBX_OK) {
            return rc;
        }

        rc = load_user_shard(path, &loaded);
        if (rc != HYBBX_OK) {
            return rc;
        }

        for (i = 0; i < loaded.count; i++) {
            rc = fn(&loaded.users[i], ctx);
            if (rc != HYBBX_OK) {
                return rc;
            }
        }
    }

    return HYBBX_OK;
}

typedef struct find_user_ctx {
    const char *username;
    hybbx_user_record_t *out;
    int found;
} find_user_ctx_t;

static hybbx_result_t find_user_cb(const hybbx_user_record_t *user, void *ctx)
{
    find_user_ctx_t *fctx = (find_user_ctx_t *)ctx;

    if (str_ieq(user->username, fctx->username)) {
        *fctx->out = *user;
        fctx->found = 1;
        return HYBBX_ERR_BUSY;
    }

    return HYBBX_OK;
}

typedef struct count_level_ctx {
    hybbx_user_level_t level;
    size_t count;
} count_level_ctx_t;

static hybbx_result_t count_level_cb(const hybbx_user_record_t *user, void *ctx)
{
    count_level_ctx_t *cctx = (count_level_ctx_t *)ctx;

    if (user->level == cctx->level) {
        cctx->count++;
    }

    return HYBBX_OK;
}

typedef struct migrate_passwords_ctx {
    hybbx_storage_t *storage;
    hybbx_result_t last_error;
} migrate_passwords_ctx_t;

static hybbx_result_t migrate_passwords_cb(const hybbx_user_record_t *user,
                                           void *ctx)
{
    migrate_passwords_ctx_t *mctx = (migrate_passwords_ctx_t *)ctx;
    hybbx_user_record_t updated;
    hybbx_result_t rc;

    if (!hybbx_password_is_plain(user->password)) {
        return HYBBX_OK;
    }

    updated = *user;
    rc = hybbx_password_hash(user->password, updated.password,
                             sizeof(updated.password));
    if (rc != HYBBX_OK) {
        mctx->last_error = rc;
        return rc;
    }

    rc = hybbx_storage_update_user(mctx->storage, &updated);
    if (rc != HYBBX_OK) {
        mctx->last_error = rc;
        return rc;
    }

    printf("[storage] upgraded plain password to sha256 for user %s\n",
           user->username);
    return HYBBX_OK;
}

static hybbx_result_t migrate_legacy_users_dat(hybbx_storage_t *storage)
{
    struct flatfile_state *state;
    FILE *fp;
    char line[HYBBX_CONFIG_LINE_MAX];
    user_shard_t shard;
    unsigned shard_index = 1;
    hybbx_result_t rc;

    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    state = storage->backend_data;
    if (state == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (!file_exists(state->legacy_users_path)) {
        return HYBBX_OK;
    }

    if (last_user_shard_index(state) > 0) {
        return HYBBX_OK;
    }

    fp = fopen(state->legacy_users_path, "r");
    if (fp == NULL) {
        return HYBBX_ERR_IO;
    }

    memset(&shard, 0, sizeof(shard));

    while (fgets(line, sizeof(line), fp) != NULL) {
        hybbx_user_record_t user;
        char *nl;

        nl = strchr(line, '\n');
        if (nl != NULL) {
            *nl = '\0';
        }

        if (!parse_legacy_user_line(line, &user)) {
            continue;
        }

        if (shard.count >= HYBBX_USERS_PER_FILE) {
            rc = save_user_shard(state, shard_index, &shard);
            if (rc != HYBBX_OK) {
                fclose(fp);
                return rc;
            }
            shard_index++;
            memset(&shard, 0, sizeof(shard));
        }

        shard.users[shard.count++] = user;
    }

    fclose(fp);

    if (shard.count > 0) {
        rc = save_user_shard(state, shard_index, &shard);
        if (rc != HYBBX_OK) {
            return rc;
        }
    }

    {
        char backup[HYBBX_PATH_MAX];

        if (hybbx_path_join(backup, sizeof(backup), storage->path,
                            "users.dat.migrated") == HYBBX_OK) {
            (void)rename(state->legacy_users_path, backup);
        }
    }

    printf("[storage] migrated legacy users.dat to INI user files\n");
    return HYBBX_OK;
}

static hybbx_result_t migrate_plain_passwords(hybbx_storage_t *storage)
{
    struct flatfile_state *state;
    migrate_passwords_ctx_t ctx;

    if (storage == NULL) {
        return HYBBX_ERR_INVALID;
    }

    state = storage->backend_data;
    if (state == NULL) {
        return HYBBX_ERR_INVALID;
    }

    ctx.storage = storage;
    ctx.last_error = HYBBX_OK;
    return foreach_user(state, migrate_passwords_cb, &ctx);
}

hybbx_result_t hybbx_storage_flatfile_open(hybbx_storage_t *storage)
{
    struct flatfile_state *state;
    hybbx_result_t rc;

    if (storage == NULL || storage->path == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (mkdir_p(storage->path) != 0) {
        return HYBBX_ERR_IO;
    }

    state = calloc(1, sizeof(*state));
    if (state == NULL) {
        return HYBBX_ERR_NOMEM;
    }

    rc = hybbx_path_join(state->users_dir, sizeof(state->users_dir),
                         storage->path, "users");
    if (rc != HYBBX_OK) {
        free(state);
        return rc;
    }

    if (mkdir_p(state->users_dir) != 0) {
        free(state);
        return HYBBX_ERR_IO;
    }

    rc = hybbx_path_join(state->legacy_users_path, sizeof(state->legacy_users_path),
                         storage->path, "users.dat");
    if (rc != HYBBX_OK) {
        free(state);
        return rc;
    }

    rc = hybbx_path_join(state->sessions_path, sizeof(state->sessions_path),
                   storage->path, "sessions.dat");
    if (rc != HYBBX_OK) {
        free(state);
        return rc;
    }

    rc = hybbx_path_join(state->guest_next_path, sizeof(state->guest_next_path),
                   storage->path, "guest.next");
    if (rc != HYBBX_OK) {
        free(state);
        return rc;
    }

    rc = hybbx_path_join(state->session_next_path, sizeof(state->session_next_path),
                   storage->path, "session.next");
    if (rc != HYBBX_OK) {
        free(state);
        return rc;
    }

    rc = hybbx_path_join(state->user_next_path, sizeof(state->user_next_path),
                   storage->path, "user.next");
    if (rc != HYBBX_OK) {
        free(state);
        return rc;
    }

    storage->backend_data = state;

    rc = migrate_legacy_users_dat(storage);
    if (rc != HYBBX_OK) {
        hybbx_storage_flatfile_close(storage);
        return rc;
    }

    rc = ensure_default_sysop(storage);
    if (rc != HYBBX_OK) {
        hybbx_storage_flatfile_close(storage);
        return rc;
    }

    rc = migrate_plain_passwords(storage);
    if (rc != HYBBX_OK) {
        hybbx_storage_flatfile_close(storage);
        return rc;
    }

    return HYBBX_OK;
}

void hybbx_storage_flatfile_close(hybbx_storage_t *storage)
{
    if (storage == NULL) {
        return;
    }

    free(storage->backend_data);
    storage->backend_data = NULL;
}

hybbx_result_t hybbx_storage_flatfile_create_guest(hybbx_storage_t *storage,
                                                   hybbx_user_record_t *out)
{
    struct flatfile_state *state = storage->backend_data;
    uint64_t guest_index;
    uint64_t guest_number;
    uint64_t user_id;
    time_t now = time(NULL);
    hybbx_result_t rc;

    if (state == NULL || out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = read_counter(state->guest_next_path, &guest_index);
    if (rc != HYBBX_OK) {
        return rc;
    }

    guest_number = guest_index + 1;
    if (guest_number > HYBBX_GUEST_NUMBER_MAX) {
        return HYBBX_ERR_BUSY;
    }

    rc = bump_counter(state->user_next_path, &user_id);
    if (rc != HYBBX_OK) {
        return rc;
    }

    memset(out, 0, sizeof(*out));
    out->id = user_id;
    snprintf(out->username, sizeof(out->username), "%s%llu",
             storage->guest_prefix, (unsigned long long)guest_number);
    out->level = HYBBX_LEVEL_GUEST;
    out->active = 1;
    out->created_at = now;

    rc = append_user_record(state, out);
    if (rc != HYBBX_OK) {
        return rc;
    }

    return write_counter(state->guest_next_path, guest_number);
}

hybbx_result_t hybbx_storage_flatfile_find_user(hybbx_storage_t *storage,
                                                const char *username,
                                                hybbx_user_record_t *out)
{
    struct flatfile_state *state;
    find_user_ctx_t ctx;
    hybbx_result_t rc;

    if (storage == NULL || username == NULL || out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    state = storage->backend_data;
    if (state == NULL) {
        return HYBBX_ERR_INVALID;
    }

    memset(out, 0, sizeof(*out));
    ctx.username = username;
    ctx.out = out;
    ctx.found = 0;

    rc = foreach_user(state, find_user_cb, &ctx);
    if (rc == HYBBX_ERR_BUSY) {
        return HYBBX_OK;
    }
    if (rc != HYBBX_OK) {
        return rc;
    }

    return HYBBX_ERR_NOT_FOUND;
}

hybbx_result_t hybbx_storage_flatfile_count_level(hybbx_storage_t *storage,
                                                  hybbx_user_level_t level,
                                                  size_t *count)
{
    struct flatfile_state *state;
    count_level_ctx_t ctx;
    hybbx_result_t rc;

    if (storage == NULL || count == NULL) {
        return HYBBX_ERR_INVALID;
    }

    state = storage->backend_data;
    if (state == NULL) {
        return HYBBX_ERR_INVALID;
    }

    ctx.level = level;
    ctx.count = 0;

    rc = foreach_user(state, count_level_cb, &ctx);
    if (rc != HYBBX_OK) {
        return rc;
    }

    *count = ctx.count;
    return HYBBX_OK;
}

hybbx_result_t hybbx_storage_flatfile_register_user(hybbx_storage_t *storage,
                                                    const hybbx_user_registration_t *reg,
                                                    hybbx_user_record_t *out)
{
    struct flatfile_state *state;
    hybbx_user_record_t existing;
    uint64_t user_id;
    time_t now = time(NULL);
    hybbx_result_t rc;

    if (storage == NULL || reg == NULL || out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    if (!hybbx_registration_valid(reg, storage->guest_prefix)) {
        return HYBBX_ERR_INVALID;
    }

    state = storage->backend_data;
    if (state == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = hybbx_storage_flatfile_find_user(storage, reg->username, &existing);
    if (rc == HYBBX_OK) {
        return HYBBX_ERR_BUSY;
    }
    if (rc != HYBBX_ERR_NOT_FOUND) {
        return rc;
    }

    rc = bump_counter(state->user_next_path, &user_id);
    if (rc != HYBBX_OK) {
        return rc;
    }

    memset(out, 0, sizeof(*out));
    out->id = user_id;
    hybbx_strlcpy(out->username, reg->username, sizeof(out->username));
    out->level = HYBBX_LEVEL_USER;
    out->active = 0;
    out->created_at = now;
    hybbx_strlcpy(out->full_name, reg->full_name, sizeof(out->full_name));
    hybbx_strlcpy(out->country, reg->country, sizeof(out->country));
    hybbx_strlcpy(out->location, reg->location, sizeof(out->location));
    hybbx_strlcpy(out->email, reg->email, sizeof(out->email));

    return append_user_record(state, out);
}

hybbx_result_t hybbx_storage_flatfile_update_user(hybbx_storage_t *storage,
                                                  const hybbx_user_record_t *user)
{
    struct flatfile_state *state;
    unsigned shard_index;
    size_t slot_index;
    char path[HYBBX_PATH_MAX];
    user_shard_t shard;
    hybbx_result_t rc;

    if (storage == NULL || user == NULL || user->id == 0) {
        return HYBBX_ERR_INVALID;
    }

    state = storage->backend_data;
    if (state == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = find_user_shard(state, user->id, &shard_index, &slot_index);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = user_shard_path(state, shard_index, path, sizeof(path));
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = load_user_shard(path, &shard);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (slot_index >= shard.count) {
        return HYBBX_ERR_NOT_FOUND;
    }

    shard.users[slot_index] = *user;
    return save_user_shard(state, shard_index, &shard);
}

hybbx_result_t hybbx_storage_flatfile_delete_user(hybbx_storage_t *storage,
                                                  uint64_t user_id)
{
    struct flatfile_state *state;
    unsigned shard_index;
    size_t slot_index;
    char path[HYBBX_PATH_MAX];
    user_shard_t shard;
    hybbx_result_t rc;

    if (storage == NULL || user_id == 0) {
        return HYBBX_ERR_INVALID;
    }

    state = storage->backend_data;
    if (state == NULL) {
        return HYBBX_ERR_INVALID;
    }

    rc = find_user_shard(state, user_id, &shard_index, &slot_index);
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = user_shard_path(state, shard_index, path, sizeof(path));
    if (rc != HYBBX_OK) {
        return rc;
    }

    rc = load_user_shard(path, &shard);
    if (rc != HYBBX_OK) {
        return rc;
    }

    if (slot_index >= shard.count) {
        return HYBBX_ERR_NOT_FOUND;
    }

    if (slot_index + 1 < shard.count) {
        memmove(&shard.users[slot_index], &shard.users[slot_index + 1],
                (shard.count - slot_index - 1) * sizeof(shard.users[0]));
    }
    shard.count--;

    return save_user_shard(state, shard_index, &shard);
}

hybbx_result_t hybbx_storage_flatfile_session_begin(hybbx_storage_t *storage,
                                                    const hybbx_user_record_t *user,
                                                    const char *transport,
                                                    hybbx_session_record_t *out)
{
    struct flatfile_state *state = storage->backend_data;
    char line[384];
    time_t now = time(NULL);
    hybbx_result_t rc;

    if (state == NULL || user == NULL || transport == NULL || out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    memset(out, 0, sizeof(*out));

    rc = bump_counter(state->session_next_path, &out->session_id);
    if (rc != HYBBX_OK) {
        return rc;
    }

    out->user_id = user->id;
    hybbx_strlcpy(out->username, user->username, sizeof(out->username));
    hybbx_strlcpy(out->transport, transport, sizeof(out->transport));
    out->connected_at = now;
    out->active = 1;

    snprintf(line, sizeof(line), "%llu|%llu|%s|%s|%lld|0|1",
             (unsigned long long)out->session_id,
             (unsigned long long)out->user_id,
             out->username, out->transport, (long long)out->connected_at);

    return append_line(state->sessions_path, line);
}

hybbx_result_t hybbx_storage_flatfile_session_end(hybbx_storage_t *storage,
                                                  uint64_t session_id)
{
    struct flatfile_state *state = storage->backend_data;
    char line[384];
    time_t now = time(NULL);

    if (state == NULL || session_id == 0) {
        return HYBBX_ERR_INVALID;
    }

    snprintf(line, sizeof(line), "%llu|0|_|_|0|%lld|0",
             (unsigned long long)session_id, (long long)now);

    return append_line(state->sessions_path, line);
}
