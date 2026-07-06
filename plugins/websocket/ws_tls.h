#ifndef HYBBX_WS_TLS_H
#define HYBBX_WS_TLS_H

#include "hybbx/types.h"

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Return 1 when this build links OpenSSL for WebSocket TLS. */
int hybbx_ws_tls_compiled(void);

/**
 * Create @p cert_dir if needed and generate a self-signed certificate on first
 * start when files are missing. No-op when OpenSSL is not compiled in.
 */
hybbx_result_t hybbx_ws_tls_ensure_certs(const char *cert_dir);

/** Load server TLS context after @p cert_dir is ready. */
hybbx_result_t hybbx_ws_tls_server_start(const char *cert_dir);

void hybbx_ws_tls_server_stop(void);

/** Return 1 when the server accepts TLS (OpenSSL + certs loaded). */
int hybbx_ws_tls_server_enabled(void);

/** Perform TLS server handshake on @p fd; returns opaque SSL* or NULL. */
void *hybbx_ws_tls_accept(int fd);

ssize_t hybbx_ws_tls_recv(void *tls, void *buf, size_t len);
ssize_t hybbx_ws_tls_send(void *tls, const void *buf, size_t len);

void hybbx_ws_tls_shutdown(void *tls);
void hybbx_ws_tls_free(void *tls);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_WS_TLS_H */
