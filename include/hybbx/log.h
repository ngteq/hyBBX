#ifndef HYBBX_LOG_H
#define HYBBX_LOG_H

#include "hybbx/types.h"
#include "hybbx/limits.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_config;

/** File log severity (configured threshold filters lower-priority lines). */
typedef enum hybbx_log_level {
    HYBBX_LOG_DEBUG = 0,
    HYBBX_LOG_INFO = 1,
    HYBBX_LOG_STATS = 2,
    HYBBX_LOG_WARN = 3
} hybbx_log_level_t;

typedef struct hybbx_log_config {
    int enabled;
    char dir[HYBBX_PATH_MAX];
    hybbx_log_level_t level;
} hybbx_log_config_t;

void hybbx_log_config_defaults(hybbx_log_config_t *cfg);
void hybbx_log_config_apply(const struct hybbx_config *config);
const hybbx_log_config_t *hybbx_log_config_get(void);

/** Non-zero when file logging is active. */
int hybbx_log_enabled(void);

const char *hybbx_log_level_name(hybbx_log_level_t level);

void hybbx_log_write(hybbx_log_level_t level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

void hybbx_log_shutdown(void);

#define hybbx_log_debug(...) hybbx_log_write(HYBBX_LOG_DEBUG, __VA_ARGS__)
#define hybbx_log_info(...)  hybbx_log_write(HYBBX_LOG_INFO, __VA_ARGS__)
#define hybbx_log_stats(...) hybbx_log_write(HYBBX_LOG_STATS, __VA_ARGS__)
#define hybbx_log_warn(...)  hybbx_log_write(HYBBX_LOG_WARN, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_LOG_H */
