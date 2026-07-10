#ifndef HYBBX_SERVICE_H
#define HYBBX_SERVICE_H

/**
 * Centralized HyBBX daemon API (`hybbx` binary).
 *
 * Loads INI ([service], [storage], [auth], [transport.*], [circuit]), starts
 * transport plugins and the HBX circuit hub. Edge link/repeater daemons attach
 * to [circuit]; see share/hybbx.ini.example and docs/TOPOLOGY.md.
 */

#include "hybbx/config.h"
#include "hybbx/plugin.h"
#include "hybbx/auth.h"
#include "hybbx/storage.h"
#include "hybbx/texts.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default global service name (`[service] name` in INI). */
#define HYBBX_DEFAULT_SERVICE_NAME "hyBBX"

typedef struct hybbx_service {
    const char *name;
    void *userdata;
} hybbx_service_t;

/** Create the main HyBBX service instance. */
hybbx_service_t *hybbx_service_create(const char *name);

/** Destroy the service and shut down all active transports. */
void hybbx_service_destroy(hybbx_service_t *service);

/**
 * Apply settings from an INI configuration (service name, enabled transports).
 */
hybbx_result_t hybbx_service_apply_config(hybbx_service_t *service,
                                          const hybbx_config_t *config,
                                          const char *config_path);

/**
 * Load a transport plugin by name (e.g. "telnet", "packet_radio").
 * Built-in plugins are linked statically; dynamic loading comes later.
 */
hybbx_result_t hybbx_service_load_transport(hybbx_service_t *service,
                                            const char *plugin_name);

/** Start a loaded transport with transport-specific configuration. */
hybbx_result_t hybbx_service_start_transport(hybbx_service_t *service,
                                               const char *plugin_name,
                                               const char *config);

/** Stop a running transport. */
hybbx_result_t hybbx_service_stop_transport(hybbx_service_t *service,
                                            const char *plugin_name);

/** Run the main event loop until hybbx_service_stop() is called. */
hybbx_result_t hybbx_service_run(hybbx_service_t *service);

/** Request the main loop to exit. */
void hybbx_service_stop(hybbx_service_t *service);

typedef enum hybbx_shutdown_mode {
    HYBBX_SHUTDOWN_NONE = 0,
    HYBBX_SHUTDOWN_STOP = 1,
    HYBBX_SHUTDOWN_RESTART = 2
} hybbx_shutdown_mode_t;

/** Store the daemon binary path used for /restart (typically argv[0]). */
void hybbx_service_set_launch_binary(hybbx_service_t *service, const char *argv0);

/** Sysop /shutdown or /restart — stops the main loop; restart re-execs after exit. */
void hybbx_service_request_shutdown(hybbx_service_t *service, int restart);

/** Shutdown mode requested while the service is still running. */
hybbx_shutdown_mode_t hybbx_service_shutdown_mode(const hybbx_service_t *service);

/**
 * Re-exec the daemon when @ref hybbx_service_shutdown_mode is
 * @ref HYBBX_SHUTDOWN_RESTART. Call before @ref hybbx_service_destroy.
 * Does not return on success.
 */
void hybbx_service_restart_exec(const hybbx_service_t *service);

const char *hybbx_service_config_path(const hybbx_service_t *service);

hybbx_storage_t *hybbx_service_get_storage(hybbx_service_t *service);
const hybbx_auth_config_t *hybbx_service_get_auth(hybbx_service_t *service);
const hybbx_texts_config_t *hybbx_service_get_texts(hybbx_service_t *service);

/**
 * Global input prompt shown to every connected user.
 * Empty string when unset (default): no visible prompt before input.
 */
const char *hybbx_service_get_prompt(const hybbx_service_t *service);

/** Global service name from configuration (default @ref HYBBX_DEFAULT_SERVICE_NAME). */
const char *hybbx_service_get_name(const hybbx_service_t *service);

/** Configured maximum simultaneous online sessions (default @ref HYBBX_DEFAULT_MAX_ONLINE). */
unsigned hybbx_service_max_online(const hybbx_service_t *service);

/** Currently connected sessions (all transports). */
unsigned hybbx_service_active_nodes(const hybbx_service_t *service);

/** Guest auto-disconnect timeout in seconds (from INI minutes setting). */
unsigned hybbx_service_guest_timeout_seconds(const hybbx_service_t *service);

struct hybbx_user_record;

/**
 * Assign the lowest free ephemeral guest slot (Guest1 … Guest25).
 * Guests are not written to user files.
 */
hybbx_result_t hybbx_service_guest_assign(hybbx_service_t *service,
                                          const char *guest_prefix,
                                          struct hybbx_user_record *out,
                                          unsigned *slot_out);

/** Release a guest slot when the session ends or leaves guest mode. */
void hybbx_service_guest_release(hybbx_service_t *service, unsigned slot);

/**
 * Reserve one node slot for a new connection.
 * @return HYBBX_ERR_BUSY when @ref hybbx_service_max_online is reached.
 */
hybbx_result_t hybbx_service_acquire_node(hybbx_service_t *service);

/** Release a node slot (pair with @ref hybbx_service_acquire_node). */
void hybbx_service_release_node(hybbx_service_t *service);

struct hybbx_session;

/** Track an active connection for chat broadcast and similar features. */
hybbx_result_t hybbx_service_attach_session(hybbx_service_t *service,
                                            struct hybbx_session *session);

/** Remove a session from the active connection list. */
void hybbx_service_detach_session(hybbx_service_t *service,
                                  struct hybbx_session *session);

typedef void (*hybbx_service_session_visit_fn)(struct hybbx_session *session,
                                               void *userdata);

/** Invoke @p fn for each attached session (holds an internal lock). */
void hybbx_service_visit_sessions(hybbx_service_t *service,
                                  hybbx_service_session_visit_fn fn,
                                  void *userdata);

/**
 * Return a logged-in registered session for @p user_id on another connection,
 * or NULL when the account is not online elsewhere.
 */
struct hybbx_session *hybbx_service_find_registered_session(
    hybbx_service_t *service,
    uint64_t user_id,
    struct hybbx_session *exclude);

struct hybbx_circuit_hub;
struct hybbx_circuit_hub *hybbx_service_circuit_hub(hybbx_service_t *service);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_SERVICE_H */
