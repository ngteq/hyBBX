#ifndef HYBBX_TELNET_H
#define HYBBX_TELNET_H

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default HyBBX telnet TCP port (BBX / terminal convention). */
#define HYBBX_TELNET_DEFAULT_PORT 2323u

#define HYBBX_TELNET_BIND_V4_MAX 64
#define HYBBX_TELNET_BIND_V6_MAX 64

#define HYBBX_TELNET_DEFAULT_BIND_V4 "0.0.0.0"
#define HYBBX_TELNET_DEFAULT_BIND_V6 "::"

typedef struct hybbx_telnet_config {
    char bind_v4[HYBBX_TELNET_BIND_V4_MAX];
    char bind_v6[HYBBX_TELNET_BIND_V6_MAX];
    unsigned int port;
    int ipv4;
    int ipv6;
} hybbx_telnet_config_t;

void hybbx_telnet_config_defaults(hybbx_telnet_config_t *config);

/**
 * Parse a semicolon-separated key=value transport config string
 * (from transport.telnet INI section).
 */
hybbx_result_t hybbx_telnet_config_parse(const char *config,
                                         hybbx_telnet_config_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_TELNET_H */
