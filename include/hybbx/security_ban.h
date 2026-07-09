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
 * Normalize @p in into @p out (uppercase CALL/CALL-SSID or link_id).
 * Returns non-zero when valid.
 */
int hybbx_security_callid_normalize(const char *in, char *out, size_t out_cap);

/** Non-zero when @p callid is banned (AX.25 callsign or HBX link_id). */
int hybbx_security_ban_callid_is_banned(const char *callid);

/**
 * Accept gate for HBX link_id / AX.25 source before circuit or RF uplink.
 * Returns non-zero when allowed.
 */
int hybbx_security_ban_callid_accept(const char *callid);

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

/** Record a failed HBX circuit link authentication (IP). */
void hybbx_security_ban_link_auth_fail(const char *ip);

/** Record a failed HBX circuit link authentication (link_id / CALLID). */
void hybbx_security_ban_link_auth_fail_callid(const char *callid);

/**
 * Record excessive abuse (chat/mail flood, register spam, …).
 * Uses @c abuse_maxretry / @c abuse_findtime — not login @c maxretry.
 * Normal traffic limits ([chat], [mail], [traffic]) never call this.
 */
void hybbx_security_ban_abuse_report(const char *ip, const char *category);

/** Same as @ref hybbx_security_ban_abuse_report for @p callid. */
void hybbx_security_ban_callid_abuse_report(const char *callid,
                                            const char *category);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_SECURITY_BAN_H */
