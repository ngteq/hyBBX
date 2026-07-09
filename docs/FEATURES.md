# Feature status

**v1.5.0** (testing) · [RELEASE-1.5.0.md](RELEASE-1.5.0.md) · INI: [MANUAL.md](MANUAL.md)

| Status | Meaning |
|--------|---------|
| **Verified** | Tested in release |
| **Built** | Shipped; verify in your deployment |
| **Partial** | Known limits |
| **Planned** | Not implemented |

## Sessions

| Feature | Status |
|---------|--------|
| Telnet `:2323` | **Verified** |
| Guest + registered auth | **Verified** |
| Mail, chat, conference | **Verified** |
| `/proxymail`, `/proxychat` | **Partial** (stub) — inter-Main via mains_proxy |
| `/` commands | **Verified** |
| Flat-file storage | **Verified** |
| Traffic pacing 2400/80 | **Verified** |
| Texts (`text/*.txt`) | **Verified** |
| Text tokens `%time%`, `%date%` | **Verified** |
| SSH `:3232` | **Built** |
| WebSocket forward-proxy | **Verified** — idle ping keepalive |
| Browser terminal UI | **Verified** — close diagnostics + auto-reconnect |
| `max_connections` (WebSocket) | **Built** |
| One session per registered user | **Built** |
| SSH/telnet/client history | **Built** — last 25 commands, up/down arrows |
| `hybbx-ssh` client | **Built** — libssh, no host-key prompt |
| `hybbx-telnet` AmigaOS client | **Verified** — A1200, AmigaOS 3.2 |
| TCP `/broadcast` | **Partial** |
| AX.25 auto-broadcast (1200 baud) | **Built** — max 48 chars; min 5 min interval |
| SQLite storage | **Built** — opt-in `backend = sqlite` |
| MySQL/MariaDB storage | **Planned** — v2.0.0 |

## Security

| Feature | Status |
|---------|--------|
| `security.log` audit trail | **Built** |
| Unified `[security]` (network + abuse) | **Built** — [SECURITY.md](SECURITY.md) |
| Soft spam limits (chat/mail/traffic) | **Built** — no bans |
| Login brute-force (telnet/ssh/ws) | **Built** |
| Circuit `link_auth` ban | **Built** |
| Per-IP connection flood ban | **Built** |
| CALLID ban (AX.25, `link_id`) | **Built** — `ban_callid`, dynamic abuse |
| `abuse_maxretry` excessive-flood hook | **Built** — call sites TBD |
| Content moderation (illegal/non-social) | **Planned** — [ROADMAP.md](ROADMAP.md) |
| Optional `iptables`/`nftables` backend | **Built** — needs root |
| External fail2ban filters (`share/fail2ban/`) | **Built** — optional adjunct |

## RF / HBX

| Feature | Status |
|---------|--------|
| HBX circuit hub `:7323` | **Built** |
| HBX-only inter-node policy | **Built** — Secondary, RF, mesh |
| Packet radio / AX.25 | **Built** — [TNCS.md](TNCS.md); live RF TBD |
| BayCom PR-Stack (`baycom`) | **Built** (opt-in) — [BAYCOM.md](BAYCOM.md) |
| Main-to-Main proxy (`mains_proxy`) | **Partial** (stub) — [MAINS_PROXY.md](MAINS_PROXY.md) |
| Multi-link hub | **Built** |
| ARDOP / CRDOP plugins | **Built** — [ARDOP.md](ARDOP.md), [CRDOP.md](CRDOP.md) |

## Build

CMake install, unit tests, GitHub Actions CI — **Built**.
