#ifndef HYBBX_CHAT_H
#define HYBBX_CHAT_H

#include "hybbx/config.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_service;
struct hybbx_session;

/** Hard limit on chat channels (also the default configured count). */
#define HYBBX_CHAT_CHANNEL_MAX 10u

#define HYBBX_CHAT_CHANNEL_NAME_MAX 32

#define HYBBX_DEFAULT_CHAT_CHANNELS HYBBX_CHAT_CHANNEL_MAX

/** Maximum characters per chat message line (fits 80-column wrap). */
#define HYBBX_CHAT_MESSAGE_MAX 72u

typedef struct hybbx_chat_config {
    /** Active channels (1 … @ref HYBBX_CHAT_CHANNEL_MAX). */
    unsigned channel_count;
    /** Maximum characters per chat message line. */
    unsigned message_max;
    /**
     * Channel names (index 0 = channel 1). Default: Channel1 … Channel10.
     * Override with [chat] channel1 … channel10 in INI.
     */
    char names[HYBBX_CHAT_CHANNEL_MAX][HYBBX_CHAT_CHANNEL_NAME_MAX];
} hybbx_chat_config_t;

void hybbx_chat_config_defaults(hybbx_chat_config_t *chat);

/** Load `[chat]` settings from an INI file. */
void hybbx_chat_config_apply(hybbx_chat_config_t *chat,
                             const hybbx_config_t *config);

const hybbx_chat_config_t *hybbx_service_get_chat(const struct hybbx_service *service);

/** Return channel name for @p channel_index (1-based), or NULL when invalid. */
const char *hybbx_chat_channel_name(const hybbx_chat_config_t *chat,
                                    unsigned channel_index);

/**
 * Resolve @p spec as channel number (1-based) or name (case-insensitive).
 * @p out_index receives 1-based channel index on success.
 */
hybbx_result_t hybbx_chat_resolve_channel(const hybbx_chat_config_t *chat,
                                          const char *spec,
                                          unsigned *out_index);

/** List configured channels on @p session. */
void hybbx_chat_list_channels(struct hybbx_session *session,
                              const hybbx_chat_config_t *chat);

/** Broadcast a chat line within the sender's channel.
 * Sender sees "ME: …"; others see "<username>: …".
 */
hybbx_result_t hybbx_chat_post(struct hybbx_service *service,
                               struct hybbx_session *from,
                               const char *message);

/** List display names in the requester's current chat channel only. */
void hybbx_chat_show_channel(struct hybbx_service *service,
                             struct hybbx_session *session);

/**
 * List all users in public chat channels as nick@ChannelN, sorted by channel,
 * wrapped at 80 columns (conference sessions excluded).
 */
void hybbx_chat_show_all(struct hybbx_service *service,
                         struct hybbx_session *session);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_CHAT_H */
