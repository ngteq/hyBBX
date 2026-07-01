#ifndef HYBBX_LINK_H
#define HYBBX_LINK_H

/**
 * HyBBX edge link registry (gateway / digipeater / repeater / link).
 *
 * Links are authenticated by password only — no TCP/IP-style ping/pong or
 * heartbeat health checks across networks or protocols.
 * Stale entries (no successful password auth within @ref HYBBX_LINK_STALE_DAYS)
 * are removed from the registry and from hybbx.ini @c [link.*] sections.
 */

#include "hybbx/types.h"
#include "hybbx/limits.h"

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_LINK_STALE_DAYS       10u
#define HYBBX_LINK_ID_MAX           64
#define HYBBX_LINK_ROLE_MAX         32
#define HYBBX_LINK_CODE_MAX         16
#define HYBBX_LINK_AUTH_PAYLOAD_MAX 512

typedef struct hybbx_link_auth {
    char password[128];
    char role[HYBBX_LINK_ROLE_MAX];
    char id[HYBBX_LINK_ID_MAX];
} hybbx_link_auth_t;

typedef struct hybbx_link_registry {
    char dir[HYBBX_PATH_MAX];
    char config_path[HYBBX_PATH_MAX];
    unsigned stale_days;
} hybbx_link_registry_t;

void hybbx_link_auth_clear(hybbx_link_auth_t *auth);

/**
 * Build line-oriented LINK_AUTH payload (password=, role=, id=).
 * @return payload length or 0 on error.
 */
size_t hybbx_link_auth_format(const hybbx_link_auth_t *auth,
                              char *out, size_t out_cap);

/** Parse LINK_AUTH payload into @p auth. Returns HYBBX_OK on success. */
hybbx_result_t hybbx_link_auth_parse(const char *payload, size_t len,
                                     hybbx_link_auth_t *auth);

void hybbx_link_registry_init(hybbx_link_registry_t *reg,
                              const char *data_dir,
                              const char *config_path,
                              unsigned stale_days);

/**
 * Record successful password authentication for @p id.
 * Creates or updates @c data/links/<id>.ini and @c [link.<id>] in hybbx.ini.
 */
hybbx_result_t hybbx_link_registry_touch(hybbx_link_registry_t *reg,
                                         const char *id,
                                         const char *role,
                                         char *link_code_out,
                                         size_t link_code_cap);

/**
 * Remove link entries with @c last_seen older than @p stale_days.
 * Also drops matching @c [link.*] sections from hybbx.ini when @p config_path set.
 */
hybbx_result_t hybbx_link_registry_prune(hybbx_link_registry_t *reg);

/** Generate a short link code (uppercase alphanumeric). */
void hybbx_link_generate_code(char *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_LINK_H */
