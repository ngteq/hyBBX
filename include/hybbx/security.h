#ifndef HYBBX_SECURITY_H
#define HYBBX_SECURITY_H

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_config;

/** Log file name under the configured logs directory. */
#define HYBBX_SECURITY_LOG_FILE "security.log"

void hybbx_security_log_config_apply(const struct hybbx_config *config);
void hybbx_security_log_shutdown(void);

/** Append one line to security.log (timestamp added). No-op when not configured. */
void hybbx_security_log_write(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_SECURITY_H */
