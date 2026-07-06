#include "hybbx/websocket.h"
#include "hybbx/util.h"

#include <stdlib.h>
#include <string.h>

static unsigned int parse_port(const char *value)
{
    char *end;
    unsigned long port;

    if (value == NULL || value[0] == '\0') {
        return HYBBX_WEBSOCKET_DEFAULT_PORT;
    }

    port = strtoul(value, &end, 10);
    if (end == value || *end != '\0' || port == 0 || port > 65535u) {
        return HYBBX_WEBSOCKET_DEFAULT_PORT;
    }

    return (unsigned int)port;
}

static unsigned int parse_max_connections(const char *value)
{
    char *end;
    unsigned long n;

    if (value == NULL || value[0] == '\0') {
        return HYBBX_WEBSOCKET_DEFAULT_MAX_CONNECTIONS;
    }

    n = strtoul(value, &end, 10);
    if (end == value || *end != '\0' || n == 0 || n > 65535u) {
        return HYBBX_WEBSOCKET_DEFAULT_MAX_CONNECTIONS;
    }

    return (unsigned int)n;
}

static const char *find_kv(const char *config, const char *key,
                           char *scratch, size_t scratch_len)
{
    const char *cursor = config;
    size_t key_len = strlen(key);

    if (config == NULL || key == NULL) {
        return NULL;
    }

    while (*cursor != '\0') {
        const char *sep = strchr(cursor, ';');
        const char *end = sep != NULL ? sep : cursor + strlen(cursor);
        const char *eq = strchr(cursor, '=');

        if (eq != NULL && eq < end && (size_t)(eq - cursor) == key_len &&
            strncmp(cursor, key, key_len) == 0) {
            const char *value = eq + 1;
            size_t value_len = (size_t)(end - value);

            if (scratch != NULL && scratch_len > 0) {
                if (value_len >= scratch_len) {
                    value_len = scratch_len - 1;
                }
                memcpy(scratch, value, value_len);
                scratch[value_len] = '\0';
                return scratch;
            }

            return value;
        }

        if (sep == NULL) {
            break;
        }
        cursor = sep + 1;
    }

    return NULL;
}

void hybbx_websocket_config_defaults(hybbx_websocket_config_t *config)
{
    if (config == NULL) {
        return;
    }

    memset(config, 0, sizeof(*config));
    hybbx_strlcpy(config->bind_v4, HYBBX_WEBSOCKET_DEFAULT_BIND_V4,
                  sizeof(config->bind_v4));
    hybbx_strlcpy(config->bind_v6, HYBBX_WEBSOCKET_DEFAULT_BIND_V6,
                  sizeof(config->bind_v6));
    hybbx_strlcpy(config->path, HYBBX_WEBSOCKET_DEFAULT_PATH,
                  sizeof(config->path));
    hybbx_strlcpy(config->cert_dir, HYBBX_WEBSOCKET_DEFAULT_CERT_DIR,
                  sizeof(config->cert_dir));
    config->port = HYBBX_WEBSOCKET_DEFAULT_PORT;
    config->max_connections = HYBBX_WEBSOCKET_DEFAULT_MAX_CONNECTIONS;
    config->ipv4 = 1;
    config->ipv6 = 1;
}

hybbx_result_t hybbx_websocket_config_parse(const char *config,
                                            hybbx_websocket_config_t *out)
{
    char scratch[HYBBX_WEBSOCKET_PATH_MAX];
    const char *value;

    if (out == NULL) {
        return HYBBX_ERR_INVALID;
    }

    hybbx_websocket_config_defaults(out);

    if (config == NULL || config[0] == '\0') {
        return HYBBX_OK;
    }

    value = find_kv(config, "port", scratch, sizeof(scratch));
    out->port = parse_port(value);

    value = find_kv(config, "max_connections", scratch, sizeof(scratch));
    if (value == NULL) {
        value = find_kv(config, "max_online", scratch, sizeof(scratch));
    }
    out->max_connections = parse_max_connections(value);

    value = find_kv(config, "bind", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        if (strchr(value, ':') != NULL) {
            hybbx_strlcpy(out->bind_v6, value, sizeof(out->bind_v6));
        } else {
            hybbx_strlcpy(out->bind_v4, value, sizeof(out->bind_v4));
        }
    }

    value = find_kv(config, "bind6", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        hybbx_strlcpy(out->bind_v6, value, sizeof(out->bind_v6));
    }

    value = find_kv(config, "path", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        hybbx_strlcpy(out->path, value, sizeof(out->path));
    }

    value = find_kv(config, "cert_dir", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        hybbx_strlcpy(out->cert_dir, value, sizeof(out->cert_dir));
    }

    value = find_kv(config, "ipv4", scratch, sizeof(scratch));
    if (value != NULL) {
        out->ipv4 = hybbx_parse_bool(value, 1);
    }

    value = find_kv(config, "ipv6", scratch, sizeof(scratch));
    if (value != NULL) {
        out->ipv6 = hybbx_parse_bool(value, 1);
    }

    return HYBBX_OK;
}
