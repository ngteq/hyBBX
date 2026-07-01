#ifndef HYBBX_MAIL_H
#define HYBBX_MAIL_H

#include "hybbx/config.h"
#include "hybbx/storage.h"
#include "hybbx/types.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_service;
struct hybbx_session;

/** Maximum stored messages per user inbox. */
#define HYBBX_MAIL_MAX_MESSAGES 50u

/** Subject line limit (80-column profile). */
#define HYBBX_MAIL_SUBJECT_MAX 72u

/** Total message body size (all lines). */
#define HYBBX_MAIL_BODY_MAX 2048u

/** Default days before recycled mail is permanently removed. */
#define HYBBX_MAIL_DEFAULT_RECYCLE_DAYS 10u

typedef struct hybbx_mail_config {
    int enabled;
    unsigned max_messages;
    unsigned subject_max;
    unsigned body_max;
    unsigned recycle_days;
    /** Root mail directory (typically <storage>/mail). */
    char root[512];
} hybbx_mail_config_t;

typedef struct hybbx_mail_entry {
    uint64_t id;
    char from[64];
    char subject[HYBBX_MAIL_SUBJECT_MAX];
    time_t received_at;
    int read;
} hybbx_mail_entry_t;

void hybbx_mail_config_defaults(hybbx_mail_config_t *mail);

/** Load `[mail]` and set @p mail->root under @p storage_path. */
void hybbx_mail_config_apply(hybbx_mail_config_t *mail,
                             const hybbx_config_t *config,
                             const char *storage_path);

const hybbx_mail_config_t *hybbx_service_get_mail(const struct hybbx_service *service);

/** List inbox on @p session (registered users only). */
void hybbx_mail_list_inbox(struct hybbx_service *service,
                           struct hybbx_session *session);

/**
 * List inbox rows @p from..@p to (1-based, newest first).
 * @p to = 0 means through the last message.
 */
void hybbx_mail_list_inbox_range(struct hybbx_service *service,
                                 struct hybbx_session *session,
                                 unsigned from, unsigned to);

/**
 * Parse @p spec as @c from-to (e.g. @c 1-15 , @c 5-20 ) or a single index.
 * Returns 0 on invalid input.
 */
int hybbx_mail_parse_list_range(const char *spec,
                                unsigned *from, unsigned *to);

/**
 * Read message by 1-based list index (newest first).
 * Marks the message as read.
 */
hybbx_result_t hybbx_mail_read(struct hybbx_service *service,
                               struct hybbx_session *session,
                               unsigned list_index);

/** Move inbox message(s) to recycle (1-based index or range). */
hybbx_result_t hybbx_mail_delete(struct hybbx_service *service,
                                 struct hybbx_session *session,
                                 unsigned list_index);

hybbx_result_t hybbx_mail_delete_range(struct hybbx_service *service,
                                       struct hybbx_session *session,
                                       unsigned from, unsigned to);

/** Permanently remove all messages in the user's recycle bin. */
hybbx_result_t hybbx_mail_recycle_empty(struct hybbx_service *service,
                                        struct hybbx_session *session);

/**
 * Deliver a message from @p from_user to @p to_user.
 * @p to_user must exist and be an active registered account.
 */
hybbx_result_t hybbx_mail_deliver(struct hybbx_service *service,
                                  const char *from_user,
                                  const char *to_user,
                                  const char *subject,
                                  const char *body);

/**
 * Mail active Sysop and Admin accounts when a guest self-registers.
 * No-op when mail is disabled. Individual delivery failures are ignored.
 */
hybbx_result_t hybbx_mail_notify_staff_registration(
    struct hybbx_service *service,
    const hybbx_user_record_t *registered);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_MAIL_H */
