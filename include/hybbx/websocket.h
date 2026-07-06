#ifndef HYBBX_WEBSOCKET_H
#define HYBBX_WEBSOCKET_H

#include "hybbx/limits.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default WebSocket listen port (behind TLS reverse proxy). */
#define HYBBX_WEBSOCKET_DEFAULT_PORT 591u

#define HYBBX_WEBSOCKET_BIND_V4_MAX 64
#define HYBBX_WEBSOCKET_BIND_V6_MAX 64
#define HYBBX_WEBSOCKET_PATH_MAX 128

#define HYBBX_WEBSOCKET_DEFAULT_BIND_V4 "127.0.0.1"
#define HYBBX_WEBSOCKET_DEFAULT_BIND_V6 "::1"
#define HYBBX_WEBSOCKET_DEFAULT_PATH "/hybbx"
#define HYBBX_WEBSOCKET_DEFAULT_CERT_DIR "keys"

/** Filenames inside @c cert_dir (auto-generated on first start when TLS). */
#define HYBBX_WS_TLS_CERT_FILENAME "hybbx_ws.crt"
#define HYBBX_WS_TLS_KEY_FILENAME "hybbx_ws.key"

/** Self-signed WebSocket TLS certificate validity (days). */
#define HYBBX_WS_TLS_CERT_VALID_DAYS 1825u

typedef struct hybbx_websocket_config {
    char bind_v4[HYBBX_WEBSOCKET_BIND_V4_MAX];
    char bind_v6[HYBBX_WEBSOCKET_BIND_V6_MAX];
    char path[HYBBX_WEBSOCKET_PATH_MAX];
    char cert_dir[HYBBX_PATH_MAX];
    unsigned int port;
    int ipv4;
    int ipv6;
} hybbx_websocket_config_t;

void hybbx_websocket_config_defaults(hybbx_websocket_config_t *config);

hybbx_result_t hybbx_websocket_config_parse(const char *config,
                                            hybbx_websocket_config_t *out);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_WEBSOCKET_H */
