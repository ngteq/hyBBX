#ifndef HYBBX_PROXYMAIL_H
#define HYBBX_PROXYMAIL_H

/**
 * Inter-Main mail via mains_proxy — separate from local `/mail`.
 *
 * Local mail uses flat-file or SQL under `[mail]`; proxymail will use its
 * own storage on each Main once the mesh is implemented. Stub only for now.
 */

#include "hybbx/mail.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** `username@remote-main-service` address string. */
#define HYBBX_PROXYMAIL_ADDRESS_MAX     128u
#define HYBBX_PROXYMAIL_SERVICE_NAME_MAX  64u

struct hybbx_service;
struct hybbx_session;

/**
 * Parse @p address as @c user@remote-service .
 * @return 0 when invalid.
 */
int hybbx_proxymail_parse_address(const char *address,
                                  char *user, size_t user_len,
                                  char *remote_service, size_t remote_len);

/** List proxymail inbox (stub). */
void hybbx_proxymail_list_inbox(struct hybbx_service *service,
                                struct hybbx_session *session);

void hybbx_proxymail_list_inbox_range(struct hybbx_service *service,
                                      struct hybbx_session *session,
                                      unsigned from, unsigned to);

hybbx_result_t hybbx_proxymail_read(struct hybbx_service *service,
                                    struct hybbx_session *session,
                                    unsigned list_index);

hybbx_result_t hybbx_proxymail_delete_range(struct hybbx_service *service,
                                            struct hybbx_session *session,
                                            unsigned from, unsigned to);

hybbx_result_t hybbx_proxymail_recycle_empty(struct hybbx_service *service,
                                             struct hybbx_session *session);

/**
 * Queue outbound proxymail (stub — mesh delivery not active).
 * @p to_address must be @c user@remote-main-service .
 */
hybbx_result_t hybbx_proxymail_deliver(struct hybbx_service *service,
                                       const char *from_user,
                                       const char *to_address,
                                       const char *subject,
                                       const char *body);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_PROXYMAIL_H */
