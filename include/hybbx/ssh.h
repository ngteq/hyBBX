#ifndef HYBBX_SSH_H
#define HYBBX_SSH_H

#include "hybbx/limits.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default HyBBX SSH TCP port (non-privileged; parallels telnet 2323). */
#define HYBBX_SSH_DEFAULT_PORT 3232u

#define HYBBX_SSH_BIND_V4_MAX 64
#define HYBBX_SSH_BIND_V6_MAX 64
#define HYBBX_SSH_HOSTKEY_DIR_MAX HYBBX_PATH_MAX

#define HYBBX_SSH_DEFAULT_BIND_V4 "0.0.0.0"
#define HYBBX_SSH_DEFAULT_BIND_V6 "::"
#define HYBBX_SSH_DEFAULT_HOSTKEY_DIR "keys"

#define HYBBX_SSH_HOSTKEY_ED25519 "ssh_host_ed25519_key"

typedef struct hybbx_ssh_config {
    char bind_v4[HYBBX_SSH_BIND_V4_MAX];
    char bind_v6[HYBBX_SSH_BIND_V6_MAX];
    char hostkey_dir[HYBBX_SSH_HOSTKEY_DIR_MAX];
    unsigned int port;
    int ipv4;
    int ipv6;
} hybbx_ssh_config_t;

void hybbx_ssh_config_defaults(hybbx_ssh_config_t *config);

/**
 * Parse a semicolon-separated key=value transport config string
 * (from transport.ssh INI section).
 */
hybbx_result_t hybbx_ssh_config_parse(const char *config,
                                      hybbx_ssh_config_t *out);

/**
 * Ensure Ed25519 host keys exist under @p keys_dir (resolved path).
 * Writes the private key path into @p hostkey_path.
 */
hybbx_result_t hybbx_ssh_keys_ensure(const char *keys_dir,
                                     char *hostkey_path,
                                     size_t hostkey_path_len);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_SSH_H */
