#ifndef HYBBX_PASSWORD_H
#define HYBBX_PASSWORD_H

#include "hybbx/types.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Stored password prefix for SHA-256 (default, tinysha256). */
#define HYBBX_PASSWORD_SHA256_PREFIX "{sha256}"
/** Stored password prefix for legacy MD5 (verify-only fallback). */
#define HYBBX_PASSWORD_MD5_PREFIX "{md5}"

/** Max stored password field: prefix + 64 hex digits + NUL. */
#define HYBBX_PASSWORD_STORED_MAX 96

/** Non-zero when @p stored is a hashed password (sha256 or md5 prefix). */
int hybbx_password_is_hashed(const char *stored);

/** Non-zero when @p stored is plain text and should be upgraded on load. */
int hybbx_password_is_plain(const char *stored);

/**
 * Hash @p plain into @p out as {sha256}<hex> using the configured backend
 * (default: bundled tinysha256; see [crypto] password_hash in hybbx.ini).
 */
hybbx_result_t hybbx_password_hash_sha256(const char *plain,
                                          char *out,
                                          size_t out_size);

/** Hash @p plain with the configured default algorithm (SHA-256). */
hybbx_result_t hybbx_password_hash(const char *plain, char *out, size_t out_size);

/** Return non-zero when @p provided matches @p stored (plain, sha256, or md5). */
int hybbx_password_match(const char *stored, const char *provided);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_PASSWORD_H */
