#if defined(__linux__) || defined(__GLIBC__)
#define _DEFAULT_SOURCE 1
#endif

#include "hybbx/max25.h"
#include "hybbx/log.h"
#include "hybbx/util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32) && !defined(__AMIGA__)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

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

            if (value_len >= scratch_len) {
                value_len = scratch_len - 1;
            }
            memcpy(scratch, value, value_len);
            scratch[value_len] = '\0';
            return scratch;
        }

        if (sep == NULL) {
            break;
        }
        cursor = sep + 1;
    }

    return NULL;
}

static const char *find_prefixed_kv(const char *config, const char *key,
                                    char *scratch, size_t scratch_len)
{
    char prefixed[HYBBX_CONFIG_KEY_MAX + 8];

    snprintf(prefixed, sizeof(prefixed), "max25_%s", key);
    return find_kv(config, prefixed, scratch, scratch_len);
}

void hybbx_max25_config_defaults(hybbx_max25_config_t *cfg)
{
    if (cfg == NULL) {
        return;
    }

    memset(cfg, 0, sizeof(*cfg));
    hybbx_strlcpy(cfg->host, HYBBX_MAX25_DEFAULT_HOST, sizeof(cfg->host));
    cfg->port = HYBBX_MAX25_DEFAULT_PORT;
    cfg->timeout_ms = HYBBX_MAX25_PROBE_TIMEOUT_MS;
}

void hybbx_max25_config_apply(hybbx_max25_config_t *cfg,
                              const hybbx_config_t *config)
{
    const char *host;

    hybbx_max25_config_defaults(cfg);
    if (cfg == NULL || config == NULL) {
        return;
    }

    cfg->check = hybbx_config_get_bool(config, "max25", "check", 1);
    host = hybbx_config_get(config, "max25", "host", HYBBX_MAX25_DEFAULT_HOST);
    hybbx_strlcpy(cfg->host, host, sizeof(cfg->host));
    cfg->port = hybbx_config_get_uint(config, "max25", "port",
                                      HYBBX_MAX25_DEFAULT_PORT, 1u, 65535u);
    cfg->timeout_ms = hybbx_config_get_uint(config, "max25", "timeout_ms",
                                            HYBBX_MAX25_PROBE_TIMEOUT_MS,
                                            100u, 60000u);
}

void hybbx_max25_config_parse_kv(const char *max25_kv,
                                 hybbx_max25_config_t *cfg)
{
    char scratch[HYBBX_CONFIG_VALUE_MAX];
    const char *value;
    unsigned long n;
    char *end;

    if (cfg == NULL) {
        return;
    }

    hybbx_max25_config_defaults(cfg);
    if (max25_kv == NULL || max25_kv[0] == '\0') {
        cfg->check = 0;
        return;
    }

    value = find_prefixed_kv(max25_kv, "check", scratch, sizeof(scratch));
    if (value != NULL) {
        cfg->check = hybbx_parse_bool(value, 1);
    } else {
        cfg->check = 0;
    }

    value = find_prefixed_kv(max25_kv, "host", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        hybbx_strlcpy(cfg->host, value, sizeof(cfg->host));
    }

    value = find_prefixed_kv(max25_kv, "port", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        n = strtoul(value, &end, 10);
        if (end != value && n > 0 && n <= 65535u) {
            cfg->port = (unsigned)n;
        }
    }

    value = find_prefixed_kv(max25_kv, "timeout_ms", scratch, sizeof(scratch));
    if (value != NULL && value[0] != '\0') {
        n = strtoul(value, &end, 10);
        if (end != value && n >= 100 && n <= 60000) {
            cfg->timeout_ms = (unsigned)n;
        }
    }
}

const char *hybbx_max25_config_skip_prefix(const char *config,
                                           hybbx_max25_config_t *cfg)
{
    char scratch[512];
    const char *sep;
    size_t len;

    if (config == NULL || cfg == NULL) {
        return config;
    }

    sep = strchr(config, HYBBX_PACKET_RADIO_INSTANCE_SEP);
    len = sep != NULL ? (size_t)(sep - config) : strlen(config);
    if (len == 0 || len >= sizeof(scratch)) {
        hybbx_max25_config_defaults(cfg);
        cfg->check = 0;
        return config;
    }

    memcpy(scratch, config, len);
    scratch[len] = '\0';

    if (find_prefixed_kv(scratch, "check", scratch, sizeof(scratch)) == NULL) {
        hybbx_max25_config_defaults(cfg);
        cfg->check = 0;
        return config;
    }

    hybbx_max25_config_parse_kv(scratch, cfg);
    return sep != NULL ? sep + 1 : config + len;
}

#if !defined(_WIN32) && !defined(__AMIGA__)

static hybbx_result_t max25_tcp_probe_host(const char *host, unsigned port,
                                           unsigned timeout_ms)
{
    struct sockaddr_in6 addr6;
    struct sockaddr_in addr4;
    struct pollfd pfd;
    const struct sockaddr *addr;
    socklen_t addr_len;
    int fd;
    int flags;
    int rc;
    int so_error = 0;
    socklen_t so_len = sizeof(so_error);

    if (host == NULL || host[0] == '\0' || port == 0u) {
        return HYBBX_ERR_INVALID;
    }

    if (strchr(host, ':') != NULL) {
        memset(&addr6, 0, sizeof(addr6));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons((uint16_t)port);
        if (inet_pton(AF_INET6, host, &addr6.sin6_addr) != 1) {
            return HYBBX_ERR_INVALID;
        }
        fd = socket(AF_INET6, SOCK_STREAM, 0);
        addr = (const struct sockaddr *)&addr6;
        addr_len = sizeof(addr6);
    } else {
        memset(&addr4, 0, sizeof(addr4));
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons((uint16_t)port);
        if (inet_pton(AF_INET, host, &addr4.sin_addr) != 1) {
            return HYBBX_ERR_INVALID;
        }
        fd = socket(AF_INET, SOCK_STREAM, 0);
        addr = (const struct sockaddr *)&addr4;
        addr_len = sizeof(addr4);
    }

    if (fd < 0) {
        return HYBBX_ERR_IO;
    }

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        close(fd);
        return HYBBX_ERR_IO;
    }

    rc = connect(fd, addr, addr_len);
    if (rc == 0) {
        close(fd);
        return HYBBX_OK;
    }
    if (errno != EINPROGRESS) {
        close(fd);
        return HYBBX_ERR_IO;
    }

    pfd.fd = fd;
    pfd.events = POLLOUT;
    pfd.revents = 0;

    rc = poll(&pfd, 1, (int)timeout_ms);
    if (rc <= 0) {
        close(fd);
        return HYBBX_ERR_IO;
    }

    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &so_len) != 0 ||
        so_error != 0) {
        close(fd);
        return HYBBX_ERR_IO;
    }

    close(fd);
    return HYBBX_OK;
}

#endif /* !WIN32 && !AMIGA */

hybbx_result_t hybbx_max25_probe(const hybbx_max25_config_t *cfg)
{
#if defined(_WIN32) || defined(__AMIGA__)
    (void)cfg;
    return HYBBX_OK;
#else
    hybbx_result_t rc;

    if (cfg == NULL || !cfg->check) {
        return HYBBX_OK;
    }

    if (cfg->host[0] == '\0' || cfg->port == 0u) {
        return HYBBX_ERR_INVALID;
    }

    rc = max25_tcp_probe_host(cfg->host, cfg->port, cfg->timeout_ms);
    if (rc != HYBBX_OK) {
        hybbx_log_warn("[max25] max25d unreachable at %s:%u (timeout %u ms)",
                       cfg->host, cfg->port, cfg->timeout_ms);
    }

    return rc;
#endif
}
