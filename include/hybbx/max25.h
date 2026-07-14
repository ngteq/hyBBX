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

/** Snapshot from max25d M25/1 STATUS (read-only; HyBBX never overrides). */
typedef struct hybbx_max25_status {
    int handshake_ok;
    char error[16];
    char voice[16];
    char stack[32];
    char serial[64];
} hybbx_max25_status_t;

void hybbx_max25_config_defaults(hybbx_max25_config_t *cfg);

/** Load `[max25]` from INI. `check` defaults to `yes`. Reporting is MAX25-only. */
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

void hybbx_max25_status_clear(hybbx_max25_status_t *status);

/**
 * TCP connect + M25/1 connect handshake (`OK` + `STATUS …`).
 * Returns @ref HYBBX_OK when max25d responds; @p status_out receives
 * `error=` / `voice=` / `stack=` / `serial=` as reported by MAX25 (valid or invalid).
 * Data-quality rules (3×20s passes, CALLID, min % good) live in max25d only —
 * HyBBX accepts STATUS without override; only TCP/handshake failure is fatal.
 */
hybbx_result_t hybbx_max25_probe(const hybbx_max25_config_t *cfg,
                                   hybbx_max25_status_t *status_out);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_MAX25_H */
