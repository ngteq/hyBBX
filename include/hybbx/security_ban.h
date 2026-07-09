#ifndef HYBBX_SECURITY_BAN_H
#define HYBBX_SECURITY_BAN_H

#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hybbx_config;

typedef enum hybbx_ban_backend {
    HYBBX_BAN_BACKEND_INTERNAL = 0,
    HYBBX_BAN_BACKEND_LOG,
    HYBBX_BAN_BACKEND_IPTABLES,
    HYBBX_BAN_BACKEND_NFTABLES,
    HYBBX_BAN_BACKEND_HOSTS
} hybbx_ban_backend_t;

void hybbx_security_ban_config_apply(const struct hybbx_config *config);
void hybbx_security_ban_shutdown(void);

/** Expire bans and prune stale failure/rate records (service loop). */
void hybbx_security_ban_tick(void);

/** Non-zero when @p ip is currently banned. */
int hybbx_security_ban_is_banned(const char *ip);

/**
 * Accept gate: rate-limit check + ban check.
 * Returns non-zero when the connection may proceed.
 */
int hybbx_security_ban_accept(const char *ip);

/**
 * Same as @ref hybbx_security_ban_accept using the peer address of @p fd.
 * Allows the connection when the peer address cannot be resolved.
 */
int hybbx_security_ban_accept_fd(int fd);

/** Record a failed login for @p transport (telnet, ssh, websocket, …). */
void hybbx_security_ban_login_fail(const char *ip, const char *transport);

/** Record a failed HBX circuit link authentication. */
void hybbx_security_ban_link_auth_fail(const char *ip);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_SECURITY_BAN_H */
