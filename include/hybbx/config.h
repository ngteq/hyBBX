#ifndef HYBBX_CONFIG_H
#define HYBBX_CONFIG_H

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hybbx_config_entry {
    char *section;
    char *key;
    char *value;
} hybbx_config_entry_t;

typedef struct hybbx_config {
    hybbx_config_entry_t *entries;
    size_t count;
} hybbx_config_t;

/** Load configuration from an INI file. */
hybbx_result_t hybbx_config_load(hybbx_config_t *config, const char *path);

/** Release all memory held by @p config. */
void hybbx_config_free(hybbx_config_t *config);

/**
 * Return the value for @p key in @p section, or @p default_value if missing.
 * Both @p section and @p key are case-sensitive.
 */
const char *hybbx_config_get(const hybbx_config_t *config,
                             const char *section,
                             const char *key,
                             const char *default_value);

/** Parse HyBBX boolean tokens (yes/no and aliases; see hybbx_parse_bool). */
int hybbx_config_get_bool(const hybbx_config_t *config,
                          const char *section,
                          const char *key,
                          int default_value);

/**
 * Parse an unsigned decimal integer in [@p min_value, @p max_value].
 * Returns @p default_value when missing or invalid.
 */
unsigned hybbx_config_get_uint(const hybbx_config_t *config,
                               const char *section,
                               const char *key,
                               unsigned default_value,
                               unsigned min_value,
                               unsigned max_value);

/**
 * Build a semicolon-separated key=value string for all keys in @p section.
 * Caller must free the returned string.
 */
char *hybbx_config_format_section(const hybbx_config_t *config,
                                  const char *section);

/**
 * Resolve INI section for a transport plugin: @c transport.&lt;plugin&gt; first,
 * else the first @c transport.&lt;plugin&gt;&lt;digits&gt; section with keys
 * (e.g. @c transport.packet_radio1). Writes the chosen name to @p out_section.
 * Returns 1 when a numbered (or legacy) section with keys was found, else 0.
 */
int hybbx_config_resolve_transport_section(const hybbx_config_t *config,
                                           const char *plugin_name,
                                           char *out_section,
                                           size_t out_size);

typedef void (*hybbx_config_iter_fn)(const char *section, const char *key,
                                     const char *value, void *ctx);

/** Invoke @p fn for every entry in @p config (in file order). */
void hybbx_config_foreach(const hybbx_config_t *config,
                          hybbx_config_iter_fn fn, void *ctx);

/** Set or replace a key in @p config (in-memory only until saved). */
hybbx_result_t hybbx_config_set(hybbx_config_t *config,
                               const char *section,
                               const char *key,
                               const char *value);

/** Remove all keys in @p section from @p config. */
void hybbx_config_remove_section(hybbx_config_t *config, const char *section);

/** Write @p config to @p path (INI format). */
hybbx_result_t hybbx_config_save(const hybbx_config_t *config,
                                 const char *path);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_CONFIG_H */
