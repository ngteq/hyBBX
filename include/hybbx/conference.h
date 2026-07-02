#ifndef HYBBX_CONFERENCE_H
#define HYBBX_CONFERENCE_H

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_service;
struct hybbx_session;

#define HYBBX_CONFERENCE_TOPIC_MAX 32

/** Invite rate limit: max invites per target user within the window. */
#define HYBBX_CONFERENCE_INVITE_MAX_PER_TARGET 2u

/** Invite rate limit window (seconds). */
#define HYBBX_CONFERENCE_INVITE_WINDOW_SEC 1800u

/**
 * Send a conference invite: @p topic to @p partner (login or nickname).
 * Partner must accept with y/n or yes/no. Max two invites per target per 30 min.
 */
hybbx_result_t hybbx_conference_start(struct hybbx_service *service,
                                      struct hybbx_session *initiator,
                                      const char *topic,
                                      const char *partner);

/** Non-zero when @p session has a pending conference invite to answer. */
int hybbx_conference_invite_pending(const struct hybbx_session *session);

/**
 * Handle invite reply line (y/n, yes/no). Call for local input while invite pending.
 * Returns HYBBX_OK when consumed.
 */
hybbx_result_t hybbx_conference_reply_invite(struct hybbx_service *service,
                                           struct hybbx_session *session,
                                           const char *line);

/** Clear pending invites involving @p session (disconnect). */
void hybbx_conference_session_closed(struct hybbx_session *session);

/** Deliver a line within an active conference (sender must be in conference area). */
hybbx_result_t hybbx_conference_post(struct hybbx_service *service,
                                     struct hybbx_session *from,
                                     const char *message);

/** Tear down conference state when leaving the conference area. */
void hybbx_conference_area_leaving(struct hybbx_session *session);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_CONFERENCE_H */
