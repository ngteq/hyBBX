# Security · HyBBX 2.4.0

Built-in `[security]` — network protection and abuse in one subsystem.

## Layer matrix

| Layer | What | Ban? |
|-------|------|------|
| **Soft limits** | RF pacing, message size, traffic shaping | **No** |
| **Abuse** | Brute-force, connection flood, excessive spam | **Yes** — short IP/CALLID cool-down |

## Soft limit matrix

| Area | Keys | Effect |
|------|------|--------|
| `[traffic]` | `baud`, `pace_output`, `line_width` | Output pacing |
| `[chat]` | `message_max` | Truncate oversized lines |
| `[mail]` | `max_messages`, `body_max` | Mailbox caps |
| AX.25 broadcast | fixed | Min 900 s auto cycle; 60 s between links |

## Ban trigger matrix

| Target | Event | Default threshold |
|--------|-------|-------------------|
| **IP** | `login_fail` | 5 / 10 min |
| **IP** | `link_auth_fail` | 5 / 10 min |
| **IP** | `rate_limit` | 30 / 60 s |
| **CALLID** | `link_auth_fail` | same as login |
| **CALLID** | `ban_callid=` config | immediate, permanent |

CALLID = AX.25 callsign or HBX `link_id`. Optional `iptables`/`nftables` via `ban_backend`.

## Related

| Goal | Doc |
|------|-----|
| Manual `[security]` | [MANUAL.md](MANUAL.md) |
| Topology link auth | [TOPOLOGY.md](TOPOLOGY.md) |
