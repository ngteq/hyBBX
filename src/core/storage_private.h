#ifndef HYBBX_STORAGE_PRIVATE_H
#define HYBBX_STORAGE_PRIVATE_H

#include "hybbx/auth.h"
#include "hybbx/storage.h"

struct hybbx_storage {
    hybbx_storage_backend_kind_t backend;
    char *path;
    char guest_prefix[HYBBX_AUTH_GUEST_PREFIX_MAX];
    void *backend_data;
};

hybbx_result_t hybbx_storage_flatfile_open(hybbx_storage_t *storage);
void hybbx_storage_flatfile_close(hybbx_storage_t *storage);
hybbx_result_t hybbx_storage_flatfile_session_begin(hybbx_storage_t *storage,
                                                    const hybbx_user_record_t *user,
                                                    const char *transport,
                                                    hybbx_session_record_t *out);
hybbx_result_t hybbx_storage_flatfile_session_end(hybbx_storage_t *storage,
                                                  uint64_t session_id);
hybbx_result_t hybbx_storage_flatfile_find_user(hybbx_storage_t *storage,
                                               const char *username,
                                               hybbx_user_record_t *out);
hybbx_result_t hybbx_storage_flatfile_count_level(hybbx_storage_t *storage,
                                                 hybbx_user_level_t level,
                                                 size_t *count);
hybbx_result_t hybbx_storage_flatfile_foreach_user(
    hybbx_storage_t *storage,
    hybbx_storage_user_fn fn,
    void *ctx);
hybbx_result_t hybbx_storage_flatfile_register_user(hybbx_storage_t *storage,
                                                    const hybbx_user_registration_t *reg,
                                                    hybbx_user_record_t *out);
hybbx_result_t hybbx_storage_flatfile_update_user(hybbx_storage_t *storage,
                                                  const hybbx_user_record_t *user);
hybbx_result_t hybbx_storage_flatfile_delete_user(hybbx_storage_t *storage,
                                                  uint64_t user_id);

hybbx_result_t hybbx_storage_sql_open(hybbx_storage_t *storage);
void hybbx_storage_sql_close(hybbx_storage_t *storage);

#endif /* HYBBX_STORAGE_PRIVATE_H */
