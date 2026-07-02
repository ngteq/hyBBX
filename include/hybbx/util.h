#ifndef HYBBX_UTIL_H
#define HYBBX_UTIL_H

#include "hybbx/types.h"

#include <stddef.h>

struct tm;

struct hybbx_config;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Safe string copy (always NUL-terminates when @p dst_size > 0).
 * Returns bytes copied excluding the terminator.
 */
size_t hybbx_strlcpy(char *dst, const char *src, size_t dst_size);

/**
 * Join @p base and @p name into @p out with a single '/'.
 * Rejects empty components and path traversal ("..").
 */
hybbx_result_t hybbx_path_join(char *out, size_t out_len,
                               const char *base, const char *name);

/** Expand `~` / `~/…` and return the default user data directory `$HOME/.hybbx`. */
hybbx_result_t hybbx_default_user_data_path(char *out, size_t out_len);

/**
 * Expand a config path: `~` → $HOME, `~/foo` → $HOME/foo.
 * Other paths are copied unchanged.
 */
hybbx_result_t hybbx_path_expand(char *out, size_t out_len, const char *path);

/** Return non-zero when @p len is safe for HyBBX allocations. */
int hybbx_size_ok(size_t len);

/**
 * HyBBX boolean configuration standard.
 * Canonical written form: @c yes / @c no (see @ref HYBBX_BOOL_YES / @ref HYBBX_BOOL_NO).
 * Accepted true:  yes, true, enable, enabled, on, 1
 * Accepted false: no, false, disable, disabled, off, 0
 * Matching is case-insensitive.
 */
#define HYBBX_BOOL_YES "yes"
#define HYBBX_BOOL_NO  "no"

/** Return non-zero when @p value is a recognized true token. */
int hybbx_bool_is_true(const char *value);

/** Return non-zero when @p value is a recognized false token. */
int hybbx_bool_is_false(const char *value);

/**
 * Parse a boolean string. Returns 1 or 0 when recognized; otherwise
 * returns @p default_value (also used when @p value is NULL or empty).
 */
int hybbx_parse_bool(const char *value, int default_value);

/** Canonical @c yes / @c no string for a boolean value. */
const char *hybbx_bool_to_string(int value);

/** Short name for @p rc (logging / tests). */
const char *hybbx_result_name(hybbx_result_t rc);

/**
 * HyBBX system-local date/time stamp (yyyymmdd + time).
 * Default: 24-hour clock, minutes only (no seconds).
 */
typedef struct hybbx_time_format {
    int clock_12h;
    int seconds;
} hybbx_time_format_t;

void hybbx_time_format_defaults(hybbx_time_format_t *fmt);
const hybbx_time_format_t *hybbx_time_format_get(void);
void hybbx_time_config_apply(const struct hybbx_config *config);

/**
 * Format @p tm as @c yyyymmdd HH:MM per @p fmt.
 * @return HYBBX_OK on success.
 */
hybbx_result_t hybbx_time_format_stamp(char *out, size_t out_len,
                                       const struct tm *tm,
                                       const hybbx_time_format_t *fmt);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_UTIL_H */
