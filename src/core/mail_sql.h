#ifndef HYBBX_MAIL_SQL_H
#define HYBBX_MAIL_SQL_H

#include "hybbx/mail.h"
#include "hybbx/storage.h"
#include "hybbx/types.h"

struct hybbx_service;
struct hybbx_session;

hybbx_result_t hybbx_mail_sql_load_inbox(hybbx_storage_t *storage,
                                         const char *username,
                                         hybbx_mail_entry_t *entries,
                                         size_t max_entries,
                                         size_t *out_count);

hybbx_result_t hybbx_mail_sql_deliver(hybbx_storage_t *storage,
                                      const hybbx_mail_config_t *mail,
                                      const char *from_user,
                                      const char *to_user,
                                      const char *subject,
                                      const char *body);

hybbx_result_t hybbx_mail_sql_read(struct hybbx_service *service,
                                   struct hybbx_session *session,
                                   unsigned list_index);

hybbx_result_t hybbx_mail_sql_delete_range(struct hybbx_service *service,
                                           struct hybbx_session *session,
                                           unsigned from,
                                           unsigned to);

hybbx_result_t hybbx_mail_sql_recycle_empty(struct hybbx_service *service,
                                            struct hybbx_session *session);

unsigned hybbx_mail_sql_purge_recycle(hybbx_storage_t *storage,
                                      const hybbx_mail_config_t *mail,
                                      const char *username);

#endif /* HYBBX_MAIL_SQL_H */
