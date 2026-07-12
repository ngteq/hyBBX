#ifndef HYBBX_MAX25_H
#define HYBBX_MAX25_H

#include "hybbx/config.h"
#include "hybbx/limits.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default max25d TCP listen address. */
#define HYBBX_MAX25_DEFAULT_HOST "127.0.0.1"

typedef struct hybbx_max25_config {
    int check;
    char host[HYBBX_CONFIG_VALUE_MAX];
    unsigned port;
    unsigned timeout_ms;
} hybbx_max25_config_t;

void hybbx_max25_config_defaults(hybbx_max25_config_t *cfg);

/** Load `[max25]` from INI. `check` defaults to `yes`. */
void hybbx_max25_config_apply(hybbx_max25_config_t *cfg,
                              const hybbx_config_t *config);

/** Parse a semicolon key=value chunk (`max25_check=…` keys). */
void hybbx_max25_config_parse_kv(const char *max25_kv,
                                 hybbx_max25_config_t *cfg);

/**
 * When @p config starts with a max25 prefix (before
 * @ref HYBBX_PACKET_RADIO_INSTANCE_SEP), parse it and return the remainder.
 * When no prefix is present, @p cfg->check is cleared.
 */
const char *hybbx_max25_config_skip_prefix(const char *config,
                                           hybbx_max25_config_t *cfg);

/**
 * TCP connect probe to max25d (no M25/1 handshake). Returns @ref HYBBX_OK when
 * the port accepts a connection within @p cfg->timeout_ms.
 */
hybbx_result_t hybbx_max25_probe(const hybbx_max25_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_MAX25_H */
