# Security and spam policy

**v2.0.0** — built-in `[security]` in core (`security.log`, `security_ban.c`). No separate spam plugin.

HyBBX treats **network security** and **spam control** as one subsystem: same `[security]` section, same `security.log`, same short cool-down bans.

## Two layers

| Layer | What | Ban? |
|-------|------|------|
| **Soft limits** | Normal use, RF pacing, message size | **No** — drop, pace, or reject single actions |
| **Abuse** | Brute-force, connection flood, *excessive* spam | **Yes** — short IP ban (`bantime`, default 10 min) |

Normal chat, mail, or guest activity must never trigger a ban. Only repeated **abuse** crosses into `[security]`.

## Soft limits (no bans)

Configured outside `[security]`; they shape traffic on slow links:

| Area | Keys | Effect |
|------|------|--------|
| `[traffic]` | `baud`, `pace_output`, `line_width` | Output pacing |
| `[chat]` | `message_max` | Truncate/deny oversized lines |
| `[mail]` | `max_messages`, `body_max`, `subject_max` | Mailbox caps |
| Conference | *(fixed)* | Max 2 invites per target / 30 min |
| AX.25 broadcast | *(fixed)* | Min 600 s between auto-beacon cycles; 180 s band idle |

These are **not** security events. A busy user on a slow link is expected.

## `[security]` — when bans apply

| Target | Event | Default threshold |
|--------|-------|-------------------|
| **IP** | `login_fail` | 5 failures / 10 min |
| **IP** | `link_auth_fail` | 5 failures / 10 min |
| **IP** | `rate_limit` (connection flood) | 30 / 60 s |
| **IP** | `abuse:*` *(hook)* | 30 events / 10 min |
| **CALLID** | `link_auth_fail` (`link_id`) | same as login |
| **CALLID** | `abuse:*` *(hook)* | same as IP abuse |
| **CALLID** | `config` (`ban_callid=`) | immediate, permanent |

**CALLID** = AX.25 callsign (`CALL` or `CALL-SSID`) or HBX `link_id`. Packet-radio RX and circuit `LINK_AUTH` check the ban list before accepting traffic. `iptables`/`nftables` apply to IPs only.

Policy: **short cool-down bans** for dynamic abuse; **permanent** only via `ban_callid` in INI.

INI keys: [MANUAL.md](MANUAL.md#security). Optional `iptables` / `nftables` via `ban_backend`. External fail2ban filters in `share/fail2ban/` remain optional.

## Abuse hook

`hybbx_security_ban_abuse_report(ip, category)` counts toward `abuse_maxretry` / `abuse_findtime`. Plugins and core call it for repeated flood patterns — not for every rejected message.

## Content policy

No automated content classifier in core. Moderator role (`mod`) exists for operator policy. IP/CALLID bans cover network abuse only.

## Inter-node

All HyBBX-to-HyBBX paths use **HBX/Circuit** + `link_password`. See [TOPOLOGY.md](TOPOLOGY.md).
