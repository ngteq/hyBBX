#include "hybbx/storage.h"
#include "hybbx/auth.h"
#include "hybbx/util.h"
#include "storage_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *hybbx_strdup(const char *s)
{
    size_t len;
    char *copy;

    if (s == NULL) {
        return NULL;
    }

    len = strlen(s) + 1;
    copy = malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len);
    }
    return copy;
}

hybbx_storage_t *hybbx_storage_open(const hybbx_storage_options_t *options)
{
    hybbx_storage_t *storage;
    hybbx_result_t rc;

    if (options == NULL || options->path == NULL) {
        return NULL;
    }

    storage = calloc(1, sizeof(*storage));
    if (storage == NULL) {
        return NULL;
    }

    storage->backend = options->backend;
    storage->path = hybbx_strdup(options->path);
    if (storage->path == NULL) {
        free(storage);
        return NULL;
    }

    if (options->guest_prefix != NULL && options->guest_prefix[0] != '\0') {
        hybbx_strlcpy(storage->guest_prefix, options->guest_prefix,
                      sizeof(storage->guest_prefix));
    } else {
        hybbx_strlcpy(storage->guest_prefix, HYBBX_AUTH_DEFAULT_GUEST_PREFIX,
                      sizeof(storage->guest_prefix));
    }

    switch (storage->backend) {
    case HYBBX_STORAGE_FLATFILE:
        rc = hybbx_storage_flatfile_open(storage);
        break;
    case HYBBX_STORAGE_SQLITE:
    case HYBBX_STORAGE_MYSQL:
    case HYBBX_STORAGE_MARIADB:
        rc = hybbx_storage_sql_open(storage);
        break;
    default:
        rc = HYBBX_ERR_UNSUPPORTED;
        break;
    }

    if (rc != HYBBX_OK) {
        hybbx_storage_close(storage);
        return NULL;
    }

    return storage;
}

void hybbx_storage_close(hybbx_storage_t *storage)
{
    if (storage == NULL) {
        return;
    }

    switch (storage->backend) {
    case HYBBX_STORAGE_FLATFILE:
        hybbx_storage_flatfile_close(storage);
        break;
    case HYBBX_STORAGE_SQLITE:
    case HYBBX_STORAGE_MYSQL:
    case HYBBX_STORAGE_MARIADB:
        hybbx_storage_sql_close(storage);
        break;
    default:
        break;
    }

    free(storage->path);
    free(storage);
}

hybbx_storage_backend_kind_t hybbx_storage_backend(const hybbx_storage_t *storage)
{
    if (storage == NULL) {
        return HYBBX_STORAGE_FLATFILE;
    }
    return storage->backend;
}

hybbx_result_t hybbx_storage_register_user(hybbx_storage_t *storage,
                                           const hybbx_user_registration_t *reg,
                                           hybbx_user_record_t *out)
{
    if (storage == NULL || reg == NULL || out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    switch (storage->backend) {
    case HYBBX_STORAGE_FLATFILE:
        return hybbx_storage_flatfile_register_user(storage, reg, out);
    default:
        return HYBBX_ERR_UNSUPPORTED;
    }
}

hybbx_result_t hybbx_storage_find_user(hybbx_storage_t *storage,
                                       const char *username,
                                       hybbx_user_record_t *out)
{
    if (storage == NULL || username == NULL || out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    switch (storage->backend) {
    case HYBBX_STORAGE_FLATFILE:
        return hybbx_storage_flatfile_find_user(storage, username, out);
    default:
        return HYBBX_ERR_UNSUPPORTED;
    }
}

hybbx_result_t hybbx_storage_resolve_user(hybbx_storage_t *storage,
                                          const char *name,
                                          hybbx_user_record_t *out)
{
    if (storage == NULL || name == NULL || out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    switch (storage->backend) {
    case HYBBX_STORAGE_FLATFILE:
        return hybbx_storage_flatfile_resolve_user(storage, name, out);
    default:
        return HYBBX_ERR_UNSUPPORTED;
    }
}

hybbx_result_t hybbx_storage_count_level(hybbx_storage_t *storage,
                                         hybbx_user_level_t level,
                                         size_t *count)
{
    if (storage == NULL || count == NULL) {
        return HYBBX_ERR_INVALID;
    }

    switch (storage->backend) {
    case HYBBX_STORAGE_FLATFILE:
        return hybbx_storage_flatfile_count_level(storage, level, count);
    default:
        return HYBBX_ERR_UNSUPPORTED;
    }
}

hybbx_result_t hybbx_storage_foreach_user(hybbx_storage_t *storage,
                                          hybbx_storage_user_fn fn,
                                          void *ctx)
{
    if (storage == NULL || fn == NULL) {
        return HYBBX_ERR_INVALID;
    }

    switch (storage->backend) {
    case HYBBX_STORAGE_FLATFILE:
        return hybbx_storage_flatfile_foreach_user(storage, fn, ctx);
    default:
        return HYBBX_ERR_UNSUPPORTED;
    }
}

hybbx_result_t hybbx_storage_session_begin(hybbx_storage_t *storage,
                                           const hybbx_user_record_t *user,
                                           const char *transport,
                                           hybbx_session_record_t *out)
{
    if (storage == NULL || user == NULL || transport == NULL || out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    switch (storage->backend) {
    case HYBBX_STORAGE_FLATFILE:
        return hybbx_storage_flatfile_session_begin(storage, user, transport, out);
    default:
        return HYBBX_ERR_UNSUPPORTED;
    }
}

hybbx_result_t hybbx_storage_session_end(hybbx_storage_t *storage,
                                         uint64_t session_id)
{
    if (storage == NULL || session_id == 0) {
        return HYBBX_ERR_INVALID;
    }

    switch (storage->backend) {
    case HYBBX_STORAGE_FLATFILE:
        return hybbx_storage_flatfile_session_end(storage, session_id);
    default:
        return HYBBX_ERR_UNSUPPORTED;
    }
}

hybbx_result_t hybbx_storage_update_user(hybbx_storage_t *storage,
                                         const hybbx_user_record_t *user)
{
    if (storage == NULL || user == NULL) {
        return HYBBX_ERR_INVALID;
    }

    switch (storage->backend) {
    case HYBBX_STORAGE_FLATFILE:
        return hybbx_storage_flatfile_update_user(storage, user);
    default:
        return HYBBX_ERR_UNSUPPORTED;
    }
}

hybbx_result_t hybbx_storage_delete_user(hybbx_storage_t *storage,
                                         uint64_t user_id)
{
    if (storage == NULL || user_id == 0) {
        return HYBBX_ERR_INVALID;
    }

    switch (storage->backend) {
    case HYBBX_STORAGE_FLATFILE:
        return hybbx_storage_flatfile_delete_user(storage, user_id);
    default:
        return HYBBX_ERR_UNSUPPORTED;
    }
}
