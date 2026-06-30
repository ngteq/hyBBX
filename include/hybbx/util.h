#ifndef HYBBX_UTIL_H
#define HYBBX_UTIL_H

#include "hybbx/types.h"

#include <stddef.h>

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

/** Return non-zero when @p len is a safe allocation size. */
int hybbx_size_ok(size_t len);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_UTIL_H */
