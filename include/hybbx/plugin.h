#ifndef HYBBX_PLUGIN_H
#define HYBBX_PLUGIN_H

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_service;
struct hybbx_session;

/**
 * Transport plugin interface (link adapters / host-client bridges).
 *
 * HyBBX is plugin-only: session core + plugins. Modems, TNCs, sound-card
 * software, ARDOPC, and CRDOPC are external — never embedded in HyBBX.
 *
 * Core uses TCP/IPv4+IPv6 and HBX internally. Plugins terminate the wire
 * to external services (telnet TCP, serial TNC, ARDOP host TCP, …) and
 * expose a byte stream to hybbx_session via the write callback.
 */
typedef struct hybbx_transport_plugin {
    const char *name;
    hybbx_transport_kind_t kind;
    unsigned int version;

    /** Called once when the plugin is loaded. */
    hybbx_result_t (*init)(struct hybbx_service *service);

    /** Called once when the plugin is unloaded. */
    void (*shutdown)(void);

    /**
     * Start listening / accepting connections on the given bind address.
     * @p config is a semicolon-separated key=value string built from the
     * transport's INI section (e.g. "bind=0.0.0.0;port=2323").
     */
    hybbx_result_t (*start)(const char *config);

    /** Stop accepting new connections; existing sessions may continue. */
    hybbx_result_t (*stop)(void);

    /**
     * Send raw bytes to the connected client for this session.
     * NULL → core writes to stdout (development fallback).
     */
    hybbx_result_t (*write)(struct hybbx_session *session,
                            const char *data, size_t len);

} hybbx_transport_plugin_t;

/**
 * Session callbacks invoked by a transport plugin when a client connects.
 */
typedef struct hybbx_session_ops {
    hybbx_result_t (*on_connect)(struct hybbx_session *session, void *userdata);
    hybbx_result_t (*on_data)(struct hybbx_session *session,
                              const uint8_t *data, size_t len,
                              void *userdata);
    void (*on_disconnect)(struct hybbx_session *session, void *userdata);
} hybbx_session_ops_t;

/** Opaque session handle passed between core and transport plugins. */
typedef struct hybbx_session {
    const hybbx_transport_plugin_t *transport;
    void *transport_data;
    void *core_data;
} hybbx_session_t;

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_PLUGIN_H */
