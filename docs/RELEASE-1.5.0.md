# HyBBX v1.5.0

**Testing release — nearly complete.** Lab-ready without RF hardware; formal GitHub Release after field tests. Mesh and proxymail/proxychat paths remain stubs.

Multi-transport session daemon: **telnet** (`:2323`), **SSH** (`:3232`), **WebSocket** (loopback `:4591`), **HBX circuit hub** (`:7323`). Clients: **hybbx-telnet**, **hybbx-ssh**, **hybbx-terminal**, **hybbx-telnet** (AmigaOS).

**Demo:** [README — hybbx.un1t.me](../README.md#live-hybbxun1tme).

## What's new

| Item | Detail |
|------|--------|
| **Topology** | [TOPOLOGY.md](TOPOLOGY.md) — Main, Secondary, mains-proxy |
| `mains_proxy` | Main-to-Main mesh stub; HBX/Circuit peers only |
| `/proxymail`, `/proxychat` | Inter-Main mail/chat sub-areas (stubs) |
| **HBX/Circuit** | Sole inter-node transport — Secondary, RF, mesh |
| **Built-in security** | `[security]` ban/rate-limit — short cool-down bans |
| **SQLite storage** | Opt-in `[storage] backend = sqlite`; periodic backups |
| AX.25 auto-broadcast | 1200 baud, max 48 chars, min 5 min interval |
| `hybbx-ssh` client | libssh; command history |

## Security

- **Inter-node:** HBX v1 + `link_password` on `:7323` only — no HyBBX-to-HyBBX bypass paths
- **Unified `[security]`:** network abuse + excessive spam — [SECURITY.md](SECURITY.md)
- **Soft limits:** `[traffic]` / `[chat]` / `[mail]` — pace and caps; **no bans** for normal use
- **Bans:** login failures, circuit auth failures, connection flood; `abuse_maxretry` for excessive flood (hook)
- **Policy:** Short `bantime` (default 10 min), repeated cool-downs — not permanent bans
- **Backends:** `internal` (default), `log`, optional `iptables` / `nftables`
- External fail2ban filters in `share/fail2ban/` optional for site-wide firewall integration
- **Content moderation** (non-social / illegal): planned — [ROADMAP.md](ROADMAP.md)

## Transports

| Transport | Detail |
|-----------|--------|
| Telnet | Primary user path `:2323` |
| SSH | libssh; Ed25519 keys in `hostkey_dir` |
| WebSocket | RFC6455 forward-proxy — [WEBSOCKET.md](WEBSOCKET.md) |
| HBX circuit | Internal hub `:7323`; link adapters only |
| Packet radio | Multi-instance TNC edge — [TNCS.md](TNCS.md) |
| mains_proxy | Main-to-Main mesh stub — [MAINS_PROXY.md](MAINS_PROXY.md) |
| BayCom | Opt-in (`HYBBX_PLUGIN_BAYCOM=OFF`) — [BAYCOM.md](BAYCOM.md) |
| ARDOP / CRDOP | External ARDOPC / CRDOPC |

## Status

| Area | Status |
|------|--------|
| Telnet, mail, chat, commands | **Verified** |
| WebSocket, browser UI | **Verified** |
| Built-in security bans | **Built** |
| SQLite storage | **Built** — opt-in |
| `hybbx-telnet` AmigaOS | **Verified** |
| mains_proxy mesh | **Partial** — stub |
| `/proxymail`, `/proxychat` | **Partial** — stub |
| TCP `/broadcast` fan-out | **Partial** — stub |
| HBX, packet_radio, ARDOP, CRDOP | **Built** — live RF TBD |

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix "$HOME"
```

Mains-proxy: `-DHYBBX_PLUGIN_MAINS_PROXY=ON`. Options: [BUILD.md](BUILD.md).

## See also

[RELEASE-NOTES-1.5.0.txt](RELEASE-NOTES-1.5.0.txt) · [TOPOLOGY.md](TOPOLOGY.md) · [FEATURES.md](FEATURES.md) · [MANUAL.md](MANUAL.md) · [ROADMAP.md](ROADMAP.md)
