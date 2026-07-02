#ifndef HYBBX_TEXTS_H
#define HYBBX_TEXTS_H

#include "hybbx/types.h"
#include "hybbx/limits.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_session;

#define HYBBX_DEFAULT_TEXTS_PATH HYBBX_DIR_TEXT

#define HYBBX_TEXT_BANNER "banner.txt"
#define HYBBX_TEXT_MOTD   "motd.txt"
#define HYBBX_TEXT_NEWS   "news.txt"
#define HYBBX_TEXT_RULES  "rules.txt"

/** Substituted in banner.txt line 1. */
#define HYBBX_BANNER_TOKEN_VERSION "@version@"
/** Substituted in banner.txt line 2. */
#define HYBBX_BANNER_TOKEN_SERVICE "@service@"
/** Substituted in motd.txt (current session nickname). */
#define HYBBX_TEXT_TOKEN_USERNAME "@username@"

#define HYBBX_TEXTS_PATH_MAX 512

typedef struct hybbx_texts_config {
    char path[HYBBX_TEXTS_PATH_MAX];
} hybbx_texts_config_t;

void hybbx_texts_config_defaults(hybbx_texts_config_t *texts);

/**
 * Build full path to a text file under the configured texts directory.
 * Returns HYBBX_OK on success.
 */
hybbx_result_t hybbx_texts_resolve(const hybbx_texts_config_t *texts,
                                   const char *filename,
                                   char *out, size_t out_len);

/**
 * Send a text file line-by-line to the session (connection output).
 * Missing files are skipped silently.
 */
hybbx_result_t hybbx_texts_send_file(const hybbx_texts_config_t *texts,
                                     struct hybbx_session *session,
                                     const char *filename);

/**
 * Send banner.txt with @version@ and @service@ tokens expanded.
 */
hybbx_result_t hybbx_texts_send_banner(const hybbx_texts_config_t *texts,
                                       struct hybbx_session *session,
                                       const char *version,
                                       const char *service_name);

/**
 * Send motd.txt with @username@ expanded from @p session.
 */
hybbx_result_t hybbx_texts_send_motd(const hybbx_texts_config_t *texts,
                                    struct hybbx_session *session);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_TEXTS_H */
