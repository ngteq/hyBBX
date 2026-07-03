#ifndef HYBBX_BANDWIDTH_POLICY_H
#define HYBBX_BANDWIDTH_POLICY_H

/**
 * Per-user bandwidth limits when a low-bandwidth circuit link is under pressure.
 * Circuit (secondary) sessions are never targeted. Among users, AX.25 sessions
 * are sacrificed before full-duplex TCP/telnet; within each class, newest first.
 */

#include "hybbx/circuit_balance.h"
#include "hybbx/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HYBBX_BANDWIDTH_DISCONNECT_MSG "You are disconnected by bandwidth limits."

struct hybbx_service;

/** Logged-in telnet and packet_radio users subject to limits (not circuit links). */
unsigned hybbx_bandwidth_policy_user_count(struct hybbx_service *service);

/**
 * Apply pause / break / cancel / resume to online users (AX.25 before TCP, then LIFO).
 * Circuit (secondary) sessions are never targeted. Returns users affected.
 */
unsigned hybbx_bandwidth_policy_apply(struct hybbx_service *service,
                                      hybbx_circuit_balance_action_t action);

#ifdef __cplusplus
}
#endif

#endif /* HYBBX_BANDWIDTH_POLICY_H */
